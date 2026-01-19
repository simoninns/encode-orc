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
#include "yuv422_loader.h"
#include "png_loader.h"
#include "mov_loader.h"
#include "mp4_loader.h"
#include "yc_tbc_writer.h"
#include "logging.h"
#include <iostream>
#include <fstream>
#include <cstdio>

namespace encode_orc {

// Initialize static member variables
std::optional<int32_t> VideoEncoder::s_blanking_16b_ire_override = std::nullopt;
std::optional<int32_t> VideoEncoder::s_black_16b_ire_override = std::nullopt;
std::optional<int32_t> VideoEncoder::s_white_16b_ire_override = std::nullopt;

void VideoEncoder::set_video_level_overrides(std::optional<int32_t> blanking_16b_ire,
                                             std::optional<int32_t> black_16b_ire,
                                             std::optional<int32_t> white_16b_ire) {
    s_blanking_16b_ire_override = blanking_16b_ire;
    s_black_16b_ire_override = black_16b_ire;
    s_white_16b_ire_override = white_16b_ire;
}

void VideoEncoder::clear_video_level_overrides() {
    s_blanking_16b_ire_override = std::nullopt;
    s_black_16b_ire_override = std::nullopt;
    s_white_16b_ire_override = std::nullopt;
}

bool VideoEncoder::encode_yuv422_image(const std::string& output_filename,
                                       VideoSystem system,
                                       LaserDiscStandard ld_standard,
                                       const std::string& yuv422_file,
                                       int32_t num_frames,
                                       int32_t picture_start,
                                       int32_t chapter,
                                       const std::string& timecode_start,
                                       bool enable_chroma_filter,
                                       bool enable_luma_filter,
                                       bool separate_yc,
                                       bool yc_legacy) {
    try {
        // Get video parameters for the system
        VideoParameters params;
        if (system == VideoSystem::PAL) {
            params = VideoParameters::create_pal_composite();
        } else {
            params = VideoParameters::create_ntsc_composite();
        }
        
        // Apply any video level overrides
        VideoParameters::apply_video_level_overrides(params,
                                                     s_blanking_16b_ire_override,
                                                     s_black_16b_ire_override,
                                                     s_white_16b_ire_override);
        
        if (s_blanking_16b_ire_override.has_value() || 
            s_black_16b_ire_override.has_value() || 
            s_white_16b_ire_override.has_value()) {
            ENCODE_ORC_LOG_DEBUG("Applied video level overrides to params:");
            ENCODE_ORC_LOG_DEBUG("  blanking: {}", params.blanking_16b_ire);
            ENCODE_ORC_LOG_DEBUG("  black: {}", params.black_16b_ire);
            ENCODE_ORC_LOG_DEBUG("  white: {}", params.white_16b_ire);
        }
        
        // Get expected dimensions for this system
        int32_t img_width = 720, img_height = (system == VideoSystem::PAL) ? 576 : 480;
        
        // Load Y'CbCr 4:2:2 raw image
        YUV422Loader yuv422_loader;
        if (!yuv422_loader.open(yuv422_file, img_width, img_height)) {
            error_message_ = "Failed to open YUV422 file: " + yuv422_file;
            return false;
        }
        
        FrameBuffer image_frame;
        if (!yuv422_loader.load_frame(0, image_frame)) {
            error_message_ = "Failed to load YUV422 frame";
            yuv422_loader.close();
            return false;
        }
        
        ENCODE_ORC_LOG_DEBUG("Encoding {} frames ({} fields)", num_frames, num_frames * 2);
        ENCODE_ORC_LOG_DEBUG("System: {}", (system == VideoSystem::PAL ? "PAL" : "NTSC"));
        ENCODE_ORC_LOG_DEBUG("Image: {} ({}x{})", yuv422_file, img_width, img_height);
        ENCODE_ORC_LOG_DEBUG("Field dimensions: {}x{}", params.field_width, params.field_height);
        
        // Determine whether this standard should carry biphase VBI metadata (for metadata DB only)
        const bool include_vbi = standard_supports_vbi(ld_standard, system);
        yuv422_loader.close();
        
        // Open TBC file for writing
        ENCODE_ORC_LOG_DEBUG("Writing TBC file: {}", output_filename);
        if (separate_yc) {
            ENCODE_ORC_LOG_DEBUG("  Mode: Separate Y/C");
        }
        
        std::ofstream tbc_file;
        YCTBCWriter yc_writer(yc_legacy ? YCTBCWriter::NamingMode::LEGACY : YCTBCWriter::NamingMode::MODERN);
        
        if (separate_yc) {
            // For separate Y/C mode, use output_filename as base (don't strip .tbc)
            // The YCTBCWriter will add .tbcy and .tbcc extensions (or .tbc and _chroma.tbc for legacy)
            if (!yc_writer.open(output_filename)) {
                error_message_ = "Failed to open Y/C output files: " + output_filename;
                return false;
            }
        } else {
            tbc_file.open(output_filename, std::ios::binary);
            if (!tbc_file) {
                error_message_ = "Failed to open output file: " + output_filename;
                return false;
            }
        }
        
        // Encode and write fields (same image for all frames)
        int32_t total_fields = num_frames * 2;
        for (int32_t frame_num = 0; frame_num < num_frames; ++frame_num) {
            int32_t field_number = frame_num * 2;
            
            if (separate_yc) {
                // Encode with separate Y/C output
                Field y_field1, c_field1, y_field2, c_field2;
                
                if (system == VideoSystem::PAL) {
                    PALEncoder pal_encoder(params, enable_chroma_filter, enable_luma_filter);
                    pal_encoder.set_laserdisc_standard(ld_standard);
                    pal_encoder.encode_frame_yc(image_frame, field_number,
                                                standard_supports_vbi(ld_standard, system) ? frame_num : -1,
                                                y_field1, c_field1, y_field2, c_field2);
                } else {
                    NTSCEncoder ntsc_encoder(params, enable_chroma_filter, enable_luma_filter);
                    ntsc_encoder.set_laserdisc_standard(ld_standard);
                    ntsc_encoder.encode_frame_yc(image_frame, field_number,
                                                 standard_supports_vbi(ld_standard, system) ? frame_num : -1,
                                                 y_field1, c_field1, y_field2, c_field2);
                }
                
                // Write Y and C fields
                yc_writer.write_y_field(y_field1);
                yc_writer.write_c_field(c_field1);
                yc_writer.write_y_field(y_field2);
                yc_writer.write_c_field(c_field2);
                
            } else {
                // Standard composite output
                Frame encoded_frame;
                
                if (system == VideoSystem::PAL) {
                    PALEncoder pal_encoder(params, enable_chroma_filter, enable_luma_filter);
                    pal_encoder.set_laserdisc_standard(ld_standard);
                    encoded_frame = pal_encoder.encode_frame(image_frame, field_number,
                                                             standard_supports_vbi(ld_standard, system) ? frame_num : -1);
                } else {
                    NTSCEncoder ntsc_encoder(params, enable_chroma_filter, enable_luma_filter);
                    ntsc_encoder.set_laserdisc_standard(ld_standard);
                    encoded_frame = ntsc_encoder.encode_frame(image_frame, field_number,
                                                             standard_supports_vbi(ld_standard, system) ? frame_num : -1);
                }
                
                // Write field 1
                const Field& field1 = encoded_frame.field1();
                tbc_file.write(reinterpret_cast<const char*>(field1.data().data()),
                              field1.data().size() * sizeof(uint16_t));
                
                // Write field 2
                const Field& field2 = encoded_frame.field2();
                tbc_file.write(reinterpret_cast<const char*>(field2.data().data()),
                              field2.data().size() * sizeof(uint16_t));
            }
            
            if ((frame_num + 1) % 10 == 0 || frame_num == num_frames - 1) {
                ENCODE_ORC_LOG_DEBUG("Writing field {} / {}", (frame_num + 1) * 2, total_fields);
            }
        }
        
        if (separate_yc) {
            yc_writer.close();
        } else {
            tbc_file.close();
        }
        
        // Create and initialize metadata
        CaptureMetadata metadata;
        metadata.initialize(system, total_fields);
        metadata.video_params = params;
        
        ENCODE_ORC_LOG_DEBUG("Metadata video_params after assignment:");
        ENCODE_ORC_LOG_DEBUG("  blanking: {}", metadata.video_params.blanking_16b_ire);
        ENCODE_ORC_LOG_DEBUG("  black: {}", metadata.video_params.black_16b_ire);
        ENCODE_ORC_LOG_DEBUG("  white: {}", metadata.video_params.white_16b_ire);
        
        metadata.git_branch = "main";
        metadata.git_commit = "v0.1.0-dev";
        metadata.capture_notes = "YUV422 raw image from " + yuv422_file;
        
        // Add VBI data for each field (frame numbers on lines 16, 17, 18) when the standard allows it
        if (include_vbi) {
            metadata.vbi_data.resize(total_fields);
        }
        
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
        
        if (include_vbi) {
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
        }
        
        // Write metadata
        std::string metadata_filename = output_filename + ".db";
        ENCODE_ORC_LOG_DEBUG("Writing metadata: {}", metadata_filename);
        
        MetadataWriter metadata_writer;
        if (!metadata_writer.open(metadata_filename)) {
            error_message_ = "Failed to open metadata database: " + metadata_writer.get_error();
            return false;
        }
        
        if (!metadata_writer.write_metadata(metadata)) {
            error_message_ = "Failed to write metadata: " + metadata_writer.get_error();
            return false;
        }
        
        ENCODE_ORC_LOG_DEBUG("  {}", output_filename);
        ENCODE_ORC_LOG_DEBUG("  {}.db", output_filename);
        
        return true;
        
    } catch (const std::exception& e) {
        error_message_ = std::string("Exception: ") + e.what();
        return false;
    }
}

bool VideoEncoder::encode_png_image(const std::string& output_filename,
                                    VideoSystem system,
                                    LaserDiscStandard ld_standard,
                                    const std::string& png_file,
                                    int32_t num_frames,
                                    int32_t picture_start,
                                    int32_t chapter,
                                    const std::string& timecode_start,
                                    bool enable_chroma_filter,
                                    bool enable_luma_filter,
                                    bool separate_yc,
                                    bool yc_legacy) {
    try {
        VideoParameters params = (system == VideoSystem::PAL)
                                 ? VideoParameters::create_pal_composite()
                                 : VideoParameters::create_ntsc_composite();
        
        // Apply any video level overrides
        VideoParameters::apply_video_level_overrides(params, 
                                                     s_blanking_16b_ire_override,
                                                     s_black_16b_ire_override,
                                                     s_white_16b_ire_override);
        
        if (s_blanking_16b_ire_override.has_value() || 
            s_black_16b_ire_override.has_value() || 
            s_white_16b_ire_override.has_value()) {
            ENCODE_ORC_LOG_DEBUG("Applied video level overrides to params:");
            ENCODE_ORC_LOG_DEBUG("  blanking: {}", params.blanking_16b_ire);
            ENCODE_ORC_LOG_DEBUG("  black: {}", params.black_16b_ire);
            ENCODE_ORC_LOG_DEBUG("  white: {}", params.white_16b_ire);
        }

        // Load PNG image and convert to YUV444P16
        PNGLoader png_loader;
        std::string load_error;
        if (!png_loader.open(png_file, load_error)) {
            error_message_ = load_error;
            return false;
        }
        
        int32_t img_width, img_height;
        if (!png_loader.get_dimensions(img_width, img_height)) {
            error_message_ = "Failed to get PNG dimensions";
            png_loader.close();
            return false;
        }

        ENCODE_ORC_LOG_DEBUG("Encoding {} frames ({} fields)", num_frames, num_frames * 2);
        ENCODE_ORC_LOG_DEBUG("System: {}", (system == VideoSystem::PAL ? "PAL" : "NTSC"));
        ENCODE_ORC_LOG_DEBUG("Image: {} ({}x{})", png_file, img_width, img_height);
        ENCODE_ORC_LOG_DEBUG("Field dimensions: {}x{}", params.field_width, params.field_height);

        FrameBuffer image_frame;
        if (!png_loader.load_frame(0, img_width, img_height, params, image_frame, load_error)) {
            error_message_ = load_error;
            png_loader.close();
            return false;
        }
        png_loader.close();

        std::ofstream tbc_file;
        YCTBCWriter yc_writer(yc_legacy ? YCTBCWriter::NamingMode::LEGACY : YCTBCWriter::NamingMode::MODERN);

        if (separate_yc) {
            if (!yc_writer.open(output_filename)) {
                error_message_ = "Failed to open Y/C output files: " + output_filename;
                return false;
            }
        } else {
            tbc_file.open(output_filename, std::ios::binary);
            if (!tbc_file) {
                error_message_ = "Failed to open output file: " + output_filename;
                return false;
            }
        }

        int32_t total_fields = num_frames * 2;
        for (int32_t frame_num = 0; frame_num < num_frames; ++frame_num) {
            int32_t field_number = frame_num * 2;

            if (separate_yc) {
                Field y_field1, c_field1, y_field2, c_field2;
                if (system == VideoSystem::PAL) {
                    PALEncoder pal_encoder(params, enable_chroma_filter, enable_luma_filter);
                    pal_encoder.set_laserdisc_standard(ld_standard);
                    pal_encoder.encode_frame_yc(image_frame, field_number,
                                                standard_supports_vbi(ld_standard, system) ? frame_num : -1,
                                                y_field1, c_field1, y_field2, c_field2);
                } else {
                    NTSCEncoder ntsc_encoder(params, enable_chroma_filter, enable_luma_filter);
                    ntsc_encoder.set_laserdisc_standard(ld_standard);
                    ntsc_encoder.encode_frame_yc(image_frame, field_number,
                                                 standard_supports_vbi(ld_standard, system) ? frame_num : -1,
                                                 y_field1, c_field1, y_field2, c_field2);
                }

                yc_writer.write_y_field(y_field1);
                yc_writer.write_c_field(c_field1);
                yc_writer.write_y_field(y_field2);
                yc_writer.write_c_field(c_field2);
            } else {
                Frame encoded_frame;
                if (system == VideoSystem::PAL) {
                    PALEncoder pal_encoder(params, enable_chroma_filter, enable_luma_filter);
                    pal_encoder.set_laserdisc_standard(ld_standard);
                    encoded_frame = pal_encoder.encode_frame(image_frame, field_number,
                                                             standard_supports_vbi(ld_standard, system) ? frame_num : -1);
                } else {
                    NTSCEncoder ntsc_encoder(params, enable_chroma_filter, enable_luma_filter);
                    ntsc_encoder.set_laserdisc_standard(ld_standard);
                    encoded_frame = ntsc_encoder.encode_frame(image_frame, field_number,
                                                             standard_supports_vbi(ld_standard, system) ? frame_num : -1);
                }

                const Field& field1 = encoded_frame.field1();
                tbc_file.write(reinterpret_cast<const char*>(field1.data().data()),
                               field1.data().size() * sizeof(uint16_t));
                const Field& field2 = encoded_frame.field2();
                tbc_file.write(reinterpret_cast<const char*>(field2.data().data()),
                               field2.data().size() * sizeof(uint16_t));
            }

            if ((frame_num + 1) % 10 == 0 || frame_num == num_frames - 1) {
                ENCODE_ORC_LOG_DEBUG("Writing field {} / {}", (frame_num + 1) * 2, total_fields);
            }
        }

        if (separate_yc) {
            yc_writer.close();
        } else {
            tbc_file.close();
        }

        CaptureMetadata metadata;
        metadata.initialize(system, total_fields);
        metadata.video_params = params;
        metadata.git_branch = "main";
        metadata.git_commit = "v0.1.0-dev";
        metadata.capture_notes = "PNG image from " + png_file;

        // Add VBI data if supported by the selected standard
        const bool include_vbi_png = standard_supports_vbi(ld_standard, system);
        if (include_vbi_png) {
            metadata.vbi_data.resize(total_fields);
        }

        // Determine VBI mode based on provided parameters
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

        if (include_vbi_png) {
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
        }

        std::string metadata_filename = output_filename + ".db";
        MetadataWriter metadata_writer;
        if (!metadata_writer.open(metadata_filename)) {
            error_message_ = "Failed to open metadata database: " + metadata_writer.get_error();
            return false;
        }
        if (!metadata_writer.write_metadata(metadata)) {
            error_message_ = "Failed to write metadata: " + metadata_writer.get_error();
            return false;
        }

        return true;

    } catch (const std::exception& e) {
        error_message_ = std::string("Exception: ") + e.what();
        return false;
    }
}

bool VideoEncoder::encode_mov_file(const std::string& output_filename,
                                   VideoSystem system,
                                   LaserDiscStandard ld_standard,
                                   const std::string& mov_file,
                                   int32_t num_frames,
                                   int32_t start_frame,
                                   int32_t picture_start,
                                   int32_t chapter,
                                   const std::string& timecode_start,
                                   bool enable_chroma_filter,
                                   bool enable_luma_filter,
                                   bool separate_yc,
                                   bool yc_legacy) {
    try {
        // Get video parameters for the system
        VideoParameters params;
        if (system == VideoSystem::PAL) {
            params = VideoParameters::create_pal_composite();
        } else {
            params = VideoParameters::create_ntsc_composite();
        }

        // Determine whether this standard should carry biphase VBI metadata (for metadata DB only)
        const bool include_vbi = standard_supports_vbi(ld_standard, system);
        
        // Apply any video level overrides
        VideoParameters::apply_video_level_overrides(params, 
                                                     s_blanking_16b_ire_override,
                                                     s_black_16b_ire_override,
                                                     s_white_16b_ire_override);
        
        // Open MOV file
        MOVLoader mov_loader;
        if (!mov_loader.open(mov_file, error_message_)) {
            return false;
        }
        
        // Get dimensions
        int32_t mov_width, mov_height;
        if (!mov_loader.get_dimensions(mov_width, mov_height)) {
            error_message_ = "Failed to get MOV dimensions";
            return false;
        }
        
        // Verify dimensions match expected video system
        int32_t expected_width = 720, expected_height = (system == VideoSystem::PAL) ? 576 : 480;
        
        ENCODE_ORC_LOG_DEBUG("MOV file: {}x{}", mov_width, mov_height);
        ENCODE_ORC_LOG_DEBUG("Expected: {}x{}", expected_width, expected_height);
        
        // Load frames from MOV file
        std::vector<FrameBuffer> frames;
        if (!mov_loader.load_frames(start_frame, num_frames, expected_width, expected_height,
                                    params, frames, error_message_)) {
            mov_loader.close();
            return false;
        }
        
        mov_loader.close();
        
        // Verify we got exactly the number of frames requested
        int32_t actual_num_frames = static_cast<int32_t>(frames.size());
        if (actual_num_frames != num_frames) {
            error_message_ = "Frame count mismatch: requested " + std::to_string(num_frames) +
                           ", got " + std::to_string(actual_num_frames) + 
                           ". MOV file extraction must be frame-accurate.";
            return false;
        }
        
        ENCODE_ORC_LOG_DEBUG("Loaded {} frames from MOV file", actual_num_frames);
        ENCODE_ORC_LOG_DEBUG("Encoding {} frames ({} fields)", num_frames, num_frames * 2);
        
        // Open TBC file for writing
        std::ofstream tbc_file;
        YCTBCWriter yc_writer(yc_legacy ? YCTBCWriter::NamingMode::LEGACY : YCTBCWriter::NamingMode::MODERN);
        
        if (separate_yc) {
            if (!yc_writer.open(output_filename)) {
                error_message_ = "Failed to open Y/C output files: " + output_filename;
                return false;
            }
        } else {
            tbc_file.open(output_filename, std::ios::binary);
            if (!tbc_file) {
                error_message_ = "Failed to open output file: " + output_filename;
                return false;
            }
        }
        
        // Encode and write fields
        for (int32_t frame_num = 0; frame_num < num_frames; ++frame_num) {
            int32_t field_number = frame_num * 2;
            
            if (frame_num % 10 == 0 || frame_num == num_frames - 1) {
                ENCODE_ORC_LOG_DEBUG("Encoding MOV frame {}/{}", frame_num + 1, num_frames);
            }
            
            if (separate_yc) {
                // Encode with separate Y/C output
                Field y_field1, c_field1, y_field2, c_field2;
                
                if (system == VideoSystem::PAL) {
                    PALEncoder pal_encoder(params, enable_chroma_filter, enable_luma_filter);
                    pal_encoder.set_laserdisc_standard(ld_standard);
                    pal_encoder.encode_frame_yc(frames[frame_num], field_number,
                                                standard_supports_vbi(ld_standard, system) ? frame_num : -1,
                                                y_field1, c_field1, y_field2, c_field2);
                } else {
                    NTSCEncoder ntsc_encoder(params, enable_chroma_filter, enable_luma_filter);
                    ntsc_encoder.set_laserdisc_standard(ld_standard);
                    ntsc_encoder.encode_frame_yc(frames[frame_num], field_number,
                                                 standard_supports_vbi(ld_standard, system) ? frame_num : -1,
                                                 y_field1, c_field1, y_field2, c_field2);
                }
                
                // Write Y and C fields
                yc_writer.write_y_field(y_field1);
                yc_writer.write_c_field(c_field1);
                yc_writer.write_y_field(y_field2);
                yc_writer.write_c_field(c_field2);
            } else {
                // Encode with composite output
                Frame frame;
                if (system == VideoSystem::PAL) {
                    PALEncoder pal_encoder(params, enable_chroma_filter, enable_luma_filter);
                    pal_encoder.set_laserdisc_standard(ld_standard);
                    frame = pal_encoder.encode_frame(frames[frame_num], field_number,
                                                    standard_supports_vbi(ld_standard, system) ? frame_num : -1);
                } else {
                    NTSCEncoder ntsc_encoder(params, enable_chroma_filter, enable_luma_filter);
                    ntsc_encoder.set_laserdisc_standard(ld_standard);
                    frame = ntsc_encoder.encode_frame(frames[frame_num], field_number,
                                                     standard_supports_vbi(ld_standard, system) ? frame_num : -1);
                }
                
                // Write fields to TBC file
                const Field& field1 = frame.field1();
                tbc_file.write(reinterpret_cast<const char*>(field1.data().data()),
                              field1.data().size() * sizeof(uint16_t));
                
                const Field& field2 = frame.field2();
                tbc_file.write(reinterpret_cast<const char*>(field2.data().data()),
                              field2.data().size() * sizeof(uint16_t));
            }
        }
        
        // Close files
        if (separate_yc) {
            yc_writer.close();
        } else if (tbc_file.is_open()) {
            tbc_file.close();
        }
        
        // Create and initialize metadata
        int32_t total_fields = num_frames * 2;
        CaptureMetadata metadata;
        metadata.initialize(system, total_fields);
        metadata.video_params = params;
        metadata.git_branch = "main";
        metadata.git_commit = "v0.1.0-dev";
        metadata.capture_notes = "MOV file from " + mov_file;
        
        // Add VBI data for each field when the standard allows it
        if (include_vbi) {
            metadata.vbi_data.resize(total_fields);
            
            // Determine VBI mode
            bool use_cav = (picture_start > 0);
            bool use_clv_chapter = (chapter > 0);
            bool use_clv_timecode = !timecode_start.empty();
            
            int32_t fps = (system == VideoSystem::PAL) ? 25 : 30;
            
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
        }
        
        // Write metadata database
        std::string metadata_filename = output_filename + ".db";
        MetadataWriter metadata_writer;
        if (!metadata_writer.open(metadata_filename)) {
            error_message_ = "Failed to open metadata database: " + metadata_writer.get_error();
            return false;
        }
        if (!metadata_writer.write_metadata(metadata)) {
            error_message_ = "Failed to write metadata: " + metadata_writer.get_error();
            return false;
        }
        
        return true;
        
    } catch (const std::exception& e) {
        error_message_ = std::string("Exception: ") + e.what();
        return false;
    }
}

bool VideoEncoder::encode_mp4_file(const std::string& output_filename,
                                   VideoSystem system,
                                   LaserDiscStandard ld_standard,
                                   const std::string& mp4_file,
                                   int32_t num_frames,
                                   int32_t start_frame,
                                   int32_t picture_start,
                                   int32_t chapter,
                                   const std::string& timecode_start,
                                   bool enable_chroma_filter,
                                   bool enable_luma_filter,
                                   bool separate_yc,
                                   bool yc_legacy) {
    try {
        // Get video parameters for the system
        VideoParameters params;
        if (system == VideoSystem::PAL) {
            params = VideoParameters::create_pal_composite();
        } else {
            params = VideoParameters::create_ntsc_composite();
        }

        // Determine whether this standard should carry biphase VBI metadata (for metadata DB only)
        const bool include_vbi = standard_supports_vbi(ld_standard, system);
        
        // Apply any video level overrides
        VideoParameters::apply_video_level_overrides(params, 
                                                     s_blanking_16b_ire_override,
                                                     s_black_16b_ire_override,
                                                     s_white_16b_ire_override);
        
        // Open MP4 file
        MP4Loader mp4_loader;
        if (!mp4_loader.open(mp4_file, error_message_)) {
            return false;
        }
        
        // Get dimensions
        int32_t mp4_width, mp4_height;
        if (!mp4_loader.get_dimensions(mp4_width, mp4_height)) {
            error_message_ = "Failed to get MP4 dimensions";
            return false;
        }
        
        // Verify dimensions match expected video system
        int32_t expected_width = 720, expected_height = (system == VideoSystem::PAL) ? 576 : 480;
        
        ENCODE_ORC_LOG_DEBUG("MP4 file: {}x{}", mp4_width, mp4_height);
        ENCODE_ORC_LOG_DEBUG("Expected: {}x{}", expected_width, expected_height);
        
        // Load frames from MP4 file
        std::vector<FrameBuffer> frames;
        if (!mp4_loader.load_frames(start_frame, num_frames, expected_width, expected_height,
                                    params, frames, error_message_)) {
            mp4_loader.close();
            return false;
        }
        
        mp4_loader.close();
        
        // Verify we got exactly the number of frames requested
        int32_t actual_num_frames = static_cast<int32_t>(frames.size());
        if (actual_num_frames != num_frames) {
            error_message_ = "Frame count mismatch: requested " + std::to_string(num_frames) +
                           ", got " + std::to_string(actual_num_frames) + 
                           ". MP4 file extraction must be frame-accurate.";
            return false;
        }
        
        ENCODE_ORC_LOG_DEBUG("Loaded {} frames from MP4 file", actual_num_frames);
        ENCODE_ORC_LOG_DEBUG("Encoding {} frames ({} fields)", num_frames, num_frames * 2);
        
        // Open TBC file for writing
        std::ofstream tbc_file;
        YCTBCWriter yc_writer(yc_legacy ? YCTBCWriter::NamingMode::LEGACY : YCTBCWriter::NamingMode::MODERN);
        
        if (separate_yc) {
            if (!yc_writer.open(output_filename)) {
                error_message_ = "Failed to open Y/C output files: " + output_filename;
                return false;
            }
        } else {
            tbc_file.open(output_filename, std::ios::binary);
            if (!tbc_file) {
                error_message_ = "Failed to open output file: " + output_filename;
                return false;
            }
        }
        
        // Encode and write fields
        for (int32_t frame_num = 0; frame_num < num_frames; ++frame_num) {
            int32_t field_number = frame_num * 2;
            
            if (frame_num % 10 == 0 || frame_num == num_frames - 1) {
                ENCODE_ORC_LOG_DEBUG("Encoding MP4 frame {}/{}", frame_num + 1, num_frames);
            }
            
            if (separate_yc) {
                // Encode with separate Y/C output
                Field y_field1, c_field1, y_field2, c_field2;
                
                if (system == VideoSystem::PAL) {
                    PALEncoder pal_encoder(params, enable_chroma_filter, enable_luma_filter);
                    pal_encoder.set_laserdisc_standard(ld_standard);
                    pal_encoder.encode_frame_yc(frames[frame_num], field_number,
                                                standard_supports_vbi(ld_standard, system) ? frame_num : -1,
                                                y_field1, c_field1, y_field2, c_field2);
                } else {
                    NTSCEncoder ntsc_encoder(params, enable_chroma_filter, enable_luma_filter);
                    ntsc_encoder.set_laserdisc_standard(ld_standard);
                    ntsc_encoder.encode_frame_yc(frames[frame_num], field_number,
                                                 standard_supports_vbi(ld_standard, system) ? frame_num : -1,
                                                 y_field1, c_field1, y_field2, c_field2);
                }
                
                // Write Y and C fields
                yc_writer.write_y_field(y_field1);
                yc_writer.write_c_field(c_field1);
                yc_writer.write_y_field(y_field2);
                yc_writer.write_c_field(c_field2);
            } else {
                // Encode with composite output
                Frame frame;
                if (system == VideoSystem::PAL) {
                    PALEncoder pal_encoder(params, enable_chroma_filter, enable_luma_filter);
                    pal_encoder.set_laserdisc_standard(ld_standard);
                    frame = pal_encoder.encode_frame(frames[frame_num], field_number,
                                                    standard_supports_vbi(ld_standard, system) ? frame_num : -1);
                } else {
                    NTSCEncoder ntsc_encoder(params, enable_chroma_filter, enable_luma_filter);
                    ntsc_encoder.set_laserdisc_standard(ld_standard);
                    frame = ntsc_encoder.encode_frame(frames[frame_num], field_number,
                                                     standard_supports_vbi(ld_standard, system) ? frame_num : -1);
                }
                
                // Write fields to TBC file
                const Field& field1 = frame.field1();
                tbc_file.write(reinterpret_cast<const char*>(field1.data().data()),
                              field1.data().size() * sizeof(uint16_t));
                
                const Field& field2 = frame.field2();
                tbc_file.write(reinterpret_cast<const char*>(field2.data().data()),
                              field2.data().size() * sizeof(uint16_t));
            }
        }
        
        // Close files
        if (separate_yc) {
            yc_writer.close();
        } else if (tbc_file.is_open()) {
            tbc_file.close();
        }
        
        // Create and initialize metadata
        int32_t total_fields = num_frames * 2;
        CaptureMetadata metadata;
        metadata.initialize(system, total_fields);
        metadata.video_params = params;
        metadata.git_branch = "main";
        metadata.git_commit = "v0.1.0-dev";
        metadata.capture_notes = "MP4 file from " + mp4_file;
        
        // Add VBI data for each field when the standard allows it
        if (include_vbi) {
            metadata.vbi_data.resize(total_fields);
            
            // Determine VBI mode
            bool use_cav = (picture_start > 0);
            bool use_clv_chapter = (chapter > 0);
            bool use_clv_timecode = !timecode_start.empty();
            
            int32_t fps = (system == VideoSystem::PAL) ? 25 : 30;
            
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
        }
        
        // Write metadata database
        std::string metadata_filename = output_filename + ".db";
        MetadataWriter metadata_writer;
        if (!metadata_writer.open(metadata_filename)) {
            error_message_ = "Failed to open metadata database: " + metadata_writer.get_error();
            return false;
        }
        if (!metadata_writer.write_metadata(metadata)) {
            error_message_ = "Failed to write metadata: " + metadata_writer.get_error();
            return false;
        }
        
        return true;
        
    } catch (const std::exception& e) {
        error_message_ = std::string("Exception: ") + e.what();
        return false;
    }
}

} // namespace encode_orc
