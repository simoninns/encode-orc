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
        // Check if file exists before attempting to parse
        std::ifstream file_check(filename);
        if (!file_check.good()) {
            error_message = "File not found: " + filename;
            return false;
        }
        file_check.close();
        
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
            if (output["writer"]) {
                config.output.writer = output["writer"].as<std::string>();
            }
            if (output["metadata_decoder"]) {
                config.output.metadata_decoder = output["metadata_decoder"].as<std::string>();
            }
            if (output["sound_format"]) {
                config.output.sound_format = output["sound_format"].as<std::string>();
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
        
        // Parse processing configuration (multi-threading)
        if (root["processing"]) {
            ProcessingConfig proc_cfg;
            YAML::Node proc_node = root["processing"];
            
            if (proc_node["threads"]) {
                // Can be a number or "auto"
                if (proc_node["threads"].IsScalar()) {
                    std::string threads_val = proc_node["threads"].as<std::string>();
                    if (threads_val == "auto") {
                        proc_cfg.threads = 0;  // 0 means auto-detect
                    } else {
                        proc_cfg.threads = proc_node["threads"].as<int32_t>();
                    }
                }
            }
            
            config.processing = proc_cfg;
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
                        
                        // Dropout effect configuration (random only)
                        if (effect_cfg.type == "dropout") {
                            if (effect_node["density"]) {
                                effect_cfg.dropout_density = effect_node["density"].as<double>();
                            }
                            if (effect_node["multi_field_probability"]) {
                                effect_cfg.dropout_multi_field_prob = effect_node["multi_field_probability"].as<double>();
                            }
                            if (effect_node["single_field_probability"]) {
                                effect_cfg.dropout_single_field_prob = effect_node["single_field_probability"].as<double>();
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
                    
                    // Validate source type
                    if (!section.source_type.empty()) {
                        if (section.source_type != "yuv422-image" && 
                            section.source_type != "png-image" && 
                            section.source_type != "mov-file" && 
                            section.source_type != "mp4-file") {
                            error_message = "Invalid source type '" + section.source_type + 
                                          "' in section '" + section.name + "'. " +
                                          "Valid types are: yuv422-image, png-image, mov-file, mp4-file";
                            return false;
                        }
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

                // Parse sound configuration (only one sound field per section)
                if (sec_node["sound"]) {
                    SoundConfig sound_cfg;
                    const YAML::Node& sound_node = sec_node["sound"];
                    
                    if (sound_node["type"]) {
                        sound_cfg.type = sound_node["type"].as<std::string>();
                    }

                    // Parse frequency parameters for waveform types
                    if (sound_cfg.type == "sine" || sound_cfg.type == "square" || sound_cfg.type == "sawtooth") {
                        if (sound_node["start_freq_hz"]) {
                            sound_cfg.start_freq_hz = sound_node["start_freq_hz"].as<double>();
                        }
                        if (sound_node["end_freq_hz"]) {
                            sound_cfg.end_freq_hz = sound_node["end_freq_hz"].as<double>();
                        }
                    } else if (sound_cfg.type == "wav") {
                        if (sound_node["file"]) {
                            sound_cfg.file = sound_node["file"].as<std::string>();
                        }
                    }
                    
                    // Parse optional amplitude (0-100 percent)
                    if (sound_node["amplitude"]) {
                        sound_cfg.amplitude = sound_node["amplitude"].as<double>();
                    }
                    
                    // Parse optional balance (-100 to +100)
                    if (sound_node["balance"]) {
                        sound_cfg.balance = sound_node["balance"].as<double>();
                    }
                    
                    // Parse optional seed for noise types
                    if (sound_node["seed"]) {
                        sound_cfg.seed = sound_node["seed"].as<uint32_t>();
                    }

                    section.sound = sound_cfg;
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

                    if (bv_node["spec"]) {
                        bv.spec = bv_node["spec"].as<std::string>();
                        if (bv.spec == "amendment2") {
                            bv.spec = "amendment-2";
                        }
                    }

                    if (bv_node["user_code"]) {
                        if (bv_node["user_code"].IsScalar()) {
                            std::string user_code_str = bv_node["user_code"].as<std::string>();
                            if (user_code_str.rfind("0x", 0) == 0 || user_code_str.rfind("0X", 0) == 0) {
                                user_code_str = user_code_str.substr(2);
                            }
                            if (user_code_str.size() != 4) {
                                throw std::invalid_argument("user_code must be exactly 4 hex digits");
                            }
                            auto hex_nibble = [](char c) -> uint32_t {
                                if (c >= '0' && c <= '9') return static_cast<uint32_t>(c - '0');
                                if (c >= 'a' && c <= 'f') return static_cast<uint32_t>(c - 'a' + 10);
                                if (c >= 'A' && c <= 'F') return static_cast<uint32_t>(c - 'A' + 10);
                                throw std::invalid_argument("user_code must be hex digits");
                            };
                            uint32_t p3 = hex_nibble(user_code_str[0]);
                            uint32_t p2 = hex_nibble(user_code_str[1]);
                            uint32_t p1 = hex_nibble(user_code_str[2]);
                            uint32_t p0 = hex_nibble(user_code_str[3]);
                            bv.user_code = 0x800000 | (p3 << 16) | (0xD << 12) | (p2 << 8) | (p1 << 4) | p0;
                        } else {
                            throw std::invalid_argument("user_code must be a 4-digit hex string");
                        }
                    }
                    
                    if (bv_node["picture_start"]) {
                        bv.picture_start = bv_node["picture_start"].as<int32_t>();
                    }
                    
                    if (bv_node["picture_stop"]) {
                        bv.picture_stop = bv_node["picture_stop"].as<bool>();
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
        
        // Auto-derive mode from format (mode field is now internal only, not from YAML)
        if (config.output.format == "pal-yc" || config.output.format == "ntsc-yc") {
            config.output.mode = "separate-yc";
        } else {
            config.output.mode = "combined";
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
    
    if (config.output.writer != "tbc" && config.output.writer != "standard") {
        error_message = "Invalid output writer: " + config.output.writer + " (must be 'tbc' or 'standard')";
        return false;
    }

    if (config.output.sound_format.has_value()) {
        const auto& sound_format = config.output.sound_format.value();
        if (sound_format != "pcm" && sound_format != "wav") {
            error_message = "Invalid sound_format: " + sound_format + " (must be 'pcm' or 'wav')";
            return false;
        }
    }
    
    if (config.sections.empty()) {
        error_message = "At least one section is required";
        return false;
    }
    
    // Validate generators: ensure no two generators share the same field line
    if (config.pipeline.metadata) {
        const auto& generators = config.pipeline.metadata->generators;
        std::map<int32_t, std::string> line_to_generator;  // Maps field line to generator type
        
        for (const auto& gen : generators) {
            if (!gen.enabled) {
                continue;  // Skip disabled generators
            }
            
            std::vector<int32_t> gen_lines;
            
            // Extract field lines based on generator type
            if (gen.type == "biphase-vbi" || gen.type == "vitc") {
                // These generators use the 'lines' array
                gen_lines = gen.lines;
            } else if (gen.type == "vits-pal" || gen.type == "vits-ntsc") {
                // VITS generators use 'signals' array - extract line numbers
                for (const auto& signal : gen.vits_signals) {
                    gen_lines.push_back(signal.line);
                }
            }
            // color-burst doesn't use specific field lines
            
            // Check for conflicts with previously registered lines
            for (int32_t line : gen_lines) {
                auto existing = line_to_generator.find(line);
                if (existing != line_to_generator.end()) {
                    error_message = "Field line " + std::to_string(line) + " is used by both '" + 
                                  existing->second + "' and '" + gen.type + "' generators. " +
                                  "Each field line can only be used by one generator.";
                    return false;
                }
                line_to_generator[line] = gen.type;
            }
        }
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

        // Validate sound configuration
        if (section.sound.has_value()) {
            const auto& sound_cfg = section.sound.value();
            if (!config.output.sound_format.has_value()) {
                error_message = "output.sound_format is required when sound is configured (section: " + section.name + ")";
                return false;
            }
            
            // Validate sound type
            const std::vector<std::string> valid_types = {"silence", "source", "sine", "square", "sawtooth", "pink", "white", "brown", "wav"};
            bool valid_type = false;
            for (const auto& vt : valid_types) {
                if (sound_cfg.type == vt) {
                    valid_type = true;
                    break;
                }
            }
            if (!valid_type) {
                error_message = "Invalid sound type '" + sound_cfg.type + "' in section: " + section.name;
                return false;
            }
            
            // Validate waveform types require start frequency
            if (sound_cfg.type == "sine" || sound_cfg.type == "square" || sound_cfg.type == "sawtooth") {
                if (!sound_cfg.start_freq_hz.has_value()) {
                    error_message = "Sound type '" + sound_cfg.type + "' requires start_freq_hz in section: " + section.name;
                    return false;
                }
            }
            
            // Validate amplitude if specified
            if (sound_cfg.amplitude.has_value()) {
                if (sound_cfg.amplitude.value() < 0.0 || sound_cfg.amplitude.value() > 100.0) {
                    error_message = "Sound amplitude must be between 0 and 100 (percent) in section: " + section.name;
                    return false;
                }
            }
            
            // Validate balance if specified
            if (sound_cfg.balance.has_value()) {
                if (sound_cfg.balance.value() < -100.0 || sound_cfg.balance.value() > 100.0) {
                    error_message = "Sound balance must be between -100 (left only) and +100 (right only) in section: " + section.name;
                    return false;
                }
            }
            
            // Validate WAV requires file
            if (sound_cfg.type == "wav") {
                if (!sound_cfg.file.has_value()) {
                    error_message = "Sound type 'wav' requires file in section: " + section.name;
                    return false;
                }
            }
            
            // Validate source type for "source" sound
            if (sound_cfg.type == "source") {
                if (section.source_type != "mov-file" && section.source_type != "mp4-file") {
                    error_message = "Sound type 'source' requires a MOV or MP4 file source in section: " + section.name;
                    return false;
                }
            }
        }
        
        // Validate Biphase VBI picture numbers if specified
        if (section.biphase_vbi) {
            if (section.biphase_vbi->spec != "standard" && section.biphase_vbi->spec != "amendment-2") {
                error_message = "Biphase VBI spec must be 'standard' or 'amendment-2' for section: " + section.name;
                return false;
            }
            if (section.biphase_vbi->user_code) {
                uint32_t code = section.biphase_vbi->user_code.value();
                bool has_key = (code & 0xF00000) == 0x800000;
                bool has_d_nibble = (code & 0x00F000) == 0x00D000;
                bool x1_valid = ((code & 0x0F0000) <= 0x070000);
                if (code > 0xFFFFFF || !has_key || !has_d_nibble || !x1_valid) {
                    error_message = "Biphase VBI user_code must be exactly 4 hex digits (X1X3X4X5) for section: " + section.name;
                    return false;
                }
            }
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
