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
#include "video_loader_base.h"
#include <string>

namespace encode_orc {

/**
 * @brief PNG image loader
 * 
 * Loads a PNG image as a single frame (frame_count = 1).
 * Inherits from VideoLoaderBase to provide consistent interface with video loaders.
 */
class PNGLoader : public VideoLoaderBase {
public:
    /**
     * @brief Open a PNG file and prepare for frame extraction
     * 
     * @param filename Path to PNG file
     * @param error_message Error message if opening fails
     * @return true on success, false on error
     */
    bool open(const std::string& filename, std::string& error_message);
    
    /**
     * @brief Get video dimensions from the opened file
     * 
     * @param width Output: image width
     * @param height Output: image height
     * @return true if dimensions are available, false otherwise
     */
    bool get_dimensions(int32_t& width, int32_t& height) const override;
    
    /**
     * @brief Get total number of frames (always 1 for PNG)
     * 
     * @return 1
     */
    int32_t get_frame_count() const override;
    
    /**
     * @brief Load the PNG image
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
    
    /**
     * @brief Load multiple frames (only frame 0 is valid for PNG)
     * 
     * @param start_frame Starting frame number (must be 0)
     * @param num_frames Number of frames to load (must be 1)
     * @param expected_width Expected width
     * @param expected_height Expected height
     * @param params Video parameters
     * @param frames Output vector of frame buffers
     * @param error_message Error message if loading fails
     * @return true on success, false on error
     */
    bool load_frames(int32_t start_frame,
                     int32_t num_frames,
                     int32_t expected_width,
                     int32_t expected_height,
                     const VideoParameters& params,
                     std::vector<FrameBuffer>& frames,
                     std::string& error_message);
    
    /**
     * @brief Close PNG file and release resources
     */
    void close();
    
    /**
     * @brief Check if loader is open
     */
    bool is_open() const override { return is_open_; }
    
    /**
     * @brief Validate PNG format (always succeeds - no frame rate)
     */
    bool validate_format(VideoSystem system, std::string& error_message) override;

private:
    std::string filename_;
    FrameBuffer cached_frame_;
    bool frame_loaded_ = false;
    
    // Private helper methods
    void rgb_to_yuv444p16(const uint8_t* rgb_data, int32_t width, int32_t height, FrameBuffer& frame);
};

} // namespace encode_orc

#endif // ENCODE_ORC_PNG_LOADER_H
