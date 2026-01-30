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
#include "video_loader.h"
#include "video_loader_base.h"
#include <string>

namespace encode_orc {

/**
 * @brief Y'CbCr 4:2:2 raw image loader
 * 
 * Loads a Y'CbCr 4:2:2 YUYV-packed (10-bit in 16-bit little-endian containers)
 * raw image file as a single frame (frame_count = 1).
 * 
 * Phase 1 Refactoring: Now implements the unified VideoLoader interface.
 * 
 * Format specification:
 * - ITU-R BT.601-derived component video
 * - Y'CbCr 4:2:2 sampling structure
 * - 10-bit quantization, packed as YUYV
 * - Studio range (Y': 64-940, Cb/Cr: 64-960, centered at 512)
 * - Byte order: little-endian
 * - Field order: top field first
 */
class YUV422Loader : public VideoLoader {
public:
    YUV422Loader();
    
    // VideoLoader interface implementation
    bool open(const std::string& path, std::string& error) override;
    VideoMetadata get_metadata() const override;
    bool load_frame(int32_t frame_index, FrameBuffer& output, std::string& error) override;
    bool load_frames(int32_t start, int32_t count, 
                     std::vector<FrameBuffer>& output, std::string& error) override;
    void close() override;
    bool is_open() const override;
    
    /**
     * @brief Open a YUV422 file with explicit dimensions
     * 
     * Legacy method for backward compatibility. Prefer using open() with
     * dimensions stored in a VideoParameters object.
     * 
     * @param filename Path to YUV422 file
     * @param expected_width Expected width (must be even)
     * @param expected_height Expected height
     * @return true on success, false on error
     */
    bool open(const std::string& filename, int32_t expected_width, int32_t expected_height);

private:
    std::string filename_;
    int32_t width_ = 0;
    int32_t height_ = 0;
    bool is_open_ = false;
    FrameBuffer cached_frame_;
    bool frame_cached_ = false;
    
    // Helper methods
    static size_t get_expected_file_size(int32_t width, int32_t height);
    static void get_expected_dimensions(const VideoParameters& params, int32_t& width, int32_t& height);
};

} // namespace encode_orc

#endif // ENCODE_ORC_YUV422_LOADER_H
