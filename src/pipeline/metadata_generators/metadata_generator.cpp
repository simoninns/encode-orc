/*
 * File:        metadata_generator.cpp
 * Module:      encode-orc
 * Purpose:     Generate metadata for TBC files
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: 2026 Simon Inns
 */

#ifdef _MSC_VER
#define _CRT_SECURE_NO_WARNINGS
#endif

#include "metadata_generator.h"
#include "metadata_writer.h"
#include "metadata.h"
#include "biphase_encoder.h"
#include <iostream>
#include <cstdio>

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

namespace encode_orc {

bool generate_metadata(const YAMLProjectConfig& config,
                      VideoSystem system,
                      int32_t total_frames,
                      const std::string& output_db,
                       std::string& error_message,
                       CaptureMetadata* output_metadata,
                       const CaptureMetadata* input_metadata) {
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

        // Configure PCM audio metadata if sound is enabled
        if (config.output.sound_format.has_value()) {
            PCMAudioParameters audio_params;
            audio_params.bits = 16;
            audio_params.is_signed = true;
            audio_params.is_little_endian = true;
            audio_params.sample_rate = 44100.0;
            combined.audio_params = audio_params;

            int32_t samples_per_field = (system == VideoSystem::PAL) ? 882 : 735;  // 44.1 kHz / (fields per second)
            for (auto& field : combined.fields) {
                field.audio_samples = samples_per_field;
            }
        }
        
        // Determine if VBI data is needed from pipeline configuration
        bool include_vbi = false;
        if (config.pipeline.metadata.has_value()) {
            for (const auto& gen : config.pipeline.metadata->generators) {
                if ((gen.type == "biphase-vbi" || gen.type == "vitc") && gen.enabled) {
                    include_vbi = true;
                    break;
                }
            }
        }
        
        if (include_vbi) {
            combined.vbi_data.resize(total_fields);
        }
        
        // Generate VBI data for entire file, preserving timecode/chapter continuity
        int32_t frame_num = 0;
        int32_t global_timecode_offset = 0;  // Tracks timecode across sections
        bool has_timecode_mode = false;       // Whether we're in timecode mode at all
        int32_t current_picture_number = 0;   // Tracks picture number across sections
        bool in_picture_mode = false;         // Whether we're generating picture numbers
        
        for (const auto& section : config.sections) {
            int32_t chapter = 0;
            std::string timecode_start = "";
            std::string disc_area = "programme-area";
            
            if (section.biphase_vbi) {
                disc_area = section.biphase_vbi->disc_area;
                
                // If picture_start is explicitly set, update current picture number and enter picture mode
                if (section.biphase_vbi->picture_start) {
                    current_picture_number = section.biphase_vbi->picture_start.value();
                    in_picture_mode = true;
                }
                
                if (section.biphase_vbi->timecode_start) {
                    timecode_start = section.biphase_vbi->timecode_start.value();
                }
                if (section.biphase_vbi->chapter) {
                    chapter = section.biphase_vbi->chapter.value();
                }
            }

            if (disc_area == "programme-area" && chapter == 0) {
                chapter = 1;
            }
            
            // Parse timecode start offset for this section
            if (!timecode_start.empty()) {
                // Explicit timecode specified - update the global offset
                int32_t hh = 0, mm = 0, ss = 0, ff = 0;
                std::sscanf(timecode_start.c_str(), "%d:%d:%d:%d", &hh, &mm, &ss, &ff);
                global_timecode_offset = hh * 3600 * fps + mm * 60 * fps + ss * fps + ff;
                has_timecode_mode = true;
            }
            // If no timecode_start specified but we're in timecode mode, 
            // global_timecode_offset stays as-is (continues from previous section)
            
            // Encode VBI for all frames in this section (only when the standard supports it)
            if (!include_vbi) {
                frame_num += section.duration.value();
                continue;
            }

            for (int32_t section_frame = 0; section_frame < section.duration.value(); ++section_frame) {
                VBIData vbi_field1;  // First field (odd)
                VBIData vbi_field2;  // Second field (even)

                vbi_field1.vbi0 = kNoCode;
                vbi_field1.vbi1 = kNoCode;
                vbi_field1.vbi2 = kNoCode;
                vbi_field2.vbi0 = kNoCode;
                vbi_field2.vbi1 = kNoCode;
                vbi_field2.vbi2 = kNoCode;

                bool use_amendment2 = false;
                std::optional<uint32_t> user_code;
                if (section.biphase_vbi) {
                    use_amendment2 = (section.biphase_vbi->spec == "amendment-2");
                    user_code = section.biphase_vbi->user_code;
                }

                if (disc_area == "lead-in") {
                    if (user_code.has_value()) {
                        vbi_field1.vbi0 = static_cast<int32_t>(user_code.value());
                        vbi_field2.vbi0 = static_cast<int32_t>(user_code.value());
                    }
                    vbi_field1.vbi1 = kLeadIn;
                    vbi_field1.vbi2 = kLeadIn;
                    vbi_field2.vbi1 = kLeadIn;
                    vbi_field2.vbi2 = kLeadIn;
                } else if (disc_area == "lead-out") {
                    if (user_code.has_value()) {
                        vbi_field1.vbi0 = static_cast<int32_t>(user_code.value());
                        vbi_field2.vbi0 = static_cast<int32_t>(user_code.value());
                    }
                    vbi_field1.vbi1 = kLeadOut;
                    vbi_field1.vbi2 = kLeadOut;
                    vbi_field2.vbi1 = kLeadOut;
                    vbi_field2.vbi2 = kLeadOut;
                } else if (in_picture_mode) {
                    // CAV mode - picture number on lines 17/18 of field 1
                    // Use current_picture_number and increment it for each frame
                    int32_t picture_number = current_picture_number + section_frame;
                    uint8_t b0, b1, b2;
                    uint32_t max_picture = (system == VideoSystem::NTSC) ? 79999 : 99999;
                    BiphaseEncoder::encode_cav_picture_number(picture_number, max_picture, b0, b1, b2);
                    int32_t cav = (static_cast<int32_t>(b0) << 16) |
                                 (static_cast<int32_t>(b1) << 8) |
                                 static_cast<int32_t>(b2);

                    vbi_field1.vbi0 = kProgrammeStatusDefault;
                    vbi_field1.vbi1 = cav;
                    vbi_field1.vbi2 = cav;

                    // Picture stop on the following field (lines 16/17)
                    vbi_field2.vbi0 = kPictureStop;
                    vbi_field2.vbi1 = kPictureStop;
                    if (chapter > 0) {
                        vbi_field2.vbi2 = encode_chapter_code(chapter);
                    }
                } else if (has_timecode_mode) {
                    // CLV timecode mode - continuous timecode across entire file on field 1
                    int32_t total_frame = global_timecode_offset + frame_num;
                    int32_t total_seconds_timecode = total_frame / fps;
                    int32_t total_minutes = total_seconds_timecode / 60;
                    int32_t total_hours = total_minutes / 60;

                    int32_t hh = total_hours;
                    int32_t mm = total_minutes % 60;

                    int32_t correction = 0;
                    if (use_amendment2 && system == VideoSystem::NTSC) {
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

                    vbi_field1.vbi0 = clv_pic_number;  // CLV picture number on line 16
                    vbi_field1.vbi1 = timecode;
                    vbi_field1.vbi2 = timecode;

                    vbi_field2.vbi0 = kProgrammeStatusDefault;  // Programme status code on line 16/279
                    vbi_field2.vbi1 = kCLVCode;                 // CLV code on line 17/280
                    vbi_field2.vbi2 = (chapter > 0) ? encode_chapter_code(chapter) : kCLVCode;  // Chapter on line 18/281, or CLV code if no chapter
                } else {
                    // Default - programme area with status only
                    vbi_field1.vbi0 = kProgrammeStatusDefault;
                    vbi_field2.vbi0 = kProgrammeStatusDefault;
                }

                combined.vbi_data[frame_num * 2] = vbi_field1;
                combined.vbi_data[frame_num * 2 + 1] = vbi_field2;
                frame_num++;
            }
            
            // Update current_picture_number for the next section if we're in picture mode
            // and not in lead-in/lead-out
            if (in_picture_mode && disc_area == "programme-area") {
                current_picture_number += section.duration.value();
            }
        }
        
        // Return metadata if requested
        // Merge dropouts from input metadata if provided
        if (input_metadata != nullptr && !input_metadata->dropouts.empty()) {
            for (const auto& dropout : input_metadata->dropouts) {
                combined.dropouts.push_back(dropout);
            }
        }
        
        if (output_metadata != nullptr) {
            *output_metadata = combined;
        }
        
        // Write metadata to database if path provided
        if (output_db.empty()) {
            return true;  // Skip writing if no output path
        }
        
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
