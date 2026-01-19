/*
 * File:        video_loader_base.cpp
 * Module:      encode-orc
 * Purpose:     Common base class and utilities for video loaders
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: 2026 Simon Inns
 */

#include "video_loader_base.h"
#include <algorithm>
#include <cmath>

namespace encode_orc {

void VideoLoaderUtils::pad_and_upsample_yuv(int32_t target_width,
                                            int32_t actual_width,
                                            int32_t height,
                                            FrameBuffer& frame,
                                            const uint16_t* y_plane,
                                            const uint16_t* u_plane,
                                            const uint16_t* v_plane,
                                            int32_t chroma_h_factor,
                                            int32_t chroma_v_factor,
                                            uint16_t neutral_y,
                                            uint16_t neutral_u,
                                            uint16_t neutral_v) {
    // Allocate output frame
    frame.resize(target_width, height, FrameBuffer::Format::YUV444P16);
    
    uint16_t* out_y = frame.data().data();
    uint16_t* out_u = out_y + (target_width * height);
    uint16_t* out_v = out_u + (target_width * height);
    
    // Calculate padding distribution
    int32_t pixels_to_add = target_width - actual_width;
    int32_t left_pad = pixels_to_add / 2;
    int32_t right_pad = pixels_to_add - left_pad;
    
    // Process each row
    for (int32_t row = 0; row < height; ++row) {
        int32_t out_row_offset = row * target_width;
        
        // Left padding
        for (int32_t col = 0; col < left_pad; ++col) {
            out_y[out_row_offset + col] = neutral_y;
            out_u[out_row_offset + col] = neutral_u;
            out_v[out_row_offset + col] = neutral_v;
        }
        
        // Copy and upsample actual data
        for (int32_t col = 0; col < actual_width; ++col) {
            int32_t out_idx = out_row_offset + left_pad + col;
            
            // Luma is always full resolution
            int32_t y_src_idx = row * actual_width + col;
            out_y[out_idx] = y_plane[y_src_idx];
            
            // Chroma needs upsampling based on factors
            int32_t chroma_col = col / chroma_h_factor;
            int32_t chroma_row = row / chroma_v_factor;
            int32_t chroma_src_idx = chroma_row * (actual_width / chroma_h_factor) + chroma_col;
            
            out_u[out_idx] = u_plane[chroma_src_idx];
            out_v[out_idx] = v_plane[chroma_src_idx];
        }
        
        // Right padding
        for (int32_t col = 0; col < right_pad; ++col) {
            int32_t out_idx = out_row_offset + left_pad + actual_width + col;
            out_y[out_idx] = neutral_y;
            out_u[out_idx] = neutral_u;
            out_v[out_idx] = neutral_v;
        }
    }
}

void VideoLoaderUtils::pad_and_upsample_yuv_8bit(int32_t target_width,
                                                 int32_t actual_width,
                                                 int32_t height,
                                                 FrameBuffer& frame,
                                                 const uint8_t* y_plane,
                                                 const uint8_t* u_plane,
                                                 const uint8_t* v_plane,
                                                 int32_t chroma_h_factor,
                                                 int32_t chroma_v_factor,
                                                 uint8_t neutral_y,
                                                 uint8_t neutral_u,
                                                 uint8_t neutral_v) {
    // Allocate output frame
    frame.resize(target_width, height, FrameBuffer::Format::YUV444P16);
    
    uint16_t* out_y = frame.data().data();
    uint16_t* out_u = out_y + (target_width * height);
    uint16_t* out_v = out_u + (target_width * height);
    
    // Calculate padding distribution
    int32_t pixels_to_add = target_width - actual_width;
    int32_t left_pad = pixels_to_add / 2;
    int32_t right_pad = pixels_to_add - left_pad;
    
    // Conversion lambda for 8-bit values
    auto convert_y = [](uint8_t val) {
        return luma_8bit_to_10bit(val);
    };
    
    auto convert_u = [](uint8_t val) {
        return chroma_8bit_to_normalized(val);
    };
    
    auto convert_v = [](uint8_t val) {
        return chroma_8bit_to_normalized(val);
    };
    
    auto neutral_y_out = convert_y(neutral_y);
    auto neutral_u_out = convert_u(neutral_u);
    auto neutral_v_out = convert_v(neutral_v);
    
    // Process each row
    for (int32_t row = 0; row < height; ++row) {
        int32_t out_row_offset = row * target_width;
        
        // Left padding
        for (int32_t col = 0; col < left_pad; ++col) {
            out_y[out_row_offset + col] = neutral_y_out;
            out_u[out_row_offset + col] = neutral_u_out;
            out_v[out_row_offset + col] = neutral_v_out;
        }
        
        // Copy and upsample actual data
        for (int32_t col = 0; col < actual_width; ++col) {
            int32_t out_idx = out_row_offset + left_pad + col;
            
            // Luma is always full resolution
            int32_t y_src_idx = row * actual_width + col;
            out_y[out_idx] = convert_y(y_plane[y_src_idx]);
            
            // Chroma needs upsampling based on factors
            int32_t chroma_col = col / chroma_h_factor;
            int32_t chroma_row = row / chroma_v_factor;
            int32_t chroma_src_idx = chroma_row * (actual_width / chroma_h_factor) + chroma_col;
            
            out_u[out_idx] = convert_u(u_plane[chroma_src_idx]);
            out_v[out_idx] = convert_v(v_plane[chroma_src_idx]);
        }
        
        // Right padding
        for (int32_t col = 0; col < right_pad; ++col) {
            int32_t out_idx = out_row_offset + left_pad + actual_width + col;
            out_y[out_idx] = neutral_y_out;
            out_u[out_idx] = neutral_u_out;
            out_v[out_idx] = neutral_v_out;
        }
    }
}

bool VideoLoaderUtils::validate_frame_rate(double frame_rate, VideoSystem system,
                                           double tolerance_fps) {
    if (frame_rate <= 0.0) {
        return true;  // Can't validate, not an error
    }
    
    double expected_fps = get_expected_frame_rate(system);
    double fps_diff = std::abs(frame_rate - expected_fps);
    
    return fps_diff <= tolerance_fps;
}

double VideoLoaderUtils::get_expected_frame_rate(VideoSystem system) {
    return (system == VideoSystem::PAL) ? 25.0 : 29.97;
}

bool VideoLoaderBase::validate_dimensions(int32_t expected_width, int32_t expected_height,
                                          std::string& error_message) const {
    if (width_ != expected_width || height_ != expected_height) {
        error_message = "Video dimension mismatch: expected " + 
                       std::to_string(expected_width) + "x" + std::to_string(expected_height) +
                       ", got " + std::to_string(width_) + "x" + std::to_string(height_);
        return false;
    }
    return true;
}

bool VideoLoaderBase::validate_frame_range(int32_t start_frame, int32_t num_frames,
                                           std::string& error_message) const {
    if (frame_count_ > 0 && start_frame + num_frames > frame_count_) {
        error_message = "Requested frame range exceeds video length: " +
                       std::to_string(start_frame) + "-" + std::to_string(start_frame + num_frames - 1) +
                       " (video has " + std::to_string(frame_count_) + " frames)";
        return false;
    }
    return true;
}


} // namespace encode_orc
