/*
 * File:        rgb30_loader.cpp
 * Module:      encode-orc
 * Purpose:     RGB30 raw image loading implementation
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: 2026 Simon Inns
 */

#include "rgb30_loader.h"
#include "color_conversion.h"
#include <fstream>
#include <cstring>

namespace encode_orc {

bool RGB30Loader::load_rgb30(const std::string& filename,
                             int32_t expected_width,
                             int32_t expected_height,
                             const VideoParameters& /* params */,
                             FrameBuffer& frame,
                             std::string& error_message) {
    try {
        // Open file
        std::ifstream file(filename, std::ios::binary);
        if (!file) {
            error_message = "Cannot open RGB30 file: " + filename;
            return false;
        }
        
        // Get file size
        file.seekg(0, std::ios::end);
        size_t file_size = file.tellg();
        file.seekg(0, std::ios::beg);
        
        // Validate file size
        size_t expected_size = get_expected_file_size(expected_width, expected_height);
        if (file_size != expected_size) {
            error_message = "RGB30 file size mismatch: expected " + std::to_string(expected_size) +
                           " bytes, got " + std::to_string(file_size) + " bytes";
            return false;
        }
        
        // Read raw RGB30 data into temporary buffer
        std::vector<uint16_t> rgb_data(expected_width * expected_height * 3);
        file.read(reinterpret_cast<char*>(rgb_data.data()), file_size);
        file.close();
        
        // Create YUV frame buffer
        frame.resize(expected_width, expected_height, FrameBuffer::Format::YUV444P16);
        
        // Convert RGB30 to full-range RGB16, then to YUV
        uint16_t* y_plane = frame.data().data();
        uint16_t* u_plane = y_plane + (expected_width * expected_height);
        uint16_t* v_plane = u_plane + (expected_width * expected_height);
        
        for (int32_t i = 0; i < expected_width * expected_height; ++i) {
            // Extract RGB30 values (in 16-bit containers, 0-1023)
            uint16_t r10 = rgb_data[i * 3];
            uint16_t g10 = rgb_data[i * 3 + 1];
            uint16_t b10 = rgb_data[i * 3 + 2];
            
            // Convert 10-bit to 16-bit full range
            uint16_t r16 = rgb10_to_rgb16(r10);
            uint16_t g16 = rgb10_to_rgb16(g10);
            uint16_t b16 = rgb10_to_rgb16(b10);
            
            // Convert RGB to YUV
            uint16_t yuv_y, yuv_u, yuv_v;
            ColorConverter::rgb_to_yuv_pixel(r16, g16, b16, yuv_y, yuv_u, yuv_v);
            
            y_plane[i] = yuv_y;
            u_plane[i] = yuv_u;
            v_plane[i] = yuv_v;
        }
        
        return true;
        
    } catch (const std::exception& e) {
        error_message = std::string("Error loading RGB30 file: ") + e.what();
        return false;
    }
}

size_t RGB30Loader::get_expected_file_size(int32_t width, int32_t height) {
    return static_cast<size_t>(width) * height * 3 * sizeof(uint16_t);
}

void RGB30Loader::get_expected_dimensions(const VideoParameters& params,
                                         int32_t& width,
                                         int32_t& height) {
    // Active video region dimensions
    width = params.active_video_end - params.active_video_start;
    // For progressive frames, height is the field height
    height = params.field_height - 2;  // Conservative (excluding top/bottom blanking)
    
    // Standard dimensions for common formats
    if (params.system == VideoSystem::PAL) {
        width = 720;
        height = 576;
    } else if (params.system == VideoSystem::NTSC) {
        width = 720;
        height = 486;
    }
}

uint16_t RGB30Loader::rgb10_to_rgb16(uint16_t value) {
    // Convert 10-bit (0-1023) to 16-bit (0-65535)
    // This scales the range linearly, filling all 16 bits
    // Formula: out = (in << 6) | (in >> 4)
    // This duplicates the MSBs to the LSBs for smooth interpolation
    return (value << 6) | (value >> 4);
}

} // namespace encode_orc
