/*
 * File:        png_loader.cpp
 * Module:      encode-orc
 * Purpose:     PNG image loading implementation using libpng
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: 2026 Simon Inns
 */

#include "png_loader.h"
#include "video_loader_base.h"
#include "color_conversion.h"
#include <png.h>
#include <cstdio>
#include <vector>
#include <stdexcept>
#include <algorithm>

namespace encode_orc {

static void png_read_error(png_structp png_ptr, png_const_charp msg) {
    (void)png_ptr;
    throw std::runtime_error(std::string("libpng error: ") + msg);
}

bool PNGLoader::open(const std::string& filename, std::string& error_message) {
    try {
        filename_ = filename;
        
        // Probe PNG to get dimensions
        FILE* fp = std::fopen(filename.c_str(), "rb");
        if (!fp) {
            error_message = "Cannot open PNG file: " + filename;
            return false;
        }

        png_structp png_ptr = png_create_read_struct(PNG_LIBPNG_VER_STRING, nullptr, png_read_error, nullptr);
        if (!png_ptr) {
            std::fclose(fp);
            error_message = "Failed to create PNG read struct";
            return false;
        }
        png_infop info_ptr = png_create_info_struct(png_ptr);
        if (!info_ptr) {
            png_destroy_read_struct(&png_ptr, nullptr, nullptr);
            std::fclose(fp);
            error_message = "Failed to create PNG info struct";
            return false;
        }

        if (setjmp(png_jmpbuf(png_ptr))) {
            png_destroy_read_struct(&png_ptr, &info_ptr, nullptr);
            std::fclose(fp);
            error_message = "libpng read error";
            return false;
        }

        png_init_io(png_ptr, fp);
        png_read_info(png_ptr, info_ptr);

        png_uint_32 w, h;
        int bit_depth, color_type;
        png_get_IHDR(png_ptr, info_ptr, &w, &h, &bit_depth, &color_type, nullptr, nullptr, nullptr);
        png_destroy_read_struct(&png_ptr, &info_ptr, nullptr);
        std::fclose(fp);
        
        width_ = static_cast<int32_t>(w);
        height_ = static_cast<int32_t>(h);
        frame_count_ = 1;
        frame_rate_ = 0.0;
        is_open_ = true;
        frame_loaded_ = false;
        
        return true;
    } catch (const std::exception& e) {
        error_message = e.what();
        return false;
    }
}

bool PNGLoader::get_dimensions(int32_t& width, int32_t& height) const {
    if (!is_open_) {
        return false;
    }
    width = width_;
    height = height_;
    return true;
}

int32_t PNGLoader::get_frame_count() const {
    return 1;
}

bool PNGLoader::load_frame(int32_t frame_number,
                           int32_t expected_width,
                           int32_t expected_height,
                           const VideoParameters& params,
                           FrameBuffer& frame,
                           std::string& error_message) {
    std::vector<FrameBuffer> frames;
    if (!load_frames(frame_number, 1, expected_width, expected_height, params, frames, error_message)) {
        return false;
    }
    
    if (frames.empty()) {
        error_message = "No frame was loaded";
        return false;
    }
    
    frame = std::move(frames[0]);
    return true;
}

bool PNGLoader::load_frames(int32_t start_frame,
                            int32_t num_frames,
                            int32_t expected_width,
                            int32_t expected_height,
                            const VideoParameters& params,
                            std::vector<FrameBuffer>& frames,
                            std::string& error_message) {
    (void)params; // PNG images are single frames without frame-rate constraints
    if (!is_open_) {
        error_message = "PNG file is not open";
        return false;
    }
    
    // Validate dimensions
    std::string dim_error;
    if (!validate_dimensions(expected_width, expected_height, dim_error)) {
        error_message = dim_error;
        return false;
    }
    
    // Validate frame range (PNG only has frame 0)
    if (start_frame != 0 || num_frames != 1) {
        error_message = "PNG loader only supports loading frame 0 (single frame image)";
        return false;
    }
    
    frames.clear();
    
    // Load PNG if not already cached
    if (!frame_loaded_) {
        try {
            FILE* fp = std::fopen(filename_.c_str(), "rb");
            if (!fp) {
                error_message = "Cannot open PNG file: " + filename_;
                return false;
            }

            png_structp png_ptr = png_create_read_struct(PNG_LIBPNG_VER_STRING, nullptr, png_read_error, nullptr);
            if (!png_ptr) {
                std::fclose(fp);
                error_message = "Failed to create PNG read struct";
                return false;
            }
            png_infop info_ptr = png_create_info_struct(png_ptr);
            if (!info_ptr) {
                png_destroy_read_struct(&png_ptr, nullptr, nullptr);
                std::fclose(fp);
                error_message = "Failed to create PNG info struct";
                return false;
            }

            if (setjmp(png_jmpbuf(png_ptr))) {
                png_destroy_read_struct(&png_ptr, &info_ptr, nullptr);
                std::fclose(fp);
                error_message = "libpng read error";
                return false;
            }

            png_init_io(png_ptr, fp);
            png_read_info(png_ptr, info_ptr);

            png_uint_32 w, h;
            int bit_depth, color_type;
            png_get_IHDR(png_ptr, info_ptr, &w, &h, &bit_depth, &color_type, nullptr, nullptr, nullptr);

            // Normalize to 8-bit RGB
            if (color_type == PNG_COLOR_TYPE_PALETTE) {
                png_set_palette_to_rgb(png_ptr);
            }
            if (color_type == PNG_COLOR_TYPE_GRAY && bit_depth < 8) {
                png_set_expand_gray_1_2_4_to_8(png_ptr);
            }
            if (png_get_valid(png_ptr, info_ptr, PNG_INFO_tRNS)) {
                png_set_tRNS_to_alpha(png_ptr);
            }
            if (bit_depth == 16) {
                png_set_strip_16(png_ptr);
            }
            if (color_type == PNG_COLOR_TYPE_GRAY || color_type == PNG_COLOR_TYPE_GRAY_ALPHA) {
                png_set_gray_to_rgb(png_ptr);
            }
            if (color_type == PNG_COLOR_TYPE_RGB_ALPHA) {
                png_set_strip_alpha(png_ptr);
            }

            png_read_update_info(png_ptr, info_ptr);

            // Read rows
            std::vector<png_bytep> row_pointers(h);
            std::vector<uint8_t> image_data(w * h * 3);
            for (png_uint_32 y = 0; y < h; ++y) {
                row_pointers[y] = image_data.data() + y * (w * 3);
            }
            png_read_image(png_ptr, row_pointers.data());
            png_read_end(png_ptr, nullptr);
            png_destroy_read_struct(&png_ptr, &info_ptr, nullptr);
            std::fclose(fp);

            // Convert to YUV444P16
            cached_frame_.resize(static_cast<int32_t>(w), static_cast<int32_t>(h), 
                                FrameBuffer::Format::YUV444P16);
            rgb_to_yuv444p16(image_data.data(), static_cast<int32_t>(w), static_cast<int32_t>(h),
                            cached_frame_);
            // Frame is now in normalized studio range (Y:64-940, U/V:0-896)
            // No additional luma remapping needed; downstream encoder expects this range
            frame_loaded_ = true;
        } catch (const std::exception& e) {
            error_message = std::string("Error loading PNG file: ") + e.what();
            return false;
        }
    }
    
    frames.push_back(cached_frame_);
    return true;
}

void PNGLoader::close() {
    is_open_ = false;
    filename_.clear();
    width_ = 0;
    height_ = 0;
    frame_count_ = 0;
    frame_loaded_ = false;
    cached_frame_ = FrameBuffer();
}

bool PNGLoader::validate_format(VideoSystem system, std::string& error_message) {
    (void)system;
    (void)error_message;
    return true;
}

void PNGLoader::rgb_to_yuv444p16(const uint8_t* rgb_data, int32_t width, int32_t height,
                                 FrameBuffer& frame) {
    uint16_t* y_plane = frame.data().data();
    uint16_t* u_plane = y_plane + (width * height);
    uint16_t* v_plane = u_plane + (width * height);

    for (int32_t i = 0; i < width * height; ++i) {
        uint8_t r8 = rgb_data[i * 3];
        uint8_t g8 = rgb_data[i * 3 + 1];
        uint8_t b8 = rgb_data[i * 3 + 2];
        
        // Convert RGB (0-255) directly to studio-range YUV using BT.601 coefficients
        // Studio range: Y: 64-940, U/V: 0-896 (in 10-bit, 0-1023 scale)
        // Coefficients for direct conversion to studio range:
        // Y = 64 + (219/255) * (0.299*R + 0.587*G + 0.114*B)
        // U = 512 + (224/255) * (-0.147*R - 0.289*G + 0.436*B)
        // V = 512 + (224/255) * (0.615*R - 0.515*G - 0.100*B)
        
        double r_val = static_cast<double>(r8) / 255.0;
        double g_val = static_cast<double>(g8) / 255.0;
        double b_val = static_cast<double>(b8) / 255.0;
        
        // Calculate luma in studio range (64-940 in 10-bit normalized to 0-1)
        double y_studio = 0.299 * r_val + 0.587 * g_val + 0.114 * b_val;
        y_studio = 64.0 + y_studio * 876.0;  // 876 = 940 - 64
        y_plane[i] = static_cast<uint16_t>(std::clamp(y_studio, 64.0, 940.0));
        
        // Calculate chroma in studio range (0-896 in normalized 10-bit)
        double u_studio = -0.147 * r_val - 0.289 * g_val + 0.436 * b_val;
        u_studio = 448.0 + u_studio * 896.0;  // Center at 448, range 0-896
        u_plane[i] = static_cast<uint16_t>(std::clamp(u_studio, 0.0, 896.0));
        
        double v_studio = 0.615 * r_val - 0.515 * g_val - 0.100 * b_val;
        v_studio = 448.0 + v_studio * 896.0;  // Center at 448, range 0-896
        v_plane[i] = static_cast<uint16_t>(std::clamp(v_studio, 0.0, 896.0));
    }
}

} // namespace encode_orc
