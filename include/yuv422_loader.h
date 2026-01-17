/*
 * File:        yuv422_loader.h
 * Module:      encode-orc
 * Purpose:     Y'CbCr 4:2:2 raw image loading utility
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: 2026 Simon Inns
 */

#ifndef ENCODE_ORC_YUV422_LOADER_H
#define ENCODE_ORC_YUV422_LOADER_H

#include "frame_buffer.h"
#include "video_parameters.h"
#include <string>

namespace encode_orc {

/**
 * @brief Y'CbCr 4:2:2 raw image loader
 * 
 * Loads Y'CbCr 4:2:2 YUYV-packed (10-bit in 16-bit little-endian containers)
 * raw image files and converts them to YUV444P16 frame buffers for video encoding.
 * 
 * Format specification:
 * - ITU-R BT.601-derived component video
 * - Y'CbCr 4:2:2 sampling structure
 * - 10-bit quantization, packed as YUYV
 * - Studio range (Y': 64-940, Cb/Cr: 64-960, centered at 512)
 * - Byte order: little-endian
 * - Field order: top field first
 */
class YUV422Loader {
public:
    /**
     * @brief Load a Y'CbCr 4:2:2 raw image and convert to YUV frame buffer
     * 
     * @param filename Path to YUV422 YUYV raw file
     * @param expected_width Expected width (must be even, validates file size)
     * @param expected_height Expected height (validates file size)
     * @param params Video parameters (for metadata, not conversion)
     * @param frame Output frame buffer in YUV444P16 format
     * @param error_message Error message if loading fails
     * @return true on success, false on error
     */
    static bool load_yuv422(const std::string& filename,
                            int32_t expected_width,
                            int32_t expected_height,
                            const VideoParameters& params,
                            FrameBuffer& frame,
                            std::string& error_message);
    
    /**
     * @brief Get expected file size for a Y'CbCr 4:2:2 YUYV image
     * 
     * @param width Image width (must be even)
     * @param height Image height
     * @return Expected file size in bytes ((width/2) × height × 4 components × 2 bytes)
     */
    static size_t get_expected_file_size(int32_t width, int32_t height);
    
    /**
     * @brief Get expected dimensions from video parameters
     * 
     * @param params Video parameters
     * @param width Output: expected image width
     * @param height Output: expected image height
     */
    static void get_expected_dimensions(const VideoParameters& params,
                                       int32_t& width,
                                       int32_t& height);

private:
    /**
     * @brief Convert 10-bit studio range to 16-bit full range
     * 
     * Converts from ITU-R BT.601 studio range to full 16-bit range:
     * - Y': 64-940 → 0-65535
     * - Cb/Cr: 64-960 (centered at 512) → 0-65535 (centered at 32768)
     * 
     * @param value 10-bit studio range value
     * @param is_luma true for Y' (luma), false for Cb/Cr (chroma)
     * @return 16-bit full range value
     */
    static uint16_t studio10_to_full16(uint16_t value, bool is_luma);
};

} // namespace encode_orc

#endif // ENCODE_ORC_YUV422_LOADER_H
