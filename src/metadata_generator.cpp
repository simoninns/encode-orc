/*
 * File:        metadata_generator.cpp
 * Module:      encode-orc
 * Purpose:     Generate metadata for TBC files
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: 2026 Simon Inns
 */

#include "metadata_generator.h"
#include "metadata_writer.h"
#include "metadata.h"
#include "biphase_encoder.h"
#include <iostream>
#include <cstdio>

namespace encode_orc {

bool generate_metadata(const YAMLProjectConfig& config,
                      VideoSystem system,
                      int32_t total_frames,
                      const std::string& output_db,
                      std::string& error_message) {
    try {
        int32_t total_fields = total_frames * 2;
        int32_t fps = (system == VideoSystem::PAL) ? 25 : 30;
        
        VideoParameters params = (system == VideoSystem::PAL)
            ? VideoParameters::create_pal_composite()
            : VideoParameters::create_ntsc_composite();
        
        // Apply video level overrides if specified in config
        if (config.output.video_levels.has_value()) {
            const auto& vl = config.output.video_levels.value();
            VideoParameters::apply_video_level_overrides(params,
                                                        vl.blanking_16b_ire,
                                                        vl.black_16b_ire,
                                                        vl.white_16b_ire);
        }
        
        // Set decoder string from config
        params.decoder = config.output.metadata_decoder;
        
        CaptureMetadata combined;
        combined.capture_id = 1;
        combined.git_branch = "main";
        combined.git_commit = "v0.1.0-dev";
        combined.capture_notes = config.description;
        combined.initialize(system, total_fields);
        combined.video_params = params;
        combined.video_params.number_of_sequential_fields = total_fields;
        
        const bool include_vbi = standard_supports_vbi(config.laserdisc.standard, system);
        if (include_vbi) {
            combined.vbi_data.resize(total_fields);
        }
        
        // Generate VBI data for entire file, preserving timecode/chapter continuity
        int32_t frame_num = 0;
        for (const auto& section : config.sections) {
            int32_t picture_start = 0;
            int32_t chapter = 0;
            std::string timecode_start = "";
            std::string disc_area = "programme-area";
            
            if (section.laserdisc) {
                disc_area = section.laserdisc->disc_area;
                if (section.laserdisc->picture_start) {
                    picture_start = section.laserdisc->picture_start.value();
                }
                if (section.laserdisc->timecode_start) {
                    timecode_start = section.laserdisc->timecode_start.value();
                }
                if (section.laserdisc->chapter) {
                    chapter = section.laserdisc->chapter.value();
                }
            }
            
            // Parse timecode start offset for this section
            int32_t timecode_offset = 0;
            if (!timecode_start.empty()) {
                int32_t hh = 0, mm = 0, ss = 0, ff = 0;
                std::sscanf(timecode_start.c_str(), "%d:%d:%d:%d", &hh, &mm, &ss, &ff);
                timecode_offset = hh * 3600 * fps + mm * 60 * fps + ss * fps + ff;
            }
            
            // Encode VBI for all frames in this section (only when the standard supports it)
            if (!include_vbi) {
                frame_num += section.duration.value();
                continue;
            }

            for (int32_t section_frame = 0; section_frame < section.duration.value(); ++section_frame) {
                VBIData vbi_field1;  // First field (odd)
                VBIData vbi_field2;  // Second field (even)
                
                // Set programme status code based on disc area
                if (disc_area == "lead-in") {
                    vbi_field1.vbi0 = 0x8BA000;  // Lead-in flag set
                    vbi_field2.vbi0 = 0x8BA000;
                } else if (disc_area == "lead-out") {
                    vbi_field1.vbi0 = 0x8F7000;  // Lead-out flag set
                    vbi_field2.vbi0 = 0x8F7000;
                } else {
                    vbi_field1.vbi0 = 0x87A000;  // Programme area
                    vbi_field2.vbi0 = 0x87A000;
                }
                
                // Lead-in and lead-out use special codes
                if (disc_area == "lead-in") {
                    vbi_field1.vbi1 = 0x88FFFF;
                    vbi_field1.vbi2 = 0x88FFFF;
                    vbi_field2.vbi1 = 0x88FFFF;
                    vbi_field2.vbi2 = 0x88FFFF;
                } else if (disc_area == "lead-out") {
                    vbi_field1.vbi1 = 0x80EEEE;
                    vbi_field1.vbi2 = 0x80EEEE;
                    vbi_field2.vbi1 = 0x80EEEE;
                    vbi_field2.vbi2 = 0x80EEEE;
                } else {
                    // Programme area - encode picture/timecode/chapter
                    // Field 1: Picture number or timecode (line 17/18)
                if (picture_start > 0) {
                    // CAV mode - picture number on line 17/18 of field 1
                    int32_t picture_number = picture_start + frame_num;
                    uint8_t b0, b1, b2;
                    BiphaseEncoder::encode_cav_picture_number(picture_number, b0, b1, b2);
                    int32_t cav = (static_cast<int32_t>(b0) << 16) |
                                 (static_cast<int32_t>(b1) << 8) |
                                 static_cast<int32_t>(b2);
                    vbi_field1.vbi1 = cav;
                    vbi_field1.vbi2 = cav;
                } else if (!timecode_start.empty()) {
                    // CLV timecode mode - continuous timecode across entire file on field 1
                    int32_t total_frame = timecode_offset + section_frame;
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
                    
                    vbi_field1.vbi0 = (0x8 << 20) | (x1 << 16) | (0xE << 12) | (sec_units << 8) | pic_bcd;
                    
                    int32_t hh_bcd = ((hh / 10) << 4) | (hh % 10);
                    int32_t mm_bcd = ((mm / 10) << 4) | (mm % 10);
                    int32_t timecode = 0xF0DD00 | (hh_bcd << 16) | mm_bcd;
                    
                    vbi_field1.vbi1 = timecode;
                    vbi_field1.vbi2 = timecode;
                } else {
                    // Default - no specific numbering on field 1
                    vbi_field1.vbi1 = 0x80DD00;  // Programme area with no picture/timecode
                    vbi_field1.vbi2 = 0x80DD00;
                }
                
                // Field 2: Chapter number (line 18 = vbi2)
                if (chapter > 0) {
                    // Chapter code on line 18 of field 2
                    int32_t chapter_bcd = ((chapter / 10) << 4) | (chapter % 10);
                    int32_t chapter_code = 0x800DDD | ((chapter_bcd & 0x7F) << 12);
                    vbi_field2.vbi1 = 0x80DD00;  // Line 17 unused in field 2 (programme area)
                    vbi_field2.vbi2 = chapter_code;
                } else {
                    // Default - no chapter on field 2
                    vbi_field2.vbi1 = 0x80DD00;  // Programme area
                    vbi_field2.vbi2 = 0x80DD00;
                }
                }
                combined.vbi_data[frame_num * 2] = vbi_field1;
                combined.vbi_data[frame_num * 2 + 1] = vbi_field2;
                frame_num++;
            }
        }
        
        // Write metadata to database
        MetadataWriter writer;
        std::remove(output_db.c_str());
        
        if (!writer.open(output_db)) {
            error_message = "Failed to create metadata database: " + writer.get_error();
            return false;
        }
        if (!writer.write_metadata(combined)) {
            error_message = "Failed to write metadata: " + writer.get_error();
            return false;
        }
        writer.close();
        
        return true;

    } catch (const std::exception& e) {
        error_message = std::string("Exception generating metadata: ") + e.what();
        return false;
    }
}

} // namespace encode_orc
