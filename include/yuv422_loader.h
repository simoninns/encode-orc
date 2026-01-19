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
#include "video_loader_base.h"
#include <string>

namespace encode_orc {

/**
 * @brief Y'CbCr 4:2:2 raw image loader
 * 
 * Loads a Y'CbCr 4:2:2 YUYV-packed (10-bit in 16-bit little-endian containers)
 * raw image file as a single frame (frame_count = 1).
 * Inherits from VideoLoaderBase to provide consistent interface with video loaders.
 * 
 * Format specification:
 * - ITU-R BT.601-derived component video
 * - Y'CbCr 4:2:2 sampling structure
 * - 10-bit quantization, packed as YUYV
 * - Studio range (Y': 64-940, Cb/Cr: 64-960, centered at 512)
 * - Byte order: little-endian
 * - Field order: top field first
 */
class YUV422Loader : public VideoLoaderBase {
public:
    YUV422Loader();
    
    /**
     * @brief Open a YUV422 file and prepare for frame extraction
     * 
     * @param filename Path to YUV422 file
     * @param expected_width Expected width (must be even)
     * @param expected_height Expected height
     * @return true on success, false on error
     */
    bool open(const std::string& filename, int32_t expected_width, int32_t expected_height);
    
    // VideoLoaderBase overrides
    /**
     * @brief Get video dimensions from the opened file (from base class)
     */
    bool get_dimensions(int32_t& width, int32_t& height) const override;
    
    /**
     * @brief Get total number of frames (always 1 for YUV422)
     */
    int32_t get_frame_count() const override;
    
    /**
     * @brief Check if loader is open
     */
    bool is_open() const override;
    
    /**
     * @brief Validate YUV422 format
     */
    bool validate_format(VideoSystem system, std::string& error_message) override;
    
    /**
     * @brief Load a single frame
     * 
     * @param frame_index Frame index (must be 0 for YUV422)
     * @param frame Output frame buffer in YUV444P16 format
     * @return true on success, false on error
     */
    bool load_frame(int32_t frame_index, FrameBuffer& frame);
    
    /**
     * @brief Load multiple frames
     * 
     * @param start_frame Starting frame index (must be 0)
     * @param end_frame Ending frame index (must be 0)
     * @param frames Output frame buffers vector
     * @param error_message Error message if loading fails
     * @return true on success, false on error
     */
    bool load_frames(int32_t start_frame, int32_t end_frame, 
                     std::vector<FrameBuffer>& frames,
                     std::string& error_message);
    
    /**
     * @brief Close YUV422 file and release resources
     */
    void close();

private:
    std::string filename_;
    FrameBuffer cached_frame_;
    bool frame_cached_ = false;
    
    // Helper methods
    static size_t get_expected_file_size(int32_t width, int32_t height);
    static void get_expected_dimensions(const VideoParameters& params, int32_t& width, int32_t& height);
};

} // namespace encode_orc

#endif // ENCODE_ORC_YUV422_LOADER_H
