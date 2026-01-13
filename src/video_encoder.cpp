/*
 * File:        video_encoder.cpp
 * Module:      encode-orc
 * Purpose:     Main video encoder implementation
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: 2026 Simon Inns
 */

#include "video_encoder.h"
#include "biphase_encoder.h"
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
            Frame encoded_frame;
            
            if (system == VideoSystem::PAL) {
                PALEncoder pal_encoder(params);
                // Pass frame_num as the VBI frame number
                encoded_frame = pal_encoder.encode_frame(test_card, field_number, frame_num);
            } else {
                NTSCEncoder ntsc_encoder(params);
                // Pass frame_num as the VBI frame number
                encoded_frame = ntsc_encoder.encode_frame(test_card, field_number, frame_num);
            }
            
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
        
        // Add VBI data for each field (frame numbers on lines 16, 17, 18)
        metadata.vbi_data.resize(total_fields);
        for (int32_t frame_num = 0; frame_num < num_frames; ++frame_num) {
            VBIData vbi;
            
            // Line 16: Programme status code (IEC 60857-1986 - 10.1.8)
            // Format: 0x8BA000 or 0x8DC000 (CX off/on) + x3x4x5
            // x3: disc size (12"=0/8"=1), side (1=0/2=1), teletext (no=0/yes=1), audio bit
            // x4: audio status bits
            // x5: parity bits
            // Configuration: 12", side 1, no teletext, stereo, CX off
            // x3 = 0x0, x4 = 0x0 (stereo), x5 = 0x0 (parity for all zeros)
            vbi.vbi0 = 0x8BA000;  // CX off, 12", side 1, stereo, correct parity
            
            if (frame_num == 0) {
                // Frame 0: Lead-in code (IEC 60857-1986 - 10.1.1)
                vbi.vbi1 = 0x88FFFF;  // Line 17: Lead-in
                vbi.vbi2 = 0x88FFFF;  // Line 18: Lead-in
            } else {
                // Encode frame number as LaserDisc CAV picture number (24-bit value)
                uint8_t vbi_byte0, vbi_byte1, vbi_byte2;
                BiphaseEncoder::encode_cav_picture_number(frame_num, vbi_byte0, vbi_byte1, vbi_byte2);
                
                // Combine bytes back into 24-bit value
                int32_t cav_picture_number = (static_cast<int32_t>(vbi_byte0) << 16) |
                                             (static_cast<int32_t>(vbi_byte1) << 8) |
                                             static_cast<int32_t>(vbi_byte2);
                
                // vbi1 = line 17 (CAV picture number)
                // vbi2 = line 18 (CAV picture number, redundant)
                vbi.vbi1 = cav_picture_number;   // Line 17: CAV picture number
                vbi.vbi2 = cav_picture_number;   // Line 18: CAV picture number (redundant)
            }
            
            metadata.vbi_data[frame_num * 2] = vbi;      // Field 1
            metadata.vbi_data[frame_num * 2 + 1] = vbi;  // Field 2
        }
        
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
