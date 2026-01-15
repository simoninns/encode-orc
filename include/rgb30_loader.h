/*
 * File:        rgb30_loader.h
 * Module:      encode-orc
 * Purpose:     RGB30 raw image loading utility
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: 2026 Simon Inns
 */

#ifndef ENCODE_ORC_RGB30_LOADER_H
#define ENCODE_ORC_RGB30_LOADER_H

#include "frame_buffer.h"
#include "video_parameters.h"
#include <string>

namespace encode_orc {

/**
 * @brief RGB30 raw image loader
 * 
 * Loads interleaved RGB30 (10-bit per channel in 16-bit little-endian containers)
 * raw image files and converts them to YUV444P16 frame buffers for video encoding.
 */
class RGB30Loader {
public:
    /**
     * @brief Load an RGB30 raw image and convert to YUV frame buffer
     * 
     * @param filename Path to RGB30 raw file
     * @param expected_width Expected width (validates file size)
     * @param expected_height Expected height (validates file size)
     * @param params Video parameters (for RGB to YUV conversion)
     * @param frame Output frame buffer in YUV444P16 format
     * @param error_message Error message if loading fails
     * @return true on success, false on error
     */
    static bool load_rgb30(const std::string& filename,
                           int32_t expected_width,
                           int32_t expected_height,
                           const VideoParameters& params,
                           FrameBuffer& frame,
                           std::string& error_message);
    
    /**
     * @brief Get expected file size for an RGB30 image
     * 
     * @param width Image width
     * @param height Image height
     * @return Expected file size in bytes (width × height × 6)
     */
    static size_t get_expected_file_size(int32_t width, int32_t height);
    
    /**
     * @brief Get active video dimensions for a video system
     * 
     * @param params Video parameters
     * @param width Output width in pixels
     * @param height Output height in pixels
     */
    static void get_expected_dimensions(const VideoParameters& params,
                                        int32_t& width,
                                        int32_t& height);

private:
    /**
     * @brief Convert 10-bit RGB (0-1023) to 16-bit RGB (0-65535)
     */
    static uint16_t rgb10_to_rgb16(uint16_t value);
};

} // namespace encode_orc

#endif // ENCODE_ORC_RGB30_LOADER_H
