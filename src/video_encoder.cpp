/*
 * File:        video_encoder.cpp
 * Module:      encode-orc
 * Purpose:     Main video encoder implementation
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: 2026 Simon Inns
 */

#include "video_encoder.h"
#include <iostream>
#include <fstream>

namespace encode_orc {

bool VideoEncoder::encode_test_card(const std::string& output_filename,
                                    VideoSystem system,
                                    TestCardGenerator::Type test_card_type,
                                    int32_t num_frames,
                                    bool verbose) {
    try {
        // Get video parameters for the system
        VideoParameters params;
        if (system == VideoSystem::PAL) {
            params = VideoParameters::create_pal_composite();
        } else {
            params = VideoParameters::create_ntsc_composite();
        }
        
        if (verbose) {
            std::cout << "Encoding " << num_frames << " frames (" 
                      << (num_frames * 2) << " fields)\n";
            std::cout << "System: " << (system == VideoSystem::PAL ? "PAL" : "NTSC") << "\n";
            std::cout << "Field dimensions: " << params.field_width << "x" 
                      << params.field_height << "\n";
            std::cout << "Blanking level: 0x" << std::hex << params.blanking_16b_ire 
                      << std::dec << "\n";
        }
        
        // Generate test card
        FrameBuffer test_card = TestCardGenerator::generate(test_card_type, params);
        
        // Create PAL encoder
        PALEncoder pal_encoder(params);
        
        // Open TBC file for writing
        if (verbose) {
            std::cout << "Writing TBC file: " << output_filename << "\n";
        }
        
        std::ofstream tbc_file(output_filename, std::ios::binary);
        if (!tbc_file) {
            error_message_ = "Failed to open output file: " + output_filename;
            return false;
        }
        
        // Encode and write fields
        int32_t total_fields = num_frames * 2;
        for (int32_t frame_num = 0; frame_num < num_frames; ++frame_num) {
            // Encode the frame (produces 2 fields)
            int32_t field_number = frame_num * 2;
            Frame encoded_frame = pal_encoder.encode_frame(test_card, field_number);
            
            // Write field 1
            const Field& field1 = encoded_frame.field1();
            tbc_file.write(reinterpret_cast<const char*>(field1.data().data()),
                          field1.data().size() * sizeof(uint16_t));
            
            // Write field 2
            const Field& field2 = encoded_frame.field2();
            tbc_file.write(reinterpret_cast<const char*>(field2.data().data()),
                          field2.data().size() * sizeof(uint16_t));
            
            if (verbose && ((frame_num + 1) % 10 == 0 || frame_num == num_frames - 1)) {
                std::cout << "\rWriting field " << ((frame_num + 1) * 2) 
                          << " / " << total_fields << std::flush;
            }
        }
        
        if (verbose) {
            std::cout << "\n";
        }
        
        tbc_file.close();
        
        // Create and initialize metadata
        CaptureMetadata metadata;
        metadata.initialize(system, total_fields);
        metadata.video_params = params;
        metadata.git_branch = "main";
        metadata.git_commit = "v0.1.0-dev";
        metadata.capture_notes = "EBU color bars test pattern from encode-orc";
        
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
            std::cout << "  " << output_filename << "\n";
            std::cout << "  " << output_filename << ".db\n";
        }
        
        return true;
        
    } catch (const std::exception& e) {
        error_message_ = std::string("Exception: ") + e.what();
        return false;
    }
}

} // namespace encode_orc
