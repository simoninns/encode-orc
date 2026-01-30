/*
 * File:        yaml_config.cpp
 * Module:      encode-orc
 * Purpose:     YAML project configuration parser implementation using yaml-cpp
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: 2026 Simon Inns
 */

#include "yaml_config.h"
#include <yaml-cpp/yaml.h>
#include <fstream>
#include <iostream>

namespace encode_orc {


bool parse_yaml_config(const std::string& filename, YAMLProjectConfig& config,
                       std::string& error_message) {
    try {
        YAML::Node root = YAML::LoadFile(filename);
        
        // Parse top-level fields
        if (root["name"]) {
            config.name = root["name"].as<std::string>();
        }
        
        if (root["description"]) {
            config.description = root["description"].as<std::string>();
        }
        
        // Parse output configuration
        if (root["output"]) {
            YAML::Node output = root["output"];
            if (output["filename"]) {
                config.output.filename = output["filename"].as<std::string>();
            }
            if (output["format"]) {
                config.output.format = output["format"].as<std::string>();
            }
            if (output["mode"]) {
                config.output.mode = output["mode"].as<std::string>();
            }
            if (output["writer"]) {
                config.output.writer = output["writer"].as<std::string>();
            }
            if (output["metadata_decoder"]) {
                config.output.metadata_decoder = output["metadata_decoder"].as<std::string>();
            }
            
            // Parse optional video levels override
            if (output["video_levels"]) {
                YAML::Node video_levels = output["video_levels"];
                VideoLevelsConfig vlc;
                
                if (video_levels["blanking_16b_ire"]) {
                    vlc.blanking_16b_ire = video_levels["blanking_16b_ire"].as<int32_t>();
                }
                if (video_levels["black_16b_ire"]) {
                    vlc.black_16b_ire = video_levels["black_16b_ire"].as<int32_t>();
                }
                if (video_levels["white_16b_ire"]) {
                    vlc.white_16b_ire = video_levels["white_16b_ire"].as<int32_t>();
                }
                
                config.output.video_levels = vlc;
            }
        }
        
        // Parse pipeline configuration (REQUIRED in Phase 4+)
        if (root["pipeline"]) {
            PipelineConfig pipeline_cfg;
            YAML::Node pipeline_node = root["pipeline"];
            
            // Parse metadata generators
            if (pipeline_node["metadata"]) {
                PipelineMetadataConfig metadata_cfg;
                YAML::Node metadata_node = pipeline_node["metadata"];
                
                if (metadata_node["generators"] && metadata_node["generators"].IsSequence()) {
                    for (const auto& gen_node : metadata_node["generators"]) {
                        PipelineGeneratorConfig gen_cfg;
                        
                        // Required: type
                        if (!gen_node["type"]) {
                            error_message = "Pipeline generator missing required 'type' field";
                            return false;
                        }
                        gen_cfg.type = gen_node["type"].as<std::string>();
                        
                        // Optional: enabled (default true)
                        if (gen_node["enabled"]) {
                            gen_cfg.enabled = gen_node["enabled"].as<bool>();
                        }
                        
                        // Type-specific configuration
                        if (gen_cfg.type == "biphase-vbi") {
                            // Parse lines array
                            if (gen_node["lines"] && gen_node["lines"].IsSequence()) {
                                for (const auto& line_node : gen_node["lines"]) {
                                    gen_cfg.lines.push_back(line_node.as<int32_t>());
                                }
                            }
                            // Parse format (cav or clv)
                            if (gen_node["format"]) {
                                gen_cfg.format = gen_node["format"].as<std::string>();
                            }
                        }
                        
                        if (gen_cfg.type == "vitc") {
                            // Parse lines array
                            if (gen_node["lines"] && gen_node["lines"].IsSequence()) {
                                for (const auto& line_node : gen_node["lines"]) {
                                    gen_cfg.lines.push_back(line_node.as<int32_t>());
                                }
                            }
                            // Parse start_frame_offset
                            if (gen_node["start_frame_offset"]) {
                                gen_cfg.start_frame_offset = gen_node["start_frame_offset"].as<int32_t>();
                            }
                        }
                        
                        if (gen_cfg.type == "vits-pal" || gen_cfg.type == "vits-ntsc") {
                            // Parse VITS signals array
                            if (gen_node["signals"] && gen_node["signals"].IsSequence()) {
                                for (const auto& sig_node : gen_node["signals"]) {
                                    PipelineGeneratorConfig::VITSSignal signal;
                                    
                                    // Line numbers in YAML are 1-indexed and absolute (1-525 for NTSC, 1-625 for PAL)
                                    // Store as-is; conversion happens in main.cpp when creating generators
                                    if (sig_node["line"]) {
                                        signal.line = sig_node["line"].as<int32_t>();
                                    }
                                    if (sig_node["signal"]) {
                                        signal.signal = sig_node["signal"].as<std::string>();
                                    }
                                    
                                    gen_cfg.vits_signals.push_back(signal);
                                }
                            }
                        }
                        
                        metadata_cfg.generators.push_back(gen_cfg);
                    }
                }
                
                pipeline_cfg.metadata = metadata_cfg;
            }
            
            // Parse preprocessing configuration (Phase 6)
            if (pipeline_node["preprocessing"]) {
                PipelinePreprocessingConfig preprocessing_cfg;
                YAML::Node preprocessing_node = pipeline_node["preprocessing"];
                
                // Parse filters
                if (preprocessing_node["filters"]) {
                    FilterConfig filter_cfg;
                    YAML::Node filters_node = preprocessing_node["filters"];
                    
                    // Parse chroma filter
                    if (filters_node["chroma"]) {
                        YAML::Node chroma_node = filters_node["chroma"];
                        if (chroma_node["enabled"]) {
                            filter_cfg.chroma.enabled = chroma_node["enabled"].as<bool>();
                        }
                    }
                    
                    // Parse luma filter
                    if (filters_node["luma"]) {
                        YAML::Node luma_node = filters_node["luma"];
                        if (luma_node["enabled"]) {
                            filter_cfg.luma.enabled = luma_node["enabled"].as<bool>();
                        }
                    }
                    
                    preprocessing_cfg.filters = filter_cfg;
                }
                
                pipeline_cfg.preprocessing = preprocessing_cfg;
            }
            
            // Parse effects configuration (Phase 6)
            if (pipeline_node["effects"]) {
                PipelineEffectsConfig effects_cfg;
                YAML::Node effects_node = pipeline_node["effects"];
                
                if (effects_node.IsSequence()) {
                    for (const auto& effect_node : effects_node) {
                        FieldEffectConfig effect_cfg;
                        
                        // Required: type
                        if (!effect_node["type"]) {
                            error_message = "Field effect missing required 'type' field";
                            return false;
                        }
                        effect_cfg.type = effect_node["type"].as<std::string>();
                        
                        // Optional: enabled (default false for effects)
                        if (effect_node["enabled"]) {
                            effect_cfg.enabled = effect_node["enabled"].as<bool>();
                        }
                        
                        // Noise effect configuration
                        if (effect_cfg.type == "noise") {
                            if (effect_node["snr_db"]) {
                                effect_cfg.snr_db = effect_node["snr_db"].as<double>();
                            }
                            if (effect_node["noise_level_db"]) {
                                effect_cfg.noise_level_db = effect_node["noise_level_db"].as<double>();
                            }
                        }
                        
                        // Dropout effect configuration
                        if (effect_cfg.type == "dropout") {
                            if (effect_node["pattern"]) {
                                effect_cfg.dropout_pattern = effect_node["pattern"].as<std::string>();
                            }
                            if (effect_node["density"]) {
                                effect_cfg.dropout_density = effect_node["density"].as<double>();
                            }
                            if (effect_node["lines"] && effect_node["lines"].IsSequence()) {
                                std::vector<int32_t> dropout_lines;
                                for (const auto& line_node : effect_node["lines"]) {
                                    dropout_lines.push_back(line_node.as<int32_t>());
                                }
                                effect_cfg.dropout_lines = dropout_lines;
                            }
                        }
                        
                        // Phase error effect configuration
                        if (effect_cfg.type == "phase-error") {
                            if (effect_node["phase_jitter_samples"]) {
                                effect_cfg.phase_jitter_samples = effect_node["phase_jitter_samples"].as<double>();
                            }
                            if (effect_node["frequency_hz"]) {
                                effect_cfg.frequency_hz = effect_node["frequency_hz"].as<double>();
                            }
                        }
                        
                        // Common configuration
                        if (effect_node["seed"]) {
                            effect_cfg.seed = effect_node["seed"].as<uint32_t>();
                        }
                        
                        effects_cfg.effects.push_back(effect_cfg);
                    }
                }
                
                pipeline_cfg.effects = effects_cfg;
            }
            
            config.pipeline = pipeline_cfg;
        } else {
            error_message = "Missing required 'pipeline' configuration. Please use the new pipeline.metadata.generators format.";
            return false;
        }
        
        // Parse sections
        if (root["sections"] && root["sections"].IsSequence()) {
            for (const auto& sec_node : root["sections"]) {
                VideoSection section;
                
                if (sec_node["name"]) {
                    section.name = sec_node["name"].as<std::string>();
                }
                
                if (sec_node["duration"]) {
                    section.duration = sec_node["duration"].as<int32_t>();
                }
                
                // Parse source
                if (sec_node["source"]) {
                    YAML::Node source = sec_node["source"];
                    
                    if (source["type"]) {
                        section.source_type = source["type"].as<std::string>();
                    }
                    
                    if (section.source_type == "yuv422-image" && source["file"]) {
                        YUV422ImageSource yuv422;
                        yuv422.file = source["file"].as<std::string>();
                        section.yuv422_image_source = yuv422;
                    }
                    if (section.source_type == "png-image" && source["file"]) {
                        PNGImageSource png;
                        png.file = source["file"].as<std::string>();
                        section.png_image_source = png;
                    }
                    if (section.source_type == "mov-file" && source["file"]) {
                        MOVFileSource mov;
                        mov.file = source["file"].as<std::string>();
                        if (source["start_frame"]) {
                            mov.start_frame = source["start_frame"].as<int32_t>();
                        }
                        section.mov_file_source = mov;
                    }
                    if (section.source_type == "mp4-file" && source["file"]) {
                        MP4FileSource mp4;
                        mp4.file = source["file"].as<std::string>();
                        if (source["start_frame"]) {
                            mp4.start_frame = source["start_frame"].as<int32_t>();
                        }
                        section.mp4_file_source = mp4;
                    }
                }
                
                // Parse filter configuration
                if (sec_node["filters"]) {
                    FilterConfig fc;
                    YAML::Node filters_node = sec_node["filters"];
                    
                    // Parse chroma filter
                    if (filters_node["chroma"]) {
                        YAML::Node chroma_node = filters_node["chroma"];
                        if (chroma_node["enabled"]) {
                            fc.chroma.enabled = chroma_node["enabled"].as<bool>();
                        }
                    }
                    
                    // Parse luma filter
                    if (filters_node["luma"]) {
                        YAML::Node luma_node = filters_node["luma"];
                        if (luma_node["enabled"]) {
                            fc.luma.enabled = luma_node["enabled"].as<bool>();
                        }
                    }
                    
                    section.filters = fc;
                }
                
                // Parse section-level biphase VBI configuration
                // Support both "biphase-vbi:" (new) and "laserdisc:" (legacy)
                YAML::Node bv_node;
                if (sec_node["biphase-vbi"]) {
                    bv_node = sec_node["biphase-vbi"];
                } else if (sec_node["laserdisc"]) {
                    bv_node = sec_node["laserdisc"];
                }
                
                if (bv_node) {
                    BiphaseVBIConfig bv;
                    
                    if (bv_node["disc_area"]) {
                        bv.disc_area = bv_node["disc_area"].as<std::string>();
                    }
                    
                    // Convenience boolean flags
                    if (bv_node["leadin"] && bv_node["leadin"].as<bool>()) {
                        bv.disc_area = "lead-in";
                    }
                    if (bv_node["leadout"] && bv_node["leadout"].as<bool>()) {
                        bv.disc_area = "lead-out";
                    }
                    
                    if (bv_node["picture_start"]) {
                        bv.picture_start = bv_node["picture_start"].as<int32_t>();
                    }
                    
                    if (bv_node["chapter"]) {
                        bv.chapter = bv_node["chapter"].as<int32_t>();
                    }
                    
                    if (bv_node["timecode_start"]) {
                        bv.timecode_start = bv_node["timecode_start"].as<std::string>();
                    }
                    
                    if (bv_node["start"]) {
                        bv.start = bv_node["start"].as<int32_t>();
                    }
                    
                    // Parse VBI configuration
                    if (bv_node["vbi"]) {
                        YAML::Node vbi = bv_node["vbi"];
                        if (vbi["enabled"]) {
                            bv.vbi.enabled = vbi["enabled"].as<bool>();
                        }
                    }
                    
                    // Parse VITS configuration
                    if (bv_node["vits"]) {
                        YAML::Node vits = bv_node["vits"];
                        if (vits["enabled"]) {
                            bv.vits.enabled = vits["enabled"].as<bool>();
                        }
                    }
                    
                    section.biphase_vbi = bv;
                }
                
                config.sections.push_back(section);
            }
        }
        
        return true;
        
    } catch (const YAML::Exception& e) {
        error_message = std::string("YAML parsing error: ") + e.what();
        return false;
    } catch (const std::exception& e) {
        error_message = std::string("Error: ") + e.what();
        return false;
    }
}

bool validate_yaml_config(const YAMLProjectConfig& config, std::string& error_message) {
    if (config.name.empty()) {
        error_message = "Project name is required";
        return false;
    }
    
    if (config.output.filename.empty()) {
        error_message = "Output filename is required";
        return false;
    }
    
    if (config.output.format.empty()) {
        error_message = "Output format is required";
        return false;
    }
    
    if (config.output.format != "pal-composite" && 
        config.output.format != "ntsc-composite" &&
        config.output.format != "pal-yc" && 
        config.output.format != "ntsc-yc") {
        error_message = "Invalid output format: " + config.output.format;
        return false;
    }
    
    if (config.output.mode != "combined" && 
        config.output.mode != "separate-yc" &&
        config.output.mode != "separate-yc-legacy") {
        error_message = "Invalid output mode: " + config.output.mode + " (must be 'combined', 'separate-yc', or 'separate-yc-legacy')";
        return false;
    }
    
    if (config.output.writer != "tbc" && config.output.writer != "standard") {
        error_message = "Invalid output writer: " + config.output.writer + " (must be 'tbc' or 'standard')";
        return false;
    }
    
    if (config.sections.empty()) {
        error_message = "At least one section is required";
        return false;
    }
    
    for (const auto& section : config.sections) {
        if (section.name.empty()) {
            error_message = "Section name is required";
            return false;
        }
        
        if (section.source_type.empty()) {
            error_message = "Section source type is required";
            return false;
        }
        
        if (section.source_type == "yuv422-image") {
            if (!section.yuv422_image_source) {
                error_message = "Raw image source missing for section: " + section.name;
                return false;
            }
            if (!section.duration) {
                error_message = "Duration is required for raw image section: " + section.name;
                return false;
            }
            if (section.duration.value() <= 0) {
                error_message = "Duration must be positive for section: " + section.name;
                return false;
            }
        }
        if (section.source_type == "png-image") {
            if (!section.png_image_source) {
                error_message = "PNG image source missing for section: " + section.name;
                return false;
            }
            if (!section.duration) {
                error_message = "Duration is required for PNG image section: " + section.name;
                return false;
            }
            if (section.duration.value() <= 0) {
                error_message = "Duration must be positive for section: " + section.name;
                return false;
            }
        }
        if (section.source_type == "mov-file") {
            if (!section.mov_file_source) {
                error_message = "MOV file source missing for section: " + section.name;
                return false;
            }
            // Note: duration is optional - if omitted, all frames from start_frame to end will be used
            // However, this requires file probing at runtime
        }
        if (section.source_type == "mp4-file") {
            if (!section.mp4_file_source) {
                error_message = "MP4 file source missing for section: " + section.name;
                return false;
            }
            // Note: duration is optional - if omitted, all frames from start_frame to end will be used
            // However, this requires file probing at runtime
        }
        
        // Validate Biphase VBI picture numbers if specified
        if (section.biphase_vbi) {
            if (section.biphase_vbi->picture_start && section.biphase_vbi->picture_start.value() <= 0) {
                error_message = "Biphase VBI picture_start must be greater than 0 for section: " + section.name;
                return false;
            }
            if (section.biphase_vbi->start && section.biphase_vbi->start.value() <= 0) {
                error_message = "Biphase VBI start picture number must be greater than 0 for section: " + section.name;
                return false;
            }
        }
    }
    
    return true;
}

} // namespace encode_orc
