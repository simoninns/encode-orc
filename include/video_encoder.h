/*
 * File:        video_encoder.h
 * Module:      encode-orc
 * Purpose:     Main video encoder coordinating RGB30 image loading and PAL/NTSC encoding
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: 2026 Simon Inns
 */

#ifndef ENCODE_ORC_VIDEO_ENCODER_H
#define ENCODE_ORC_VIDEO_ENCODER_H

#include "video_parameters.h"
#include "laserdisc_standard.h"
#include "pal_encoder.h"
#include "ntsc_encoder.h"
#include "tbc_writer.h"
#include "metadata_writer.h"
#include <string>
#include <cstdint>

namespace encode_orc {

/**
 * @brief Main video encoder class
 * 
 * Coordinates RGB30 image loading, PAL/NTSC encoding, and file output
 */
class VideoEncoder {
public:
    /**
     * @brief Encode video with RGB30 image repeated for multiple frames
     * @param output_filename Output .tbc filename (or base filename for Y/C mode)
     * @param system Video system (PAL or NTSC)
     * @param rgb30_file Path to RGB30 raw image file
     * @param num_frames Number of frames to encode (image repeated each frame)
     * @param verbose Enable verbose output
     * @param picture_start Starting CAV picture number (0 = not used)
     * @param chapter CLV chapter number (0 = not used)
     * @param timecode_start CLV timecode HH:MM:SS:FF (empty = not used)
     * @param enable_chroma_filter Enable 1.3 MHz chroma low-pass filter (default: true)
     * @param enable_luma_filter Enable luma low-pass filter (default: false)
     * @param separate_yc Enable separate Y/C TBC output (default: false)
     * @param yc_legacy Use legacy naming (.tbc/.tbc_chroma) instead of modern (.tbcy/.tbcc) (default: false)
     * @return true on success, false on error
     */
    bool encode_rgb30_image(const std::string& output_filename,
                           VideoSystem system,
                           LaserDiscStandard ld_standard,
                           const std::string& rgb30_file,
                           int32_t num_frames,
                           bool verbose = false,
                           int32_t picture_start = 0,
                           int32_t chapter = 0,
                           const std::string& timecode_start = "",
                           bool enable_chroma_filter = true,
                           bool enable_luma_filter = false,
                           bool separate_yc = false,
                           bool yc_legacy = false);
    
    /**
     * @brief Encode video with PNG image repeated for multiple frames
     * @param output_filename Output .tbc filename (or base filename for Y/C mode)
     * @param system Video system (PAL or NTSC)
     * @param png_file Path to PNG image file
     * @param num_frames Number of frames to encode (image repeated each frame)
     * @param verbose Enable verbose output
     * @param picture_start Starting CAV picture number (0 = not used)
     * @param chapter CLV chapter number (0 = not used)
     * @param timecode_start CLV timecode HH:MM:SS:FF (empty = not used)
     * @param enable_chroma_filter Enable 1.3 MHz chroma low-pass filter (default: true)
     * @param enable_luma_filter Enable luma low-pass filter (default: false)
     * @param separate_yc Enable separate Y/C TBC output (default: false)
     * @param yc_legacy Use legacy naming (.tbc/.tbc_chroma) instead of modern (.tbcy/.tbcc) (default: false)
     * @return true on success, false on error
     */
    bool encode_png_image(const std::string& output_filename,
                          VideoSystem system,
                          LaserDiscStandard ld_standard,
                          const std::string& png_file,
                          int32_t num_frames,
                          bool verbose = false,
                          int32_t picture_start = 0,
                          int32_t chapter = 0,
                          const std::string& timecode_start = "",
                          bool enable_chroma_filter = true,
                          bool enable_luma_filter = false,
                          bool separate_yc = false,
                          bool yc_legacy = false);
    
    /**
     * @brief Get error message from last operation
     */
    const std::string& get_error() const { return error_message_; }
    
private:
    std::string error_message_;
};

} // namespace encode_orc

#endif // ENCODE_ORC_VIDEO_ENCODER_H
