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
#include "rgb30_loader.h"
#include <iostream>
#include <fstream>
#include <cstdio>

namespace encode_orc {

bool VideoEncoder::encode_rgb30_image(const std::string& output_filename,
                                      VideoSystem system,
                                      const std::string& rgb30_file,
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
        
        // Get expected dimensions for this system
        int32_t img_width, img_height;
        RGB30Loader::get_expected_dimensions(params, img_width, img_height);
        
        if (verbose) {
            std::cout << "Encoding " << num_frames << " frames (" 
                      << (num_frames * 2) << " fields)\n";
            std::cout << "System: " << (system == VideoSystem::PAL ? "PAL" : "NTSC") << "\n";
            std::cout << "Image: " << rgb30_file << " (" << img_width << "x" << img_height << ")\n";
            std::cout << "Field dimensions: " << params.field_width << "x" 
                      << params.field_height << "\n";
        }
        
        // Load RGB30 image
        FrameBuffer image_frame;
        std::string load_error;
        if (!RGB30Loader::load_rgb30(rgb30_file, img_width, img_height, params, 
                                     image_frame, load_error)) {
            error_message_ = load_error;
            return false;
        }
        
        // Open TBC file for writing
        if (verbose) {
            std::cout << "Writing TBC file: " << output_filename << "\n";
        }
        
        std::ofstream tbc_file(output_filename, std::ios::binary);
        if (!tbc_file) {
            error_message_ = "Failed to open output file: " + output_filename;
            return false;
        }
        
        // Encode and write fields (same image for all frames)
        int32_t total_fields = num_frames * 2;
        for (int32_t frame_num = 0; frame_num < num_frames; ++frame_num) {
            int32_t field_number = frame_num * 2;
            Frame encoded_frame;
            
            if (system == VideoSystem::PAL) {
                PALEncoder pal_encoder(params);
                pal_encoder.enable_vits();
                encoded_frame = pal_encoder.encode_frame(image_frame, field_number, frame_num);
            } else {
                NTSCEncoder ntsc_encoder(params);
                ntsc_encoder.enable_vits();
                encoded_frame = ntsc_encoder.encode_frame(image_frame, field_number, frame_num);
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
        metadata.capture_notes = "RGB30 image from " + rgb30_file;
        
        // Add VBI data for each field (frame numbers on lines 16, 17, 18)
        metadata.vbi_data.resize(total_fields);
        
        // Determine VBI mode based on which parameter is provided
        bool use_cav = (picture_start > 0);
        bool use_clv_chapter = (chapter > 0);
        bool use_clv_timecode = !timecode_start.empty();
        
        // Determine frame rate based on video system
        int32_t fps = (system == VideoSystem::PAL) ? 25 : 30;
        
        // Parse CLV timecode start value (HH:MM:SS:FF format)
        int32_t timecode_start_frame = 0;
        if (use_clv_timecode) {
            int32_t start_hh = 0, start_mm = 0, start_ss = 0, start_ff = 0;
            int parsed = std::sscanf(timecode_start.c_str(), "%d:%d:%d:%d", 
                                      &start_hh, &start_mm, &start_ss, &start_ff);
            if (parsed >= 3) {
                timecode_start_frame = start_hh * 3600 * fps +
                                       start_mm * 60 * fps +
                                       start_ss * fps +
                                       start_ff;
            }
        }
        
        for (int32_t frame_num = 0; frame_num < num_frames; ++frame_num) {
            VBIData vbi;
            
            if (use_cav) {
                vbi.vbi0 = 0x8BA000;
                
                int32_t picture_number = picture_start + frame_num;
                uint8_t vbi_byte0, vbi_byte1, vbi_byte2;
                BiphaseEncoder::encode_cav_picture_number(picture_number, vbi_byte0, vbi_byte1, vbi_byte2);
                
                int32_t cav_picture_number = (static_cast<int32_t>(vbi_byte0) << 16) |
                                             (static_cast<int32_t>(vbi_byte1) << 8) |
                                             static_cast<int32_t>(vbi_byte2);
                
                vbi.vbi1 = cav_picture_number;
                vbi.vbi2 = cav_picture_number;
            } else if (use_clv_chapter) {
                int32_t chapter_bcd = ((chapter / 10) << 4) | (chapter % 10);
                int32_t chapter_code = 0x800DDD | ((chapter_bcd & 0x7F) << 12);
                
                vbi.vbi1 = chapter_code;
                vbi.vbi2 = chapter_code;
            } else if (use_clv_timecode) {
                int32_t total_frame = timecode_start_frame + frame_num;
                int32_t total_seconds = total_frame / fps;
                int32_t frame_in_second = total_frame % fps;
                
                int32_t total_minutes = total_seconds / 60;
                int32_t total_hours = total_minutes / 60;
                
                int32_t hh = total_hours % 10;
                int32_t mm = total_minutes % 60;
                int32_t ss = total_seconds % 60;
                
                int32_t sec_tens = ss / 10;
                int32_t sec_units = ss % 10;
                int32_t x1 = 0x0A + sec_tens;
                
                int32_t pic_tens = frame_in_second / 10;
                int32_t pic_units = frame_in_second % 10;
                int32_t pic_bcd = (pic_tens << 4) | pic_units;
                
                vbi.vbi0 = (0x8 << 20) | (x1 << 16) | (0xE << 12) | (sec_units << 8) | pic_bcd;
                
                int32_t hh_bcd = ((hh / 10) << 4) | (hh % 10);
                int32_t mm_bcd = ((mm / 10) << 4) | (mm % 10);
                
                int32_t timecode = 0xF0DD00 | (hh_bcd << 16) | mm_bcd;
                
                vbi.vbi1 = timecode;
                vbi.vbi2 = timecode;
            } else {
                vbi.vbi1 = 0x88FFFF;
                vbi.vbi2 = 0x88FFFF;
            }
            
            metadata.vbi_data[frame_num * 2] = vbi;
            metadata.vbi_data[frame_num * 2 + 1] = vbi;
        }
        
        // Write metadata
        std::string metadata_filename = output_filename + ".db";
        if (verbose) {
            std::cout << "Writing metadata: " << metadata_filename << "\n";
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
        
        if (verbose) {
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
