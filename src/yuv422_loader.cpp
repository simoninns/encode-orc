/*
 * File:        yuv422_loader.cpp
 * Module:      encode-orc
 * Purpose:     Y'CbCr 4:2:2 raw image loading implementation
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: 2026 Simon Inns
 */

#include "yuv422_loader.h"
#include "video_loader_base.h"
#include <fstream>
#include <cstring>
#include <algorithm>
#include <filesystem>

namespace encode_orc {

YUV422Loader::YUV422Loader()
    : filename_(""), frame_cached_(false) {
}

bool YUV422Loader::open(const std::string& filename, int32_t expected_width, int32_t expected_height) {
    // Validate width is even (required for 4:2:2 sampling)
    if (expected_width % 2 != 0) {
        return false;
    }
    
    // Check if file exists
    if (!std::filesystem::exists(filename)) {
        return false;
    }
    
    filename_ = filename;
    width_ = expected_width;
    height_ = expected_height;
    frame_count_ = 1;  // Single frame
    
    // Validate file size
    std::ifstream file(filename, std::ios::binary);
    if (!file) {
        return false;
    }
    
    file.seekg(0, std::ios::end);
    size_t file_size = file.tellg();
    file.close();
    
    size_t expected_size = get_expected_file_size(expected_width, expected_height);
    if (file_size != expected_size) {
        return false;
    }
    
    is_open_ = true;
    return true;
}

bool YUV422Loader::get_dimensions(int32_t& width, int32_t& height) const {
    if (!is_open_) {
        return false;
    }
    width = width_;
    height = height_;
    return true;
}

int32_t YUV422Loader::get_frame_count() const {
    return 1;
}

bool YUV422Loader::is_open() const {
    return is_open_;
}

bool YUV422Loader::validate_format(VideoSystem /* system */, std::string& error_message) {
    // YUV422 is a single frame, no frame rate
    error_message = "YUV422 format does not have frame rate";
    return false;
}

bool YUV422Loader::load_frame(int32_t frame_index, FrameBuffer& frame) {
    std::vector<FrameBuffer> frames;
    std::string error_message;
    if (!load_frames(frame_index, frame_index, frames, error_message)) {
        return false;
    }
    
    if (frames.empty()) {
        return false;
    }
    
    frame = std::move(frames[0]);
    return true;
}

bool YUV422Loader::load_frames(int32_t start_frame, int32_t end_frame, 
                                std::vector<FrameBuffer>& frames,
                                std::string& error_message) {
    if (!is_open_) {
        error_message = "YUV422 loader is not open";
        return false;
    }
    
    // Only frame 0 is valid
    if (start_frame != 0 || end_frame != 0) {
        error_message = "YUV422 loader only supports frame 0";
        return false;
    }
    
    // Return cached frame if already loaded
    if (frame_cached_) {
        frames.push_back(cached_frame_);
        return true;
    }
    
    try {
        // Open and read raw YUYV data
        std::ifstream file(filename_, std::ios::binary);
        if (!file) {
            error_message = "Cannot open YUV422 file: " + filename_;
            return false;
        }
        
        // Read raw YUYV data into temporary buffer
        // Format: Y0 U0 Y1 V0 (4 components per 2 pixels, each 16-bit little-endian)
        size_t num_components = (width_ / 2) * height_ * 4;
        std::vector<uint16_t> yuyv_data(num_components);
        file.read(reinterpret_cast<char*>(yuyv_data.data()), num_components * sizeof(uint16_t));
        file.close();
        
        // Create YUV frame buffer
        cached_frame_.resize(width_, height_, FrameBuffer::Format::YUV444P16);
        
        // Get plane pointers
        uint16_t* y_plane = cached_frame_.data().data();
        uint16_t* u_plane = y_plane + (width_ * height_);
        uint16_t* v_plane = u_plane + (width_ * height_);
        
        // Convert studio range to normalized range
        auto to_luma = [](uint16_t studio_value) {
            // Map 10-bit studio range (64-940) to normalized range
            if (studio_value < 64) return VideoLoaderUtils::NORMALIZED_LUMA_MIN_10BIT;
            if (studio_value > 940) return VideoLoaderUtils::NORMALIZED_LUMA_MAX_10BIT;
            
            // Map 64-940 to 64-940 (preserve exact values)
            return studio_value;
        };

        auto to_chroma = [](uint16_t studio_value) {
            // Map studio chroma range (64-960) to normalized range (0-896)
            if (studio_value < 64) return uint16_t(0);
            if (studio_value > 960) return VideoLoaderUtils::NORMALIZED_CHROMA_MAX_10BIT;
            
            // Map 64-960 to 0-896
            int32_t delta = studio_value - 64;
            int32_t scaled = (delta * 896) / 896;
            return static_cast<uint16_t>(scaled);
        };

        // Convert YUYV 4:2:2 to YUV444 (upsample chroma)
        // Process pairs of pixels
        for (int32_t row = 0; row < height_; ++row) {
            for (int32_t col = 0; col < width_; col += 2) {
                int32_t pixel_pair_idx = (row * (width_ / 2)) + (col / 2);
                int32_t component_idx = pixel_pair_idx * 4;
                
                // Extract YUYV components (10-bit studio range in 16-bit containers)
                uint16_t y0_studio = yuyv_data[component_idx];
                uint16_t cb_studio = yuyv_data[component_idx + 1];
                uint16_t y1_studio = yuyv_data[component_idx + 2];
                uint16_t cr_studio = yuyv_data[component_idx + 3];
                
                // Convert studio range to normalized range
                uint16_t y0_norm = to_luma(y0_studio);
                uint16_t y1_norm = to_luma(y1_studio);
                uint16_t cb_norm = to_chroma(cb_studio);
                uint16_t cr_norm = to_chroma(cr_studio);
                
                // Calculate linear indices for the pixel pair
                int32_t idx0 = row * width_ + col;
                int32_t idx1 = row * width_ + col + 1;
                
                // Store luma for both pixels
                y_plane[idx0] = y0_norm;
                y_plane[idx1] = y1_norm;
                
                // Store chroma (shared by both pixels in 4:2:2)
                u_plane[idx0] = cb_norm;
                u_plane[idx1] = cb_norm;
                v_plane[idx0] = cr_norm;
                v_plane[idx1] = cr_norm;
            }
        }
        
        frame_cached_ = true;
        frames.push_back(cached_frame_);
        return true;
        
    } catch (const std::exception& e) {
        error_message = std::string("Error loading YUV422 file: ") + e.what();
        return false;
    }
}

void YUV422Loader::close() {
    is_open_ = false;
    filename_ = "";
    cached_frame_ = FrameBuffer();
    frame_cached_ = false;
}

size_t YUV422Loader::get_expected_file_size(int32_t width, int32_t height) {
    // YUYV format: 4 components per 2 pixels, each component is 2 bytes (16-bit)
    // Total: (width/2) * height * 4 * 2
    return static_cast<size_t>(width / 2) * height * 4 * sizeof(uint16_t);
}

void YUV422Loader::get_expected_dimensions(const VideoParameters& params,
                                           int32_t& width,
                                           int32_t& height) {
    // Active video region dimensions
    // Standard dimensions for common formats
    if (params.system == VideoSystem::PAL) {
        width = 720;
        height = 576;
    } else if (params.system == VideoSystem::NTSC) {
        width = 720;
        height = 480;
    } else {
        // Fallback to calculating from parameters
        width = params.active_video_end - params.active_video_start;
        height = params.field_height - 2;  // Conservative (excluding top/bottom blanking)
    }
}

} // namespace encode_orc
