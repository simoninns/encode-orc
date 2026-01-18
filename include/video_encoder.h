/*
 * File:        video_encoder.h
 * Module:      encode-orc
 * Purpose:     Main video encoder coordinating raw image loading and PAL/NTSC encoding
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
#include <optional>

namespace encode_orc {

/**
 * @brief Main video encoder class
 * 
 * Coordinates raw image loading (Y'CbCr 4:2:2 or PNG), PAL/NTSC encoding, and file output
 */
class VideoEncoder {
public:
    /**
     * @brief Set video level overrides for subsequent encoding operations
     * @param blanking_16b_ire Optional blanking level override
     * @param black_16b_ire Optional black level override
     * @param white_16b_ire Optional white level override
     */
    static void set_video_level_overrides(std::optional<int32_t> blanking_16b_ire = std::nullopt,
                                          std::optional<int32_t> black_16b_ire = std::nullopt,
                                          std::optional<int32_t> white_16b_ire = std::nullopt);
    
    /**
     * @brief Clear all video level overrides
     */
    static void clear_video_level_overrides();
    
    /**
     * @brief Encode video with Y'CbCr 4:2:2 raw image repeated for multiple frames
     * @param output_filename Output .tbc filename (or base filename for Y/C mode)
     * @param system Video system (PAL or NTSC)
     * @param yuv422_file Path to Y'CbCr 4:2:2 raw image file (YUYV packed, 10-bit studio range)
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
    bool encode_yuv422_image(const std::string& output_filename,
                            VideoSystem system,
                            LaserDiscStandard ld_standard,
                            const std::string& yuv422_file,
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
     * @brief Encode video from MOV file frames
     * @param output_filename Output .tbc filename (or base filename for Y/C mode)
     * @param system Video system (PAL or NTSC)
     * @param ld_standard LaserDisc standard
     * @param mov_file Path to MOV file (v210 or other ffmpeg-supported format)
     * @param num_frames Number of frames to encode
     * @param start_frame Starting frame number in MOV file (0-indexed, default: 0)
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
    bool encode_mov_file(const std::string& output_filename,
                         VideoSystem system,
                         LaserDiscStandard ld_standard,
                         const std::string& mov_file,
                         int32_t num_frames,
                         int32_t start_frame = 0,
                         bool verbose = false,
                         int32_t picture_start = 0,
                         int32_t chapter = 0,
                         const std::string& timecode_start = "",
                         bool enable_chroma_filter = true,
                         bool enable_luma_filter = false,
                         bool separate_yc = false,
                         bool yc_legacy = false);
    
    /**
     * @brief Encode video from MP4 file frames
     * @param output_filename Output .tbc filename (or base filename for Y/C mode)
     * @param system Video system (PAL or NTSC)
     * @param ld_standard LaserDisc standard
     * @param mp4_file Path to MP4 file (H.264, H.265, or other ffmpeg-supported codec)
     * @param num_frames Number of frames to encode
     * @param start_frame Starting frame number in MP4 file (0-indexed, default: 0)
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
    bool encode_mp4_file(const std::string& output_filename,
                         VideoSystem system,
                         LaserDiscStandard ld_standard,
                         const std::string& mp4_file,
                         int32_t num_frames,
                         int32_t start_frame = 0,
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
    
    // Static video level overrides for all encoding operations
    static std::optional<int32_t> s_blanking_16b_ire_override;
    static std::optional<int32_t> s_black_16b_ire_override;
    static std::optional<int32_t> s_white_16b_ire_override;
};

} // namespace encode_orc

#endif // ENCODE_ORC_VIDEO_ENCODER_H
