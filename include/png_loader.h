/*
 * File:        png_loader.h
 * Module:      encode-orc
 * Purpose:     PNG image loading utility
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: 2026 Simon Inns
 */

#ifndef ENCODE_ORC_PNG_LOADER_H
#define ENCODE_ORC_PNG_LOADER_H

#include "frame_buffer.h"
#include "video_parameters.h"
#include <string>

namespace encode_orc {

/**
 * @brief PNG image loader
 */
class PNGLoader {
public:
    /**
     * @brief Load a PNG image and convert to frame buffer
     * 
     * @param filename Path to PNG file
     * @param params Video parameters (used to determine expected dimensions)
     * @param frame Output frame buffer in YUV444P16 format
     * @param error_message Error message if loading fails
     * @return true on success, false on error
     */
    static bool load_png(const std::string& filename, const VideoParameters& params,
                         FrameBuffer& frame, std::string& error_message);
    
    /**
     * @brief Get dimensions of a PNG file without loading the full image
     * 
     * @param filename Path to PNG file
     * @param width Output width in pixels
     * @param height Output height in pixels
     * @param error_message Error message if loading fails
     * @return true on success, false on error
     */
    static bool get_png_dimensions(const std::string& filename, int32_t& width, 
                                   int32_t& height, std::string& error_message);
    
    /**
     * @brief Get expected active video dimensions for a video system
     * 
     * @param params Video parameters
     * @param width Output width in pixels
     * @param height Output height in pixels
     */
    static void get_expected_dimensions(const VideoParameters& params, 
                                       int32_t& width, int32_t& height);

private:
    /**
     * @brief Convert RGB buffer to YUV444P16
     */
    static void rgb_to_yuv444p16(const uint8_t* rgb_data, int32_t width, int32_t height,
                                  FrameBuffer& frame);
};

} // namespace encode_orc

#endif // ENCODE_ORC_PNG_LOADER_H
