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
        
        // Parse project-level laserdisc configuration
        if (root["laserdisc"]) {
            YAML::Node laserdisc = root["laserdisc"];
            if (laserdisc["standard"]) {
                config.laserdisc.standard_name = laserdisc["standard"].as<std::string>();
                if (!parse_source_video_standard(config.laserdisc.standard_name, config.laserdisc.standard)) {
                    error_message = "Invalid source video standard: " + config.laserdisc.standard_name + " (expected iec60856-1986, iec60857-1986, consumer-tape, or none)";
                    return false;
                }
            }
            if (laserdisc["mode"]) {
                config.laserdisc.mode = laserdisc["mode"].as<std::string>();
            }
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
                
                // Parse section-level laserdisc configuration
                if (sec_node["laserdisc"]) {
                    LaserDiscConfig ld;
                    YAML::Node ld_node = sec_node["laserdisc"];
                    
                    if (ld_node["disc_area"]) {
                        ld.disc_area = ld_node["disc_area"].as<std::string>();
                    }
                    
                    // Convenience boolean flags
                    if (ld_node["leadin"] && ld_node["leadin"].as<bool>()) {
                        ld.disc_area = "lead-in";
                    }
                    if (ld_node["leadout"] && ld_node["leadout"].as<bool>()) {
                        ld.disc_area = "lead-out";
                    }
                    
                    if (ld_node["picture_start"]) {
                        ld.picture_start = ld_node["picture_start"].as<int32_t>();
                    }
                    
                    if (ld_node["chapter"]) {
                        ld.chapter = ld_node["chapter"].as<int32_t>();
                    }
                    
                    if (ld_node["timecode_start"]) {
                        ld.timecode_start = ld_node["timecode_start"].as<std::string>();
                    }
                    
                    if (ld_node["start"]) {
                        ld.start = ld_node["start"].as<int32_t>();
                    }
                    
                    // Parse VBI configuration
                    if (ld_node["vbi"]) {
                        YAML::Node vbi = ld_node["vbi"];
                        if (vbi["enabled"]) {
                            ld.vbi.enabled = vbi["enabled"].as<bool>();
                        }
                    }
                    
                    // Parse VITS configuration
                    if (ld_node["vits"]) {
                        YAML::Node vits = ld_node["vits"];
                        if (vits["enabled"]) {
                            ld.vits.enabled = vits["enabled"].as<bool>();
                        }
                    }
                    
                    section.laserdisc = ld;
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
    
    // Validate that LaserDisc standard matches video system format
    if (config.laserdisc.standard != SourceVideoStandard::None &&
        config.laserdisc.standard != SourceVideoStandard::ConsumerTape) {
        bool is_pal_format = (config.output.format == "pal-composite" || config.output.format == "pal-yc");
        bool is_ntsc_format = (config.output.format == "ntsc-composite" || config.output.format == "ntsc-yc");
        
        if (config.laserdisc.standard == SourceVideoStandard::IEC60857_1986) {
            // PAL standard
            if (!is_pal_format) {
                error_message = "LaserDisc standard 'iec60857-1986' (PAL) can only be used with PAL output formats (pal-composite or pal-yc), but got '" + config.output.format + "'";
                return false;
            }
        } else if (config.laserdisc.standard == SourceVideoStandard::IEC60856_1986) {
            // NTSC standard
            if (!is_ntsc_format) {
                error_message = "LaserDisc standard 'iec60856-1986' (NTSC) can only be used with NTSC output formats (ntsc-composite or ntsc-yc), but got '" + config.output.format + "'";
                return false;
            }
        }
    }
    
    if (config.output.mode != "combined" && 
        config.output.mode != "separate-yc" &&
        config.output.mode != "separate-yc-legacy") {
        error_message = "Invalid output mode: " + config.output.mode + " (must be 'combined', 'separate-yc', or 'separate-yc-legacy')";
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
        
        // Validate LaserDisc picture numbers if specified
        if (section.laserdisc) {
            if (section.laserdisc->picture_start && section.laserdisc->picture_start.value() <= 0) {
                error_message = "LaserDisc picture_start must be greater than 0 for section: " + section.name;
                return false;
            }
            if (section.laserdisc->start && section.laserdisc->start.value() <= 0) {
                error_message = "LaserDisc start picture number must be greater than 0 for section: " + section.name;
                return false;
            }
        }
    }
    
    return true;
}

} // namespace encode_orc
