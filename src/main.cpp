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
#include "test_card_generator.h"
#include "video_parameters.h"
#include <iostream>
#include <fstream>
#include <cstdio>

int main(int argc, char* argv[]) {
    using namespace encode_orc;
    
    // Require exactly one argument - a YAML project file
    if (argc != 2) {
        std::cerr << "Usage: " << argv[0] << " <project.yaml>\n";
        std::cerr << "\nOnly YAML project files are supported.\n";
        return 1;
    }
    
    std::string yaml_file = argv[1];
    
    // Check if it's a YAML file (C++17 compatible)
    bool is_yaml = (yaml_file.length() > 5 && yaml_file.substr(yaml_file.length() - 5) == ".yaml") ||
                   (yaml_file.length() > 4 && yaml_file.substr(yaml_file.length() - 4) == ".yml");
    if (!is_yaml) {
        std::cerr << "Error: File must be a YAML project (.yaml or .yml)\n";
        return 1;
    }
    
    YAMLProjectConfig config;
    std::string error_msg;
    
    if (!parse_yaml_config(yaml_file, config, error_msg)) {
        std::cerr << "Error parsing YAML config: " << error_msg << "\n";
        return 1;
    }
    
    if (!validate_yaml_config(config, error_msg)) {
        std::cerr << "Error validating YAML config: " << error_msg << "\n";
        return 1;
    }
    
    // Process YAML configuration
    std::cout << "encode-orc YAML Project Encoder\n";
    std::cout << "Project: " << config.name << "\n";
    std::cout << "Description: " << config.description << "\n";
    std::cout << "Output: " << config.output.filename << " (" << config.output.format << ")\n";
    std::cout << "\n";
    
    // Determine video system
    VideoSystem system;
    if (config.output.format == "pal-composite" || config.output.format == "pal-yc") {
        system = VideoSystem::PAL;
    } else if (config.output.format == "ntsc-composite" || config.output.format == "ntsc-yc") {
        system = VideoSystem::NTSC;
    } else {
        std::cerr << "Error: Unsupported format " << config.output.format << "\n";
        return 1;
    }
    
    // Calculate total frames and fps
    int32_t total_frames = 0;
    for (const auto& section : config.sections) {
        if (section.duration) {
            total_frames += section.duration.value();
        }
    }
    
    // Display section information
    for (const auto& section : config.sections) {
        std::cout << "Section: " << section.name << "\n";
        if (section.testcard_source) {
            if (!section.testcard_source->pattern.empty()) {
                std::cout << "  Pattern: " << section.testcard_source->pattern << "\n";
            }
        }
        if (section.duration) {
            std::cout << "  Frames: " << section.duration.value() << "\n";
        }
    }
    std::cout << "\nTotal frames: " << total_frames << "\n\n";
    
    // Encode video for each section
    std::ofstream tbc_file(config.output.filename, std::ios::binary);
    if (!tbc_file) {
        std::cerr << "Error: Could not open output file: " << config.output.filename << "\n";
        return 1;
    }
    
    int32_t frame_offset = 0;
    for (const auto& section : config.sections) {
        std::cout << "Encoding section: " << section.name << "\n";
        
        if (section.testcard_source) {
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
            
            VideoEncoder encoder;
            std::string error;
            std::string pattern = section.testcard_source->pattern;
            TestCardGenerator::Type tc_type = pattern_to_testcard_type(pattern, error);
            
            if (!encoder.encode_test_card(config.output.filename + ".temp",
                                         system, tc_type,
                                         section.duration.value(), false,
                                         picture_start, chapter, timecode_start)) {
                std::cerr << "Error: " << encoder.get_error() << "\n";
                return 1;
            }
            
            // Append temp file to main output
            std::ifstream temp_file(config.output.filename + ".temp", std::ios::binary);
            tbc_file << temp_file.rdbuf();
            temp_file.close();
            std::remove((config.output.filename + ".temp").c_str());
            std::remove((config.output.filename + ".temp.db").c_str());
            std::remove((config.output.filename + ".temp.json").c_str());
            
            frame_offset += section.duration.value();
            std::cout << "  ✓ Encoded " << section.duration.value() << " frames\n";
        }
    }
    
    tbc_file.close();
    
    // Generate metadata for entire file using unified generator
    std::string meta_error;
    if (!generate_metadata(config, system, total_frames, 
                          config.output.filename + ".db", meta_error)) {
        std::cerr << "Error: " << meta_error << "\n";
        return 1;
    }
    
    std::cout << "\nSuccessfully generated " << total_frames << " frames to " << config.output.filename << "\n";
    return 0;
}
