/*
 * File:        main.cpp
 * Module:      encode-orc
 * Purpose:     Main application entry point
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: 2026 Simon Inns
 */

#include "yaml_config.h"
#include "video_encoder.h"
#include "metadata_generator.h"
#include "video_parameters.h"
#include "logging.h"
#include "mov_loader.h"
#include "mp4_loader.h"
#include "version.h"
#include <iostream>
#include <fstream>
#include <cstdio>

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
    
    // Parse command-line arguments to extract logging options
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
            
            int32_t total_frames = probe_loader.get_frame_count();
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
            
            int32_t total_frames = probe_loader.get_frame_count();
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
    std::ofstream tbc_file;
    bool is_separate_yc = (config.output.mode == "separate-yc" || config.output.mode == "separate-yc-legacy");
    bool is_yc_legacy = (config.output.mode == "separate-yc-legacy");
    
    // For combined mode, open the single output file
    if (!is_separate_yc) {
        tbc_file.open(config.output.filename, std::ios::binary);
        if (!tbc_file) {
            ENCODE_ORC_LOG_ERROR("Could not open output file: {}", config.output.filename);
            return 1;
        }
    } else {
        // For Y/C mode, delete any pre-existing output files to ensure clean start
        std::string base_out = config.output.filename;
        if (base_out.length() > 4 && base_out.substr(base_out.length() - 4) == ".tbc") {
            base_out = base_out.substr(0, base_out.length() - 4);
        }
        
        if (is_yc_legacy) {
            std::remove((base_out + ".tbc").c_str());
            std::remove((base_out + "_chroma.tbc").c_str());
        } else {
            std::remove((base_out + ".tbcy").c_str());
            std::remove((base_out + ".tbcc").c_str());
        }
    }
    
    // Set video level overrides if specified in YAML
    if (has_video_level_overrides) {
        VideoEncoder::set_video_level_overrides(
            video_levels.value().blanking_16b_ire,
            video_levels.value().black_16b_ire,
            video_levels.value().white_16b_ire
        );
    }
    
    int32_t frame_offset = 0;
    for (const auto& section : config.sections) {
        ENCODE_ORC_LOG_INFO("Encoding section: {}", section.name);
        
        // Track actual number of frames encoded in this section
        int32_t section_frames = 0;
        
        if (section.yuv422_image_source || section.png_image_source || section.mov_file_source || section.mp4_file_source) {
            int32_t picture_start = 0;
            int32_t chapter = 0;
            std::string timecode_start = "";
            
            if (section.laserdisc) {
                if (section.laserdisc->picture_start) {
                    picture_start = section.laserdisc->picture_start.value();
                } else if (section.laserdisc->chapter) {
                    chapter = section.laserdisc->chapter.value();
                } else if (section.laserdisc->timecode_start) {
                    timecode_start = section.laserdisc->timecode_start.value();
                }
            }
            
            // Get filter settings (use defaults if not specified)
            bool enable_chroma_filter = true;  // Default: enabled
            bool enable_luma_filter = false;   // Default: disabled
            
            if (section.filters) {
                enable_chroma_filter = section.filters->chroma.enabled;
                enable_luma_filter = section.filters->luma.enabled;
            }
            
            VideoEncoder encoder;
            bool ok = false;
            if (section.yuv422_image_source) {
                std::string yuv422_file = section.yuv422_image_source->file;
                section_frames = section.duration.value();
                ok = encoder.encode_yuv422_image(config.output.filename + ".temp",
                                                system, config.laserdisc.standard, yuv422_file,
                                                section_frames,
                                                picture_start, chapter, timecode_start,
                                                enable_chroma_filter, enable_luma_filter,
                                                is_separate_yc, is_yc_legacy);
            } else if (section.png_image_source) {
                std::string png_file = section.png_image_source->file;
                section_frames = section.duration.value();
                ok = encoder.encode_png_image(config.output.filename + ".temp",
                                              system, config.laserdisc.standard, png_file,
                                              section_frames,
                                              picture_start, chapter, timecode_start,
                                              enable_chroma_filter, enable_luma_filter,
                                              is_separate_yc, is_yc_legacy);
            } else if (section.mov_file_source) {
                std::string mov_file = section.mov_file_source->file;
                int32_t start_frame = section.mov_file_source->start_frame.value_or(0);
                section_frames = section.duration.value();  // Already populated in preprocessing
                
                ok = encoder.encode_mov_file(config.output.filename + ".temp",
                                            system, config.laserdisc.standard, mov_file,
                                            section_frames, start_frame,
                                            picture_start, chapter, timecode_start,
                                            enable_chroma_filter, enable_luma_filter,
                                            is_separate_yc, is_yc_legacy);
            } else if (section.mp4_file_source) {
                std::string mp4_file = section.mp4_file_source->file;
                int32_t start_frame = section.mp4_file_source->start_frame.value_or(0);
                section_frames = section.duration.value();  // Already populated in preprocessing
                
                ok = encoder.encode_mp4_file(config.output.filename + ".temp",
                                            system, config.laserdisc.standard, mp4_file,
                                            section_frames, start_frame,
                                            picture_start, chapter, timecode_start,
                                            enable_chroma_filter, enable_luma_filter,
                                            is_separate_yc, is_yc_legacy);
            }
            if (!ok) {
                ENCODE_ORC_LOG_ERROR("Encoding error: {}", encoder.get_error());
                return 1;
            }
            
            // Append temp file(s) to main output
            if (is_separate_yc) {
                // For separate Y/C mode, append both luma and chroma files
                // Calculate base filename by removing .tbc extension if present
                std::string base_out = config.output.filename;
                if (base_out.length() > 4 && base_out.substr(base_out.length() - 4) == ".tbc") {
                    base_out = base_out.substr(0, base_out.length() - 4);
                }
                
                if (is_yc_legacy) {
                    // Legacy mode: base.tbc (luma) and base_chroma.tbc (chroma)
                    // Temp files are named: config.output.filename + ".temp.tbc" and ".temp_chroma.tbc"
                    // Final files are named: base_out + ".tbc" and base_out + "_chroma.tbc"
                    
                    // Append Y file (luma)
                    std::ifstream temp_y_file(config.output.filename + ".temp.tbc", std::ios::binary);
                    if (temp_y_file) {
                        std::ios::openmode mode = (frame_offset == 0) ? std::ios::binary : (std::ios::binary | std::ios::app);
                        std::ofstream out_y_file(base_out + ".tbc", mode);
                        out_y_file << temp_y_file.rdbuf();
                        temp_y_file.close();
                        out_y_file.close();
                        std::remove((config.output.filename + ".temp.tbc").c_str());
                    } else {
                        ENCODE_ORC_LOG_WARN("Could not open temp Y file: {}.temp.tbc", config.output.filename);
                    }
                    
                    // Append C file (chroma)
                    std::ifstream temp_c_file(config.output.filename + ".temp_chroma.tbc", std::ios::binary);
                    if (temp_c_file) {
                        std::ios::openmode mode = (frame_offset == 0) ? std::ios::binary : (std::ios::binary | std::ios::app);
                        std::ofstream out_c_file(base_out + "_chroma.tbc", mode);
                        out_c_file << temp_c_file.rdbuf();
                        temp_c_file.close();
                        out_c_file.close();
                        std::remove((config.output.filename + ".temp_chroma.tbc").c_str());
                    } else {
                        ENCODE_ORC_LOG_WARN("Could not open temp C file: {}.temp_chroma.tbc", config.output.filename);
                    }
                } else {
                    // Modern mode: base.tbcy (luma) and base.tbcc (chroma)
                    // Temp files are named: config.output.filename + ".temp.tbcy" and ".temp.tbcc"
                    // Final files are named: base_out + ".tbcy" and base_out + ".tbcc"
                    
                    // Append Y file
                    std::ifstream temp_y_file(config.output.filename + ".temp.tbcy", std::ios::binary);
                    if (temp_y_file) {
                        std::ios::openmode mode = (frame_offset == 0) ? std::ios::binary : (std::ios::binary | std::ios::app);
                        std::ofstream out_y_file(base_out + ".tbcy", mode);
                        out_y_file << temp_y_file.rdbuf();
                        temp_y_file.close();
                        out_y_file.close();
                        std::remove((config.output.filename + ".temp.tbcy").c_str());
                    } else {
                        ENCODE_ORC_LOG_WARN("Could not open temp Y file: {}.temp.tbcy", config.output.filename);
                    }
                    
                    // Append C file
                    std::ifstream temp_c_file(config.output.filename + ".temp.tbcc", std::ios::binary);
                    if (temp_c_file) {
                        std::ios::openmode mode = (frame_offset == 0) ? std::ios::binary : (std::ios::binary | std::ios::app);
                        std::ofstream out_c_file(base_out + ".tbcc", mode);
                        out_c_file << temp_c_file.rdbuf();
                        temp_c_file.close();
                        out_c_file.close();
                        std::remove((config.output.filename + ".temp.tbcc").c_str());
                    } else {
                        ENCODE_ORC_LOG_WARN("Could not open temp C file: {}.temp.tbcc", config.output.filename);
                    }
                }
            } else {
                // For combined mode, append single .tbc file
                std::ifstream temp_file(config.output.filename + ".temp", std::ios::binary);
                if (temp_file) {
                    tbc_file << temp_file.rdbuf();
                    tbc_file.flush();  // Ensure data is written to disk
                    temp_file.close();
                    std::remove((config.output.filename + ".temp").c_str());
                } else {
                    ENCODE_ORC_LOG_WARN("Could not open temp file: {}.temp", config.output.filename);
                }
            }
            
            std::remove((config.output.filename + ".temp.db").c_str());
            std::remove((config.output.filename + ".temp.json").c_str());
            
            frame_offset += section_frames;
            ENCODE_ORC_LOG_INFO("  ✓ Encoded {} frames", section_frames);
        }
    }
    
    if (!is_separate_yc) {
        tbc_file.close();
    }
    
    // Generate metadata for entire file
    std::string meta_error;
    std::string metadata_filename = config.output.filename + ".db";
    
    if (!generate_metadata(config, system, total_frames, metadata_filename, meta_error)) {
        ENCODE_ORC_LOG_ERROR("Metadata generation error: {}", meta_error);
        return 1;
    }
    
    ENCODE_ORC_LOG_INFO("Successfully generated {} frames", total_frames);
    if (is_separate_yc) {
        std::string base_out = config.output.filename;
        if (base_out.length() > 4 && base_out.substr(base_out.length() - 4) == ".tbc") {
            base_out = base_out.substr(0, base_out.length() - 4);
        }
        if (is_yc_legacy) {
            ENCODE_ORC_LOG_INFO("Output files:");
            ENCODE_ORC_LOG_INFO("  {} (luma)", base_out + ".tbc");
            ENCODE_ORC_LOG_INFO("  {} (chroma)", base_out + "_chroma.tbc");
        } else {
            ENCODE_ORC_LOG_INFO("Output files:");
            ENCODE_ORC_LOG_INFO("  {} (luma)", base_out + ".tbcy");
            ENCODE_ORC_LOG_INFO("  {} (chroma)", base_out + ".tbcc");
        }
    } else {
        ENCODE_ORC_LOG_INFO("Output file: {}", config.output.filename);
    }
    return 0;
}
