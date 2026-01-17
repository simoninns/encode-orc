/*
 * File:        yuv422_loader.cpp
 * Module:      encode-orc
 * Purpose:     Y'CbCr 4:2:2 raw image loading implementation
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: 2026 Simon Inns
 */

#include "yuv422_loader.h"
#include <fstream>
#include <cstring>
#include <algorithm>

namespace encode_orc {

bool YUV422Loader::load_yuv422(const std::string& filename,
                                int32_t expected_width,
                                int32_t expected_height,
                                const VideoParameters& /* params */,
                                FrameBuffer& frame,
                                std::string& error_message) {
    try {
        // Validate width is even (required for 4:2:2 sampling)
        if (expected_width % 2 != 0) {
            error_message = "YUV422 loader requires even width, got " + std::to_string(expected_width);
            return false;
        }
        
        // Open file
        std::ifstream file(filename, std::ios::binary);
        if (!file) {
            error_message = "Cannot open YUV422 file: " + filename;
            return false;
        }
        
        // Get file size
        file.seekg(0, std::ios::end);
        size_t file_size = file.tellg();
        file.seekg(0, std::ios::beg);
        
        // Validate file size
        size_t expected_size = get_expected_file_size(expected_width, expected_height);
        if (file_size != expected_size) {
            error_message = "YUV422 file size mismatch: expected " + std::to_string(expected_size) +
                           " bytes, got " + std::to_string(file_size) + " bytes";
            return false;
        }
        
        // Read raw YUYV data into temporary buffer
        // Format: Y0 U0 Y1 V0 (4 components per 2 pixels, each 16-bit little-endian)
        size_t num_components = (expected_width / 2) * expected_height * 4;
        std::vector<uint16_t> yuyv_data(num_components);
        file.read(reinterpret_cast<char*>(yuyv_data.data()), file_size);
        file.close();
        
        // Create YUV frame buffer
        frame.resize(expected_width, expected_height, FrameBuffer::Format::YUV444P16);
        
        // Get plane pointers
        uint16_t* y_plane = frame.data().data();
        uint16_t* u_plane = y_plane + (expected_width * expected_height);
        uint16_t* v_plane = u_plane + (expected_width * expected_height);
        
        // Keep studio-range codes as-is (0-1023 for luma, 0-1023 for chroma)
        // This preserves sub-black (0-63) and allows the encoder to detect studio range by checking y_max <= 1023.
        // The encoder will then apply final scaling/mapping to video levels.
        auto to_luma = [](uint16_t studio_value) {
            // Clamp to 10-bit studio range but allow full range in 16-bit container
            return std::min<uint16_t>(studio_value, 1023);
        };

        // Chroma: keep in studio range (64-960 → mapped to 0-1023)
        auto to_chroma = [](uint16_t studio_value) {
            // Studio chroma: 64→0, 512→448, 960→896; clamp upper only
            uint16_t capped = std::min<uint16_t>(studio_value, 960);
            int32_t delta = capped - 64;
            int32_t scaled = (delta * 896) / 896;  // This is just delta, 0-896
            return static_cast<uint16_t>(std::min(896, scaled));
        };

        // Convert YUYV 4:2:2 to YUV444 (upsample chroma)
        // Process pairs of pixels
        for (int32_t row = 0; row < expected_height; ++row) {
            for (int32_t col = 0; col < expected_width; col += 2) {
                int32_t pixel_pair_idx = (row * (expected_width / 2)) + (col / 2);
                int32_t component_idx = pixel_pair_idx * 4;
                
                // Extract YUYV components (10-bit studio range in 16-bit containers)
                uint16_t y0_studio = yuyv_data[component_idx];
                uint16_t cb_studio = yuyv_data[component_idx + 1];
                uint16_t y1_studio = yuyv_data[component_idx + 2];
                uint16_t cr_studio = yuyv_data[component_idx + 3];
                
                // Convert studio range to full range (16-bit)
                uint16_t y0_full = to_luma(y0_studio);
                uint16_t y1_full = to_luma(y1_studio);
                uint16_t cb_full = to_chroma(cb_studio);
                uint16_t cr_full = to_chroma(cr_studio);
                
                // Calculate linear indices for the pixel pair
                int32_t idx0 = row * expected_width + col;
                int32_t idx1 = row * expected_width + col + 1;
                
                // Store luma for both pixels
                y_plane[idx0] = y0_full;
                y_plane[idx1] = y1_full;
                
                // Store chroma (shared by both pixels in 4:2:2)
                u_plane[idx0] = cb_full;
                u_plane[idx1] = cb_full;
                v_plane[idx0] = cr_full;
                v_plane[idx1] = cr_full;
            }
        }
        
        return true;
        
    } catch (const std::exception& e) {
        error_message = std::string("Error loading YUV422 file: ") + e.what();
        return false;
    }
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
