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

namespace {
constexpr int32_t kNoCode = 0x80DD00;
constexpr int32_t kLeadIn = 0x88FFFF;
constexpr int32_t kLeadOut = 0x80EEEE;
constexpr int32_t kCLVCode = 0x87FFFF;
constexpr int32_t kPictureStop = 0x82CFFF;
constexpr int32_t kProgrammeStatusDefault = 0x8DC000;

int32_t to_bcd_byte(int32_t value) {
    return ((value / 10) << 4) | (value % 10);
}

int32_t encode_chapter_code(int32_t chapter, bool stop_bit_one = true) {
    // Chapter is encoded as BCD in bits 12-18, with stop bit at bit 19
    // Format: 0x8[stop][tens][units]DDD
    // Example: chapter 42 with stop=1 -> 0x8C2DDD
    int32_t chapter_bcd = to_bcd_byte(chapter);
    int32_t stop_bit = stop_bit_one ? 0x80000 : 0x00000;
    return 0x800DDD | stop_bit | ((chapter_bcd & 0x7F) << 12);
}

int32_t encode_timecode(int32_t hh, int32_t mm) {
    int32_t hh_bcd = to_bcd_byte(hh);
    int32_t mm_bcd = to_bcd_byte(mm);
    return 0xF0DD00 | (hh_bcd << 16) | mm_bcd;
}

int32_t amendment2_ntsc_correction(int32_t frame_index) {
    if (frame_index <= 0) {
        return 0;
    }

    int32_t count = 0;
    int32_t l_max = frame_index / 8991;
    for (int32_t l = 0; l <= l_max; ++l) {
        int32_t remaining = frame_index - (8991 * l);
        int32_t max_m = remaining / 899;
        if (max_m > 9) {
            max_m = 9;
        }
        if (max_m >= 0) {
            count += (max_m + 1);
        }
    }

    // Exclude the N=0 term
    if (count > 0) {
        count -= 1;
    }
    return count;
}
}  // namespace

void VBIMetadataGenerator::generate_frame_vbi(
    int32_t frame_num,
    Mode mode,
    int32_t picture_start,
    int32_t timecode_start_frame,
    int32_t chapter,
    int32_t fps,
    VideoSystem system,
    Spec spec,
    const std::string& disc_area,
    bool picture_stop,
    VBIData& field1,
    VBIData& field2
) {
    // Initialize to "no code"
    field1.vbi0 = kNoCode;
    field1.vbi1 = kNoCode;
    field1.vbi2 = kNoCode;
    field2.vbi0 = kNoCode;
    field2.vbi1 = kNoCode;
    field2.vbi2 = kNoCode;

    if (disc_area == "lead-in") {
        field1.vbi1 = kLeadIn;
        field1.vbi2 = kLeadIn;
        field2.vbi1 = kLeadIn;
        field2.vbi2 = kLeadIn;
        return;
    }

    if (disc_area == "lead-out") {
        field1.vbi1 = kLeadOut;
        field1.vbi2 = kLeadOut;
        field2.vbi1 = kLeadOut;
        field2.vbi2 = kLeadOut;
        return;
    }

    // Programme area - encode picture/timecode/chapter
    if (mode == Mode::CAV) {
        int32_t picture_number = picture_start + frame_num;
        uint8_t b0, b1, b2;
        uint32_t max_picture = (system == VideoSystem::NTSC) ? 79999 : 99999;
        BiphaseEncoder::encode_cav_picture_number(picture_number, max_picture, b0, b1, b2);
        int32_t cav = (static_cast<int32_t>(b0) << 16) |
                     (static_cast<int32_t>(b1) << 8) |
                     static_cast<int32_t>(b2);

        // Field 1: picture number on lines 17/18, programme status on line 16
        field1.vbi0 = kProgrammeStatusDefault;
        field1.vbi1 = cav;
        field1.vbi2 = cav;

        // Field 2: picture stop on lines 16/17 (if enabled), chapter (optional) on line 18
        if (picture_stop) {
            field2.vbi0 = kPictureStop;
            field2.vbi1 = kPictureStop;
        } else {
            field2.vbi0 = kProgrammeStatusDefault;
            field2.vbi1 = kProgrammeStatusDefault;
        }
        if (chapter > 0) {
            field2.vbi2 = encode_chapter_code(chapter);
        }

    } else if (mode == Mode::CLVTimecode) {
        int32_t total_frame = timecode_start_frame + frame_num;
        int32_t total_seconds_timecode = total_frame / fps;
        int32_t total_minutes = total_seconds_timecode / 60;
        int32_t total_hours = total_minutes / 60;

        int32_t hh = total_hours;
        int32_t mm = total_minutes % 60;

        int32_t correction = 0;
        if (spec == Spec::Amendment2 && system == VideoSystem::NTSC) {
            correction = amendment2_ntsc_correction(total_frame);
        }
        int32_t corrected_frame = total_frame + correction;
        int32_t corrected_seconds = corrected_frame / fps;
        int32_t frame_in_second = corrected_frame % fps;

        int32_t sec_tens = (corrected_seconds % 60) / 10;
        int32_t sec_units = (corrected_seconds % 60) % 10;
        int32_t x1 = 0x0A + sec_tens;

        int32_t pic_tens = frame_in_second / 10;
        int32_t pic_units = frame_in_second % 10;
        int32_t pic_bcd = (pic_tens << 4) | pic_units;

        int32_t clv_pic_number = (0x8 << 20) | (x1 << 16) | (0xE << 12) | (sec_units << 8) | pic_bcd;
        int32_t timecode = encode_timecode(hh, mm);

        // Field 1: CLV picture number and programme time code
        field1.vbi0 = clv_pic_number;
        field1.vbi1 = timecode;
        field1.vbi2 = timecode;

        // Field 2: programme status and CLV code, chapter (optional)
        field2.vbi0 = kProgrammeStatusDefault;
        field2.vbi1 = kCLVCode;
        if (chapter > 0) {
            field2.vbi2 = encode_chapter_code(chapter);
        }

    } else if (mode == Mode::CLVChapter) {
        int32_t chapter_code = encode_chapter_code(chapter);
        field1.vbi0 = kProgrammeStatusDefault;
        field1.vbi1 = chapter_code;
        field1.vbi2 = chapter_code;
        field2.vbi0 = kProgrammeStatusDefault;
        field2.vbi1 = chapter_code;
        field2.vbi2 = chapter_code;

    } else {
        // None - programme area with status only
        field1.vbi0 = kProgrammeStatusDefault;
        field2.vbi0 = kProgrammeStatusDefault;
    }
}

void VBIMetadataGenerator::generate_leadin_vbi(VBIData& field1, VBIData& field2) {
    field1.vbi0 = kNoCode;
    field2.vbi0 = kNoCode;
    field1.vbi1 = kLeadIn;
    field1.vbi2 = kLeadIn;
    field2.vbi1 = kLeadIn;
    field2.vbi2 = kLeadIn;
}

void VBIMetadataGenerator::generate_leadout_vbi(VBIData& field1, VBIData& field2) {
    field1.vbi0 = kNoCode;
    field2.vbi0 = kNoCode;
    field1.vbi1 = kLeadOut;
    field1.vbi2 = kLeadOut;
    field2.vbi1 = kLeadOut;
    field2.vbi2 = kLeadOut;
}

} // namespace encode_orc
