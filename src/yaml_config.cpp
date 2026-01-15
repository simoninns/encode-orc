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

TestCardGenerator::Type pattern_to_testcard_type(const std::string& pattern, std::string& error) {
    std::string pat = pattern;
    std::transform(pat.begin(), pat.end(), pat.begin(), ::tolower);
    
    if (pat == "color-bars" || pat == "ebu" || pat == "eia" || pat == "smpte") {
        return TestCardGenerator::Type::COLOR_BARS;
    } else if (pat == "pm5544") {
        return TestCardGenerator::Type::PM5544;
    } else if (pat == "testcard-f") {
        return TestCardGenerator::Type::TESTCARD_F;
    }
    
    error = "Unknown test card pattern: " + pattern;
    return TestCardGenerator::Type::COLOR_BARS;
}

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
        }
        
        // Parse project-level laserdisc configuration
        if (root["laserdisc"]) {
            YAML::Node laserdisc = root["laserdisc"];
            if (laserdisc["standard"]) {
                config.laserdisc.standard = laserdisc["standard"].as<std::string>();
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
                    
                    if (section.source_type == "testcard" && source["pattern"]) {
                        TestCardSource tc;
                        tc.pattern = source["pattern"].as<std::string>();
                        section.testcard_source = tc;
                    } else if (section.source_type == "rgb-file") {
                        RGBFileSource rgb;
                        if (source["path"]) {
                            rgb.path = source["path"].as<std::string>();
                        }
                        if (source["width"]) {
                            rgb.width = source["width"].as<int32_t>();
                        }
                        if (source["height"]) {
                            rgb.height = source["height"].as<int32_t>();
                        }
                        if (source["frame_start"]) {
                            rgb.frame_start = source["frame_start"].as<int32_t>();
                        }
                        if (source["frame_end"]) {
                            rgb.frame_end = source["frame_end"].as<int32_t>();
                        }
                        section.rgb_source = rgb;
                    }
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
        
        if (section.source_type == "testcard") {
            if (!section.testcard_source) {
                error_message = "Test card source missing for section: " + section.name;
                return false;
            }
            if (!section.duration) {
                error_message = "Duration is required for test card section: " + section.name;
                return false;
            }
            if (section.duration.value() <= 0) {
                error_message = "Duration must be positive for section: " + section.name;
                return false;
            }
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
