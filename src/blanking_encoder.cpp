/*
 * File:        blanking_encoder.cpp
 * Module:      encode-orc
 * Purpose:     Blanking-level video encoder implementation
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: 2026 Simon Inns
 */

#include "blanking_encoder.h"
#include <iostream>

namespace encode_orc {

Field BlankingEncoder::generate_blanking_field(const VideoParameters& params) {
    Field field(params.field_width, params.field_height);
    
    // Fill entire field with blanking level
    field.fill(static_cast<uint16_t>(params.blanking_16b_ire));
    
    return field;
}

bool BlankingEncoder::encode(const std::string& output_filename,
                              VideoSystem system,
                              int32_t num_frames,
                              bool verbose) {
    // Initialize video parameters
    VideoParameters params;
    if (system == VideoSystem::PAL) {
        params = VideoParameters::create_pal_composite();
    } else if (system == VideoSystem::NTSC) {
        params = VideoParameters::create_ntsc_composite();
    } else {
        error_message_ = "Unsupported video system";
        return false;
    }
    
    int32_t num_fields = num_frames * 2;
    params.number_of_sequential_fields = num_fields;
    
    if (verbose) {
        std::cout << "Encoding " << num_frames << " frames (" << num_fields << " fields)\n";
        std::cout << "System: " << video_system_to_string(system) << "\n";
        std::cout << "Field dimensions: " << params.field_width << "x" << params.field_height << "\n";
        std::cout << "Blanking level: 0x" << std::hex << params.blanking_16b_ire << std::dec << "\n";
    }
    
    // Open TBC file
    TBCWriter tbc_writer;
    std::string tbc_filename = output_filename;
    if (!tbc_writer.open(tbc_filename)) {
        error_message_ = "Failed to open TBC file: " + tbc_filename;
        return false;
    }
    
    if (verbose) {
        std::cout << "Writing TBC file: " << tbc_filename << "\n";
    }
    
    // Initialize metadata
    CaptureMetadata metadata;
    metadata.initialize(system, num_fields);
    metadata.git_branch = "main";
    metadata.git_commit = "v0.1.0-dev";
    metadata.capture_notes = "Blanking-level validation output from encode-orc";
    
    // Generate and write fields
    for (int32_t i = 0; i < num_fields; ++i) {
        if (verbose && (i % 100 == 0)) {
            std::cout << "Writing field " << i << " / " << num_fields << "\r" << std::flush;
        }
        
        // Generate blanking-level field
        Field field = generate_blanking_field(params);
        
        // Write to TBC file
        if (!tbc_writer.write_field(field)) {
            error_message_ = "Failed to write field " + std::to_string(i);
            return false;
        }
    }
    
    if (verbose) {
        std::cout << "Writing field " << num_fields << " / " << num_fields << "\n";
    }
    
    // Close TBC file
    tbc_writer.close();
    
    // Write metadata
    std::string metadata_filename = output_filename + ".db";
    if (verbose) {
        std::cout << "Writing metadata file: " << metadata_filename << "\n";
    }
    
    MetadataWriter metadata_writer;
    if (!metadata_writer.open(metadata_filename)) {
        error_message_ = "Failed to open metadata database: " + metadata_writer.get_error();
        return false;
    }
    
    if (!metadata_writer.write_metadata(metadata)) {
        error_message_ = "Failed to write metadata: " + metadata_writer.get_error();
        return false;
    }
    
    metadata_writer.close();
    
    if (verbose) {
        std::cout << "Encoding complete!\n";
        std::cout << "Output files:\n";
        std::cout << "  " << tbc_filename << "\n";
        std::cout << "  " << metadata_filename << "\n";
    }
    
    return true;
}

} // namespace encode_orc
