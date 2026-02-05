/*
 * File:        vbi_metadata_generator.cpp
 * Module:      encode-orc
 * Purpose:     Generate VBI metadata (vbi0, vbi1, vbi2 bytes) for LaserDisc formats
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: 2026 Simon Inns
 */

#include "vbi_metadata_generator.h"
#include "biphase_encoder.h"

namespace encode_orc {

void VBIMetadataGenerator::generate_frame_vbi(
    int32_t frame_num,
    Mode mode,
    int32_t picture_start,
    int32_t timecode_start_frame,
    int32_t chapter,
    int32_t fps,
    const std::string& disc_area,
    VBIData& field1,
    VBIData& field2
) {
    // Set programme status code based on disc area
    if (disc_area == "lead-in") {
        field1.vbi0 = 0x8BA000;  // Lead-in flag set
        field2.vbi0 = 0x8BA000;
    } else if (disc_area == "lead-out") {
        field1.vbi0 = 0x8F7000;  // Lead-out flag set
        field2.vbi0 = 0x8F7000;
    } else {
        field1.vbi0 = 0x8DC000;  // Programme status code (programme area)
        field2.vbi0 = 0x8DC000;
    }
    
    // Lead-in and lead-out use special codes
    if (disc_area == "lead-in") {
        field1.vbi1 = 0x88FFFF;
        field1.vbi2 = 0x88FFFF;
        field2.vbi1 = 0x88FFFF;
        field2.vbi2 = 0x88FFFF;
        return;
    } else if (disc_area == "lead-out") {
        field1.vbi1 = 0x80EEEE;
        field1.vbi2 = 0x80EEEE;
        field2.vbi1 = 0x80EEEE;
        field2.vbi2 = 0x80EEEE;
        return;
    }
    
    // Programme area - encode picture/timecode/chapter
    if (mode == Mode::CAV) {
        // CAV mode - picture number on field 1
        int32_t picture_number = picture_start + frame_num;
        uint8_t b0, b1, b2;
        BiphaseEncoder::encode_cav_picture_number(picture_number, b0, b1, b2);
        int32_t cav = (static_cast<int32_t>(b0) << 16) |
                     (static_cast<int32_t>(b1) << 8) |
                     static_cast<int32_t>(b2);
        field1.vbi1 = cav;
        field1.vbi2 = cav;
        
        // Field 2 - default or chapter if provided
        if (chapter > 0) {
            int32_t chapter_bcd = ((chapter / 10) << 4) | (chapter % 10);
            int32_t chapter_code = 0x800DDD | ((chapter_bcd & 0x7F) << 12);
            field2.vbi1 = 0x80DD00;
            field2.vbi2 = chapter_code;
        } else {
            field2.vbi1 = 0x80DD00;
            field2.vbi2 = 0x80DD00;
        }
        
    } else if (mode == Mode::CLVTimecode) {
        // CLV timecode mode
        int32_t total_frame = timecode_start_frame + frame_num;
        int32_t total_seconds = total_frame / fps;
        int32_t frame_in_second = total_frame % fps;
        int32_t total_minutes = total_seconds / 60;
        int32_t total_hours = total_minutes / 60;
        
        int32_t hh = total_hours;
        int32_t mm = total_minutes % 60;
        int32_t ss = total_seconds % 60;
        
        int32_t sec_tens = ss / 10;
        int32_t sec_units = ss % 10;
        int32_t x1 = 0x0A + sec_tens;
        
        int32_t pic_tens = frame_in_second / 10;
        int32_t pic_units = frame_in_second % 10;
        int32_t pic_bcd = (pic_tens << 4) | pic_units;

        // CLV picture number (seconds + picture number within second)
        int32_t clv_pic_number = (0x8 << 20) | (x1 << 16) | (0xE << 12) | (sec_units << 8) | pic_bcd;
        
        int32_t hh_bcd = ((hh / 10) << 4) | (hh % 10);
        int32_t mm_bcd = ((mm / 10) << 4) | (mm % 10);
        int32_t timecode = 0xF0DD00 | (hh_bcd << 16) | mm_bcd;
        
        // CLV timecode (hours/minutes) on lines 17/18 of the first field
        field1.vbi0 = clv_pic_number;  // CLV picture number on line 16
        field1.vbi1 = timecode;
        field1.vbi2 = timecode;

        // CLV code and programme status on the other field (no timecode/picture there)
        field2.vbi0 = 0x8DC000;  // Programme status code
        field2.vbi1 = 0x87FFFF;  // CLV code
        
        // Field 2 - chapter if provided (line 18)
        if (chapter > 0) {
            int32_t chapter_bcd = ((chapter / 10) << 4) | (chapter % 10);
            int32_t chapter_code = 0x800DDD | ((chapter_bcd & 0x7F) << 12);
            field2.vbi2 = chapter_code;
        } else {
            field2.vbi2 = 0x80DD00;
        }
        
    } else if (mode == Mode::CLVChapter) {
        // CLV chapter mode
        int32_t chapter_bcd = ((chapter / 10) << 4) | (chapter % 10);
        int32_t chapter_code = 0x800DDD | ((chapter_bcd & 0x7F) << 12);
        
        field1.vbi1 = chapter_code;
        field1.vbi2 = chapter_code;
        field2.vbi1 = 0x80DD00;
        field2.vbi2 = chapter_code;
        
    } else {
        // None - default programme area
        field1.vbi1 = 0x88FFFF;
        field1.vbi2 = 0x88FFFF;
        field2.vbi1 = 0x80DD00;
        field2.vbi2 = 0x80DD00;
    }
}

void VBIMetadataGenerator::generate_leadin_vbi(VBIData& field1, VBIData& field2) {
    field1.vbi0 = 0x8BA000;  // Lead-in flag
    field2.vbi0 = 0x8BA000;
    field1.vbi1 = 0x88FFFF;
    field1.vbi2 = 0x88FFFF;
    field2.vbi1 = 0x88FFFF;
    field2.vbi2 = 0x88FFFF;
}

void VBIMetadataGenerator::generate_leadout_vbi(VBIData& field1, VBIData& field2) {
    field1.vbi0 = 0x8F7000;  // Lead-out flag
    field2.vbi0 = 0x8F7000;
    field1.vbi1 = 0x80EEEE;
    field1.vbi2 = 0x80EEEE;
    field2.vbi1 = 0x80EEEE;
    field2.vbi2 = 0x80EEEE;
}

} // namespace encode_orc
