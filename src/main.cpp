/*
 * File:        main.cpp
 * Module:      encode-orc
 * Purpose:     Main application entry point
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: 2026 Simon Inns
 */

#include "yaml_config.h"
#include "video_encoder_pipeline.h"
#include "metadata_generator.h"
#include "pipeline_generators.h"
#include "vits_pipeline_generator.h"
#include "video_parameters.h"
#include "metadata.h"
#include "logging.h"
#include "yuv422_loader.h"
#include "png_loader.h"
#include "mov_loader.h"
#include "mp4_loader.h"
#include "writer.h"
#include "tbc_writer.h"
#include "standard_writer.h"
#include "version.h"
#include <iostream>
#include <fstream>
#include <cstdio>
#include <memory>

int main(int argc, char* argv[]) {
    using namespace encode_orc;
    
    // Check for help and version flags first
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--version" || arg == "-v") {
            std::cout << "encode-orc git commit: " << ENCODE_ORC_GIT_COMMIT << "\n";
            std::cout << "Encoder for decode-orc (for making test TBC/Metadata files)\n";
            return 0;
        }
        if (arg == "--help" || arg == "-h") {
            std::cout << "Usage: " << argv[0] << " <project.yaml> [OPTIONS]\n\n";
            std::cout << "Arguments:\n";
            std::cout << "  <project.yaml>          YAML project file to process\n";
            std::cout << "\n";
            std::cout << "Options:\n";
            std::cout << "  -h, --help              Show this help message\n";
            std::cout << "  -v, --version           Show version information\n";
            std::cout << "  --log-level LEVEL       Set logging verbosity\n";
            std::cout << "                          (trace, debug, info, warn, error, critical, off)\n";
            std::cout << "                          Default: info\n";
            std::cout << "  --log-file FILE         Write logs to specified file\n";
            std::cout << "\n";
            std::cout << "Examples:\n";
            std::cout << "  " << argv[0] << " project.yaml\n";
            std::cout << "  " << argv[0] << " project.yaml --log-level debug\n";
            std::cout << "  " << argv[0] << " project.yaml --log-level debug --log-file debug.log\n";
            return 0;
        }
    }
    
    // Parse command-line arguments to extract logging options and other flags
    std::string log_level = "info";
    std::string log_file = "";
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--log-level" && i + 1 < argc) {
            log_level = argv[++i];
        } else if (arg == "--log-file" && i + 1 < argc) {
            log_file = argv[++i];
        }
    }
    
    // Initialize logging system
    init_logging(log_level, "[%Y-%m-%d %H:%M:%S.%e] [%n] [%^%l%$] %v", log_file);
    
    // Require exactly one argument - a YAML project file
    if (argc < 2) {
        ENCODE_ORC_LOG_ERROR("No YAML project file specified");
        ENCODE_ORC_LOG_ERROR("Usage: {} <project.yaml>", argv[0]);
        ENCODE_ORC_LOG_ERROR("       {} --help", argv[0]);
        return 1;
    }
    
    // Find the YAML filename (first non-option argument)
    std::string yaml_file;
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg[0] != '-' && (i + 1 >= argc || argv[i + 1][0] == '-' || 
            (i > 0 && (std::string(argv[i - 1]) == "--log-level" || 
                       std::string(argv[i - 1]) == "--log-file")))) {
            // Check if this is a value for an option
            if (i > 0) {
                std::string prev_arg = argv[i - 1];
                if (prev_arg == "--log-level" || prev_arg == "--log-file") {
                    continue;
                }
            }
            yaml_file = arg;
            break;
        }
    }
    
    if (yaml_file.empty()) {
        ENCODE_ORC_LOG_ERROR("No YAML project file specified");
        return 1;
    }
    
    // Check if it's a YAML file
    bool is_yaml = (yaml_file.length() > 5 && yaml_file.substr(yaml_file.length() - 5) == ".yaml") ||
                   (yaml_file.length() > 4 && yaml_file.substr(yaml_file.length() - 4) == ".yml");
    if (!is_yaml) {
        ENCODE_ORC_LOG_ERROR("File must be a YAML project (.yaml or .yml), got: {}", yaml_file);
        return 1;
    }
    
    YAMLProjectConfig config;
    std::string error_msg;
    
    if (!parse_yaml_config(yaml_file, config, error_msg)) {
        ENCODE_ORC_LOG_ERROR("Error parsing YAML config: {}", error_msg);
        return 1;
    }
    
    if (!validate_yaml_config(config, error_msg)) {
        ENCODE_ORC_LOG_ERROR("Error validating YAML config: {}", error_msg);
        return 1;
    }
    
    // Process YAML configuration
    ENCODE_ORC_LOG_INFO("encode-orc YAML Project Encoder");
    ENCODE_ORC_LOG_INFO("Project: {}", config.name);
    ENCODE_ORC_LOG_INFO("Description: {}", config.description);
    ENCODE_ORC_LOG_INFO("Output: {} ({})", config.output.filename, config.output.format);
    
    // Determine video system
    VideoSystem system;
    if (config.output.format == "pal-composite" || config.output.format == "pal-yc") {
        system = VideoSystem::PAL;
    } else if (config.output.format == "ntsc-composite" || config.output.format == "ntsc-yc") {
        system = VideoSystem::NTSC;
    } else {
        ENCODE_ORC_LOG_ERROR("Unsupported format: {}", config.output.format);
        return 1;
    }
    
    // Store video level overrides for later use in encoding
    const auto& video_levels = config.output.video_levels;
    bool has_video_level_overrides = video_levels.has_value();
    
    if (has_video_level_overrides && video_levels.value().blanking_16b_ire.has_value()) {
        ENCODE_ORC_LOG_INFO("Video level overrides detected");
        if (video_levels.value().blanking_16b_ire.has_value()) {
            ENCODE_ORC_LOG_INFO("  blanking_16b_ire: {}", video_levels.value().blanking_16b_ire.value());
        }
        if (video_levels.value().black_16b_ire.has_value()) {
            ENCODE_ORC_LOG_INFO("  black_16b_ire: {}", video_levels.value().black_16b_ire.value());
        }
        if (video_levels.value().white_16b_ire.has_value()) {
            ENCODE_ORC_LOG_INFO("  white_16b_ire: {}", video_levels.value().white_16b_ire.value());
        }
    }
    
    // Preprocessing: probe MOV files without duration and populate them
    // This ensures all downstream code can rely on duration being set
    for (auto& section : config.sections) {
        if (section.mov_file_source && !section.duration) {
            ENCODE_ORC_LOG_DEBUG("Probing MOV file for section: {}", section.name);
            MOVLoader probe_loader;
            std::string probe_error;
            if (!probe_loader.open(section.mov_file_source->file, probe_error)) {
                ENCODE_ORC_LOG_ERROR("Error probing MOV file for section '{}': {}", section.name, probe_error);
                return 1;
            }
            
            int32_t total_frames = probe_loader.get_metadata().frame_count;
            int32_t start_frame = section.mov_file_source->start_frame.value_or(0);
            probe_loader.close();
            
            if (total_frames <= 0) {
                ENCODE_ORC_LOG_ERROR("Could not determine frame count from MOV file for section '{}'", section.name);
                return 1;
            }
            
            if (start_frame >= total_frames) {
                ENCODE_ORC_LOG_ERROR("start_frame {} is beyond available frames ({}) in section '{}'", 
                                   start_frame, total_frames, section.name);
                return 1;
            }
            
            // Set duration to remaining frames from start_frame
            const_cast<VideoSection&>(section).duration = total_frames - start_frame;
            ENCODE_ORC_LOG_DEBUG("MOV file duration set to {} frames", total_frames - start_frame);
        }
    }
    
    // Preprocessing: probe MP4 files without duration and populate them
    for (auto& section : config.sections) {
        if (section.mp4_file_source && !section.duration) {
            ENCODE_ORC_LOG_DEBUG("Probing MP4 file for section: {}", section.name);
            MP4Loader probe_loader;
            std::string probe_error;
            if (!probe_loader.open(section.mp4_file_source->file, probe_error)) {
                ENCODE_ORC_LOG_ERROR("Error probing MP4 file for section '{}': {}", section.name, probe_error);
                return 1;
            }
            
            int32_t total_frames = probe_loader.get_metadata().frame_count;
            int32_t start_frame = section.mp4_file_source->start_frame.value_or(0);
            probe_loader.close();
            
            if (total_frames <= 0) {
                ENCODE_ORC_LOG_ERROR("Could not determine frame count from MP4 file for section '{}'", section.name);
                return 1;
            }
            
            if (start_frame >= total_frames) {
                ENCODE_ORC_LOG_ERROR("start_frame {} is beyond available frames ({}) in section '{}'", 
                                   start_frame, total_frames, section.name);
                return 1;
            }
            
            // Set duration to remaining frames from start_frame
            const_cast<VideoSection&>(section).duration = total_frames - start_frame;
            ENCODE_ORC_LOG_DEBUG("MP4 file duration set to {} frames", total_frames - start_frame);
        }
    }
    
    // Calculate total frames now that all durations are known
    int32_t total_frames = 0;
    for (const auto& section : config.sections) {
        if (section.duration) {
            total_frames += section.duration.value();
        }
    }
    
    // Display section information
    ENCODE_ORC_LOG_INFO("Sections to encode: {}", config.sections.size());
    for (const auto& section : config.sections) {
        ENCODE_ORC_LOG_INFO("Section: {}", section.name);
        
        if (section.yuv422_image_source) {
            ENCODE_ORC_LOG_INFO("  File: {}", section.yuv422_image_source->file);
        }
        if (section.png_image_source) {
            ENCODE_ORC_LOG_INFO("  File: {}", section.png_image_source->file);
        }
        if (section.mov_file_source) {
            ENCODE_ORC_LOG_INFO("  MOV File: {}", section.mov_file_source->file);
            if (section.mov_file_source->start_frame) {
                ENCODE_ORC_LOG_INFO("  Start Frame: {}", section.mov_file_source->start_frame.value());
            }
        }
        if (section.mp4_file_source) {
            ENCODE_ORC_LOG_INFO("  MP4 File: {}", section.mp4_file_source->file);
            if (section.mp4_file_source->start_frame) {
                ENCODE_ORC_LOG_INFO("  Start Frame: {}", section.mp4_file_source->start_frame.value());
            }
        }
        if (section.duration) {
            ENCODE_ORC_LOG_INFO("  Frames: {}", section.duration.value());
        }
    }
    
    ENCODE_ORC_LOG_INFO("Total frames to encode: {}", total_frames);
    
    // Encode video for each section
    bool is_separate_yc = (config.output.mode == "separate-yc" || config.output.mode == "separate-yc-legacy");
    if (is_separate_yc) {
        ENCODE_ORC_LOG_ERROR("Separate Y/C output is not supported by the pipeline encoder");
        ENCODE_ORC_LOG_ERROR("Please use combined output mode or re-enable legacy encoders");
        return 1;
    }

    // Build video parameters (with optional overrides)
    VideoParameters params = (system == VideoSystem::PAL) ?
        VideoParameters::create_pal_composite() :
        VideoParameters::create_ntsc_composite();

    if (has_video_level_overrides) {
        VideoParameters::apply_video_level_overrides(
            params,
            video_levels.value().blanking_16b_ire,
            video_levels.value().black_16b_ire,
            video_levels.value().white_16b_ire
        );
    }

    // Ensure filename has .tbc extension for composite formats
    std::string output_filename = config.output.filename;
    if (config.output.format == "pal-composite" || config.output.format == "ntsc-composite") {
        // Add .tbc extension if not already present
        if (output_filename.length() < 4 || output_filename.substr(output_filename.length() - 4) != ".tbc") {
            output_filename += ".tbc";
        }
    }

    // Open output writer
    std::unique_ptr<Writer> writer;
    if (config.output.writer == "standard") {
        writer = std::make_unique<StandardWriter>();
    } else {
        writer = std::make_unique<TBCWriter>();
    }

    if (!writer->open(output_filename)) {
        ENCODE_ORC_LOG_ERROR("Could not open output file: {}", output_filename);
        return 1;
    }

    if (auto* tbc = dynamic_cast<TBCWriter*>(writer.get())) {
        int32_t field_height_diff = params.field2_height - params.field1_height;
        tbc->set_field1_padding(params.field_width, static_cast<uint16_t>(params.blanking_16b_ire), field_height_diff);
    }

    // Pre-generate VBI data if required (LaserDisc biphase)
    bool needs_vbi_data = false;
    if (config.pipeline.metadata.has_value()) {
        for (const auto& gen : config.pipeline.metadata->generators) {
            if (gen.type == "biphase-vbi" && gen.enabled) {
                needs_vbi_data = true;
                break;
            }
        }
    }

    CaptureMetadata pre_metadata;
    if (needs_vbi_data) {
        std::string prep_error;
        if (!generate_metadata(config, system, total_frames, "", prep_error, &pre_metadata)) {
            ENCODE_ORC_LOG_ERROR("Metadata preparation error: {}", prep_error);
            return 1;
        }
    }

    int32_t frame_offset = 0;

    for (const auto& section : config.sections) {
        ENCODE_ORC_LOG_INFO("Encoding section: {}", section.name);

        // Track actual number of frames encoded in this section
        int32_t section_frames = 0;

        if (section.yuv422_image_source || section.png_image_source || section.mov_file_source || section.mp4_file_source) {
            // Get filter settings (use defaults if not specified)
            bool enable_chroma_filter = true;  // Default: enabled
            bool enable_luma_filter = false;   // Default: disabled

            if (section.filters) {
                enable_chroma_filter = section.filters->chroma.enabled;
                enable_luma_filter = section.filters->luma.enabled;
            }

            // Instantiate metadata generators from YAML configuration
            std::vector<std::unique_ptr<MetadataGenerator>> generators;
            if (config.pipeline.metadata.has_value()) {
                for (const auto& gen_config : config.pipeline.metadata->generators) {
                    if (!gen_config.enabled) {
                        continue;  // Skip disabled generators
                    }

                    if (gen_config.type == "color-burst") {
                        generators.push_back(std::make_unique<ColorBurstMetadataGenerator>(params));
                        ENCODE_ORC_LOG_DEBUG("Added ColorBurst generator to pipeline");
                    }
                    else if (gen_config.type == "vitc") {
                        // Parse lines if provided, otherwise use defaults
                        std::vector<int32_t> lines;
                        if (!gen_config.lines.empty()) {
                            // Convert from 1-indexed (YAML) to 0-indexed (internal)
                            for (int32_t line_1indexed : gen_config.lines) {
                                lines.push_back(line_1indexed - 1);
                            }
                        }
                        generators.push_back(std::make_unique<VITCMetadataGenerator>(params, lines));
                        ENCODE_ORC_LOG_DEBUG("Added VITC generator to pipeline");
                    }
                    else if (gen_config.type == "vits") {
                        generators.push_back(std::make_unique<VITSMetadataGenerator>(params));
                        ENCODE_ORC_LOG_DEBUG("Added VITS generator to pipeline");
                    }
                    else if (gen_config.type == "vits-pal") {
                        // Parse VITS signal configuration from YAML
                        std::vector<VITSSignalConfig> vits_signals;
                        bool has_errors = false;
                        
                        for (const auto& signal_yaml : gen_config.vits_signals) {
                            VITSSignalConfig signal_cfg;
                            
                            // Line numbers in YAML are 1-indexed absolute
                            // Convert to field number and field-relative 0-indexed line
                            int32_t absolute_line_1indexed = signal_yaml.line;
                            int32_t absolute_line_0indexed = absolute_line_1indexed - 1;
                            int32_t total_lines = params.field1_height + params.field2_height;
                            if (absolute_line_0indexed < 0 || absolute_line_0indexed >= total_lines) {
                                ENCODE_ORC_LOG_ERROR("VITS line {} is out of range for this video system (valid: 1-{})", absolute_line_1indexed, total_lines);
                                has_errors = true;
                                continue;
                            }
                            
                            if (absolute_line_0indexed < params.field1_height) {
                                signal_cfg.field = 1;
                                signal_cfg.line = absolute_line_0indexed;
                            } else {
                                signal_cfg.field = 2;
                                signal_cfg.line = absolute_line_0indexed - params.field1_height;
                            }
                            
                            if (!parse_vits_signal_type(signal_yaml.signal, signal_cfg.signal)) {
                                ENCODE_ORC_LOG_WARN("Unknown VITS signal type '{}', skipping", signal_yaml.signal);
                                continue;
                            }
                            
                            // Validate that only PAL-specific signals are used
                            if (signal_cfg.signal == VITSSignalType::VIR ||
                                signal_cfg.signal == VITSSignalType::NTC7_COMPOSITE ||
                                signal_cfg.signal == VITSSignalType::NTC7_COMBINATION) {
                                ENCODE_ORC_LOG_ERROR("VITS signal '{}' is NTSC-specific and cannot be used in PAL projects", signal_yaml.signal);
                                has_errors = true;
                                continue;
                            }
                            
                            vits_signals.push_back(signal_cfg);
                        }
                        
                        if (has_errors) {
                            ENCODE_ORC_LOG_ERROR("PAL VITS configuration has errors. Use only PAL-specific signals: itu-composite, uk-national, itu-combination, multiburst");
                            return 1;
                        }
                        
                        generators.push_back(std::make_unique<PALVITSMetadataGenerator>(params, vits_signals));
                        ENCODE_ORC_LOG_DEBUG("Added PAL VITS generator to pipeline with {} signals", vits_signals.size());
                    }
                    else if (gen_config.type == "vits-ntsc") {
                        // Parse VITS signal configuration from YAML
                        std::vector<VITSSignalConfig> vits_signals;
                        bool has_errors = false;
                        
                        for (const auto& signal_yaml : gen_config.vits_signals) {
                            VITSSignalConfig signal_cfg;
                            
                            // Line numbers in YAML are 1-indexed absolute
                            // Convert to field number and field-relative 0-indexed line
                            int32_t absolute_line_1indexed = signal_yaml.line;
                            int32_t absolute_line_0indexed = absolute_line_1indexed - 1;
                            int32_t total_lines = params.field1_height + params.field2_height;
                            if (absolute_line_0indexed < 0 || absolute_line_0indexed >= total_lines) {
                                ENCODE_ORC_LOG_ERROR("VITS line {} is out of range for this video system (valid: 1-{})", absolute_line_1indexed, total_lines);
                                has_errors = true;
                                continue;
                            }
                            
                            if (absolute_line_0indexed < params.field1_height) {
                                signal_cfg.field = 1;
                                signal_cfg.line = absolute_line_0indexed;
                            } else {
                                signal_cfg.field = 2;
                                signal_cfg.line = absolute_line_0indexed - params.field1_height;
                            }
                            
                            if (!parse_vits_signal_type(signal_yaml.signal, signal_cfg.signal)) {
                                ENCODE_ORC_LOG_WARN("Unknown VITS signal type '{}', skipping", signal_yaml.signal);
                                continue;
                            }
                            
                            // Validate that only NTSC-specific signals are used
                            if (signal_cfg.signal == VITSSignalType::UK_NATIONAL ||
                                signal_cfg.signal == VITSSignalType::ITU_COMPOSITE ||
                                signal_cfg.signal == VITSSignalType::ITU_ITS ||
                                signal_cfg.signal == VITSSignalType::MULTIBURST) {
                                ENCODE_ORC_LOG_ERROR("VITS signal '{}' is PAL-specific and cannot be used in NTSC projects", signal_yaml.signal);
                                has_errors = true;
                                continue;
                            }
                            
                            vits_signals.push_back(signal_cfg);
                        }
                        
                        if (has_errors) {
                            ENCODE_ORC_LOG_ERROR("NTSC VITS configuration has errors. Use only NTSC-specific signals: vir, ntc7-composite, ntc7-combination");
                            return 1;
                        }
                        
                        generators.push_back(std::make_unique<NTSCVITSMetadataGenerator>(params, vits_signals));
                        ENCODE_ORC_LOG_DEBUG("Added NTSC VITS generator to pipeline with {} signals", vits_signals.size());
                    }
                    else if (gen_config.type == "biphase-vbi") {
                        // Parse lines if provided, otherwise use defaults
                        std::vector<int32_t> lines;
                        if (!gen_config.lines.empty()) {
                            // Convert from 1-indexed (YAML) to 0-indexed (internal)
                            for (int32_t line_1indexed : gen_config.lines) {
                                lines.push_back(line_1indexed - 1);
                            }
                        }
                        generators.push_back(std::make_unique<BiphaseVBIMetadataGenerator>(params, lines));
                        ENCODE_ORC_LOG_DEBUG("Added BiphaseVBI generator to pipeline");
                    }
                    else {
                        ENCODE_ORC_LOG_WARN("Unknown generator type '{}' in pipeline configuration", gen_config.type);
                    }
                }
            }

            // Build pipeline for this section
            VideoEncoderPipeline::Builder pipeline_builder;
            pipeline_builder.set_system(system)
                            .set_parameters(params)
                            .enable_chroma_filter(enable_chroma_filter)
                            .enable_luma_filter(enable_luma_filter);

            if (!generators.empty()) {
                pipeline_builder.set_metadata_generators(std::move(generators));
            }

            auto pipeline = pipeline_builder.build();
            if (!pipeline) {
                ENCODE_ORC_LOG_ERROR("Failed to create pipeline for section '{}'", section.name);
                return 1;
            }

            auto encode_frame = [&](const FrameBuffer& frame_buffer, int32_t section_frame) -> bool {
                int32_t global_frame = frame_offset + section_frame;
                int32_t field_number = global_frame * 2;

                const VBIData* vbi_data = nullptr;
                if (needs_vbi_data && field_number < static_cast<int32_t>(pre_metadata.vbi_data.size()) &&
                    pre_metadata.vbi_data[field_number].has_value()) {
                    vbi_data = &pre_metadata.vbi_data[field_number].value();
                }

                Frame encoded_frame = pipeline->encode_frame(frame_buffer, field_number, vbi_data);

                if (!writer->write_field(encoded_frame.field1()) || !writer->write_field(encoded_frame.field2())) {
                    ENCODE_ORC_LOG_ERROR("Failed to write encoded fields for frame {}", global_frame);
                    return false;
                }

                if ((section_frame + 1) % 10 == 0 || section_frame == section_frames - 1) {
                    ENCODE_ORC_LOG_DEBUG("Writing field {} / {}", (global_frame + 1) * 2, total_frames * 2);
                }

                return true;
            };

            if (section.yuv422_image_source) {
                std::string yuv422_file = section.yuv422_image_source->file;
                section_frames = section.duration.value();

                int32_t img_width = 720;
                int32_t img_height = (system == VideoSystem::PAL) ? 576 : 480;

                YUV422Loader yuv422_loader;
                if (!yuv422_loader.open(yuv422_file, img_width, img_height)) {
                    ENCODE_ORC_LOG_ERROR("Failed to open YUV422 file: {}", yuv422_file);
                    return 1;
                }

                FrameBuffer frame_buffer;
                std::string load_error;
                if (!yuv422_loader.load_frame(0, frame_buffer, load_error)) {
                    ENCODE_ORC_LOG_ERROR("Failed to load YUV422 frame: {}", load_error);
                    yuv422_loader.close();
                    return 1;
                }
                yuv422_loader.close();

                for (int32_t i = 0; i < section_frames; ++i) {
                    if (!encode_frame(frame_buffer, i)) {
                        return 1;
                    }
                }
            } else if (section.png_image_source) {
                std::string png_file = section.png_image_source->file;
                section_frames = section.duration.value();

                PNGLoader png_loader;
                std::string load_error;
                if (!png_loader.open(png_file, load_error)) {
                    ENCODE_ORC_LOG_ERROR("Failed to open PNG file: {}", load_error);
                    return 1;
                }

                FrameBuffer frame_buffer;
                if (!png_loader.load_frame(0, frame_buffer, load_error)) {
                    ENCODE_ORC_LOG_ERROR("Failed to load PNG frame: {}", load_error);
                    png_loader.close();
                    return 1;
                }
                png_loader.close();

                for (int32_t i = 0; i < section_frames; ++i) {
                    if (!encode_frame(frame_buffer, i)) {
                        return 1;
                    }
                }
            } else if (section.mov_file_source) {
                std::string mov_file = section.mov_file_source->file;
                int32_t start_frame = section.mov_file_source->start_frame.value_or(0);
                section_frames = section.duration.value();

                MOVLoader mov_loader;
                std::string load_error;
                if (!mov_loader.open(mov_file, load_error)) {
                    ENCODE_ORC_LOG_ERROR("Failed to open MOV file: {}", load_error);
                    return 1;
                }

                for (int32_t i = 0; i < section_frames; ++i) {
                    FrameBuffer frame_buffer;
                    if (!mov_loader.load_frame(start_frame + i, frame_buffer, load_error)) {
                        ENCODE_ORC_LOG_ERROR("Failed to load MOV frame {}: {}", start_frame + i, load_error);
                        mov_loader.close();
                        return 1;
                    }

                    if (!encode_frame(frame_buffer, i)) {
                        mov_loader.close();
                        return 1;
                    }
                }
                mov_loader.close();
            } else if (section.mp4_file_source) {
                std::string mp4_file = section.mp4_file_source->file;
                int32_t start_frame = section.mp4_file_source->start_frame.value_or(0);
                section_frames = section.duration.value();

                MP4Loader mp4_loader;
                std::string load_error;
                if (!mp4_loader.open(mp4_file, load_error)) {
                    ENCODE_ORC_LOG_ERROR("Failed to open MP4 file: {}", load_error);
                    return 1;
                }

                for (int32_t i = 0; i < section_frames; ++i) {
                    FrameBuffer frame_buffer;
                    if (!mp4_loader.load_frame(start_frame + i, frame_buffer, load_error)) {
                        ENCODE_ORC_LOG_ERROR("Failed to load MP4 frame {}: {}", start_frame + i, load_error);
                        mp4_loader.close();
                        return 1;
                    }

                    if (!encode_frame(frame_buffer, i)) {
                        mp4_loader.close();
                        return 1;
                    }
                }
                mp4_loader.close();
            }

            frame_offset += section_frames;
            ENCODE_ORC_LOG_INFO("  ✓ Encoded {} frames", section_frames);
        }
    }

    writer->close();
    
    // Generate metadata for entire file (only for TBC writer, not standard writer)
    if (config.output.writer != "standard") {
        std::string meta_error;
        std::string metadata_filename = output_filename + ".db";
        
        if (!generate_metadata(config, system, total_frames, metadata_filename, meta_error)) {
            ENCODE_ORC_LOG_ERROR("Metadata generation error: {}", meta_error);
            return 1;
        }
    }
    
    ENCODE_ORC_LOG_INFO("Successfully generated {} frames", total_frames);
    ENCODE_ORC_LOG_INFO("Output file: {}", output_filename);
    return 0;
}
