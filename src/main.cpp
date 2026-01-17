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
        if (section.yuv422_image_source) {
            std::cout << "  File: " << section.yuv422_image_source->file << "\n";
        }
        if (section.png_image_source) {
            std::cout << "  File: " << section.png_image_source->file << "\n";
        }
        if (section.duration) {
            std::cout << "  Frames: " << section.duration.value() << "\n";
        }
    }
    std::cout << "\nTotal frames: " << total_frames << "\n\n";
    
    // Encode video for each section
    std::ofstream tbc_file;
    bool is_separate_yc = (config.output.mode == "separate-yc" || config.output.mode == "separate-yc-legacy");
    bool is_yc_legacy = (config.output.mode == "separate-yc-legacy");
    
    // For combined mode, open the single output file
    if (!is_separate_yc) {
        tbc_file.open(config.output.filename, std::ios::binary);
        if (!tbc_file) {
            std::cerr << "Error: Could not open output file: " << config.output.filename << "\n";
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
    
    int32_t frame_offset = 0;
    for (const auto& section : config.sections) {
        std::cout << "Encoding section: " << section.name << "\n";
        
        if (section.yuv422_image_source || section.png_image_source) {
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
                ok = encoder.encode_yuv422_image(config.output.filename + ".temp",
                                                system, config.laserdisc.standard, yuv422_file,
                                                section.duration.value(), false,
                                                picture_start, chapter, timecode_start,
                                                enable_chroma_filter, enable_luma_filter,
                                                is_separate_yc, is_yc_legacy);
            } else if (section.png_image_source) {
                std::string png_file = section.png_image_source->file;
                ok = encoder.encode_png_image(config.output.filename + ".temp",
                                              system, config.laserdisc.standard, png_file,
                                              section.duration.value(), false,
                                              picture_start, chapter, timecode_start,
                                              enable_chroma_filter, enable_luma_filter,
                                              is_separate_yc, is_yc_legacy);
            }
            if (!ok) {
                std::cerr << "Error: " << encoder.get_error() << "\n";
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
                        std::cerr << "Warning: Could not open temp Y file: " << config.output.filename << ".temp.tbc\n";
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
                        std::cerr << "Warning: Could not open temp C file: " << config.output.filename << ".temp_chroma.tbc\n";
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
                        std::cerr << "Warning: Could not open temp Y file: " << config.output.filename << ".temp.tbcy\n";
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
                        std::cerr << "Warning: Could not open temp C file: " << config.output.filename << ".temp.tbcc\n";
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
                    std::cerr << "Warning: Could not open temp file: " << config.output.filename << ".temp\n";
                }
            }
            
            std::remove((config.output.filename + ".temp.db").c_str());
            std::remove((config.output.filename + ".temp.json").c_str());
            
            frame_offset += section.duration.value();
            std::cout << "  ✓ Encoded " << section.duration.value() << " frames\n";
        }
    }
    
    if (!is_separate_yc) {
        tbc_file.close();
    }
    
    // Generate metadata for entire file
    std::string meta_error;
    std::string metadata_filename = config.output.filename + ".db";
    
    if (!generate_metadata(config, system, total_frames, metadata_filename, meta_error)) {
        std::cerr << "Error: " << meta_error << "\n";
        return 1;
    }
    
    if (is_separate_yc) {
        std::string base_out = config.output.filename;
        if (base_out.length() > 4 && base_out.substr(base_out.length() - 4) == ".tbc") {
            base_out = base_out.substr(0, base_out.length() - 4);
        }
        std::cout << "\nSuccessfully generated " << total_frames << " frames to:\n";
        if (is_yc_legacy) {
            std::cout << "  " << base_out << ".tbc (luma)\n";
            std::cout << "  " << base_out << "_chroma.tbc (chroma)\n";
        } else {
            std::cout << "  " << base_out << ".tbcy (luma)\n";
            std::cout << "  " << base_out << ".tbcc (chroma)\n";
        }
    } else {
        std::cout << "\nSuccessfully generated " << total_frames << " frames to " << config.output.filename << "\n";
    }
    return 0;
}
