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
#include "video_loader.h"
#include <string>

namespace encode_orc {

/**
 * @brief PNG image loader
 * 
 * Loads a PNG image as a single frame (frame_count = 1).
 * Phase 1 Refactoring: Now implements the unified VideoLoader interface.
 */
class PNGLoader : public VideoLoader {
public:
    // VideoLoader interface implementation
    bool open(const std::string& path, std::string& error) override;
    VideoMetadata get_metadata() const override;
    bool load_frame(int32_t frame_index, FrameBuffer& output, std::string& error) override;
    bool load_frames(int32_t start, int32_t count, 
                     std::vector<FrameBuffer>& output, std::string& error) override;
    void close() override;
    bool is_open() const override;
    
    /**
     * @brief Load a frame with validation
     * 
     * Legacy method for backward compatibility with expected dimensions and params.
     * 
     * @param frame_number Frame number (must be 0 for PNG)
     * @param expected_width Expected width (validates against image dimensions)
     * @param expected_height Expected height (validates against image dimensions)
     * @param params Video parameters (for clamping luma to video IRE limits)
     * @param frame Output frame buffer in YUV444P16 format
     * @param error_message Error message if loading fails
     * @return true on success, false on error
     */
    bool load_frame(int32_t frame_number,
                    int32_t expected_width,
                    int32_t expected_height,
                    const VideoParameters& params,
                    FrameBuffer& frame,
                    std::string& error_message);

private:
    std::string filename_;
    int32_t width_ = 0;
    int32_t height_ = 0;
    bool is_open_ = false;
    FrameBuffer cached_frame_;
    bool frame_loaded_ = false;
    
    // Private helper methods
    void rgb_to_yuv444p16(const uint8_t* rgb_data, int32_t width, int32_t height, FrameBuffer& frame);
};

} // namespace encode_orc

#endif // ENCODE_ORC_PNG_LOADER_H
