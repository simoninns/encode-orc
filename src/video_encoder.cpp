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
#include <cstdio>

namespace encode_orc {

bool VideoEncoder::encode_test_card(const std::string& output_filename,
                                    VideoSystem system,
                                    TestCardGenerator::Type test_card_type,
                                    int32_t num_frames,
                                    bool verbose,
                                    int32_t picture_start,
                                    int32_t chapter,
                                    const std::string& timecode_start) {
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
                pal_encoder.enable_vits();  // Enable VITS for PAL
                // Pass frame_num as the VBI frame number
                encoded_frame = pal_encoder.encode_frame(test_card, field_number, frame_num);
            } else {
                NTSCEncoder ntsc_encoder(params);
                ntsc_encoder.enable_vits();  // Enable VITS for NTSC
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
        
        // Determine VBI mode based on which parameter is provided
        bool use_cav = (picture_start > 0);
        bool use_clv_chapter = (chapter > 0);
        bool use_clv_timecode = !timecode_start.empty();
        
        // Determine frame rate based on video system
        int32_t fps = (system == VideoSystem::PAL) ? 25 : 30;  // NTSC uses 30fps (simplified from 29.97)
        
        // Parse CLV timecode start value (HH:MM:SS:FF format)
        int32_t timecode_start_frame = 0;
        if (use_clv_timecode) {
            // Parse HH:MM:SS:FF
            int32_t start_hh = 0, start_mm = 0, start_ss = 0, start_ff = 0;
            int parsed = std::sscanf(timecode_start.c_str(), "%d:%d:%d:%d", 
                                      &start_hh, &start_mm, &start_ss, &start_ff);
            if (parsed >= 3) {
                // Convert to frame offset
                timecode_start_frame = start_hh * 3600 * fps +  // Hours to frames
                                       start_mm * 60 * fps +      // Minutes to frames
                                       start_ss * fps +            // Seconds to frames
                                       start_ff;                   // Frame offset
            }
        }
        
        for (int32_t frame_num = 0; frame_num < num_frames; ++frame_num) {
            VBIData vbi;
            
            // Line 16: Programme status code (IEC 60857-1986 - 10.1.8)
            // For CLV, this also encodes seconds and picture number on line 16
            
            if (use_cav) {
                // CAV mode: Programme status code with standard format
                vbi.vbi0 = 0x8BA000;  // CX off, 12", side 1, stereo, correct parity
            
                // CAV mode: Picture numbers
                int32_t picture_number = picture_start + frame_num;
                uint8_t vbi_byte0, vbi_byte1, vbi_byte2;
                BiphaseEncoder::encode_cav_picture_number(picture_number, vbi_byte0, vbi_byte1, vbi_byte2);
                
                int32_t cav_picture_number = (static_cast<int32_t>(vbi_byte0) << 16) |
                                             (static_cast<int32_t>(vbi_byte1) << 8) |
                                             static_cast<int32_t>(vbi_byte2);
                
                vbi.vbi1 = cav_picture_number;
                vbi.vbi2 = cav_picture_number;
            } else if (use_clv_chapter) {
                // CLV mode: Chapter numbers (IEC 60857-1986 10.1.5)
                // Format: 0x800DDD with chapter in bits 12-18 (BCD, 0-79)
                // First digit masked to 0-7 range (top bit marks first 400 tracks)
                int32_t chapter_bcd = ((chapter / 10) << 4) | (chapter % 10);
                int32_t chapter_code = 0x800DDD | ((chapter_bcd & 0x7F) << 12);
                
                vbi.vbi1 = chapter_code;
                vbi.vbi2 = chapter_code;
            } else if (use_clv_timecode) {
                // CLV mode: Programme time code (IEC 60857-1986)
                // Line 16 (vbi0): CLV picture number with seconds
                // Line 17 (vbi1): Hour and minute
                
                // Calculate current timecode based on frame offset
                int32_t total_frame = timecode_start_frame + frame_num;
                int32_t total_seconds = total_frame / fps;
                int32_t frame_in_second = total_frame % fps;
                
                int32_t total_minutes = total_seconds / 60;
                int32_t total_hours = total_minutes / 60;
                
                // Extract time components
                int32_t hh = total_hours % 10;  // Single digit (0-9)
                int32_t mm = total_minutes % 60;
                int32_t ss = total_seconds % 60;
                
                // Encode line 16 (vbi0): CLV picture number format
                // X1 (bits 16-19) encodes seconds tens: 0xA=0-9s, 0xB=10-19s, etc.
                // Bits 8-11 encode seconds units (0-9)
                int32_t sec_tens = ss / 10;       // 0-5
                int32_t sec_units = ss % 10;      // 0-9
                int32_t x1 = 0x0A + sec_tens;     // 0xA-0xF
                
                // BCD encode picture number: high nibble is tens, low nibble is units
                int32_t pic_tens = frame_in_second / 10;
                int32_t pic_units = frame_in_second % 10;
                int32_t pic_bcd = (pic_tens << 4) | pic_units;
                
                // Format: 0x8 in bits 20-23, X1 in 16-19, 0xE in 12-15, 
                //         seconds_units in 8-11, picture_bcd in 0-7
                vbi.vbi0 = (0x8 << 20) | (x1 << 16) | (0xE << 12) | (sec_units << 8) | pic_bcd;
                
                // Encode line 17 (vbi1): Hour and minute
                int32_t hh_bcd = ((hh / 10) << 4) | (hh % 10);
                int32_t mm_bcd = ((mm / 10) << 4) | (mm % 10);
                
                // Format: 0xF0DD00 - hour in bits 16-19, DD=fixed pattern, minute in bits 0-7
                int32_t timecode = 0xF0DD00 | (hh_bcd << 16) | mm_bcd;
                
                vbi.vbi1 = timecode;
                vbi.vbi2 = timecode;
            } else {
                // Default: No frame numbering, just lead-in codes
                vbi.vbi1 = 0x88FFFF;
                vbi.vbi2 = 0x88FFFF;
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
