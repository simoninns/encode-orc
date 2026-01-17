/*
 * File:        png_loader.cpp
 * Module:      encode-orc
 * Purpose:     PNG image loading implementation using libpng
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: 2026 Simon Inns
 */

#include "png_loader.h"
#include "color_conversion.h"
#include <png.h>
#include <cstdio>
#include <vector>
#include <stdexcept>

namespace encode_orc {

static void png_read_error(png_structp png_ptr, png_const_charp msg) {
    (void)png_ptr;
    throw std::runtime_error(std::string("libpng error: ") + msg);
}

bool PNGLoader::load_png(const std::string& filename, const VideoParameters& params,
                         FrameBuffer& frame, std::string& error_message) {
    try {
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
            // libpng will jump here on error
            png_destroy_read_struct(&png_ptr, &info_ptr, nullptr);
            std::fclose(fp);
            error_message = "libpng read error";
            return false;
        }

        png_init_io(png_ptr, fp);
        png_read_info(png_ptr, info_ptr);

        png_uint_32 width, height;
        int bit_depth, color_type;
        png_get_IHDR(png_ptr, info_ptr, &width, &height, &bit_depth, &color_type, nullptr, nullptr, nullptr);

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
            // Reduce 16-bit to 8-bit for simplicity; we'll scale to 16-bit later
            png_set_strip_16(png_ptr);
        }
        if (color_type == PNG_COLOR_TYPE_GRAY || color_type == PNG_COLOR_TYPE_GRAY_ALPHA) {
            png_set_gray_to_rgb(png_ptr);
        }
        if (color_type == PNG_COLOR_TYPE_RGB_ALPHA) {
            // Remove alpha by background composition (assume opaque)
            png_set_strip_alpha(png_ptr);
        }

        png_read_update_info(png_ptr, info_ptr);

        // Validate expected dimensions
        int32_t expected_w, expected_h;
        get_expected_dimensions(params, expected_w, expected_h);
        if ((int32_t)width != expected_w || (int32_t)height != expected_h) {
            // Allow but warn; we will still proceed by requiring exact match
            png_destroy_read_struct(&png_ptr, &info_ptr, nullptr);
            std::fclose(fp);
            error_message = "PNG dimensions mismatch: expected " + std::to_string(expected_w) + "x" + std::to_string(expected_h) +
                           ", got " + std::to_string(width) + "x" + std::to_string(height);
            return false;
        }

        // Read rows
        std::vector<png_bytep> row_pointers(height);
        std::vector<uint8_t> image_data(width * height * 3);
        for (png_uint_32 y = 0; y < height; ++y) {
            row_pointers[y] = image_data.data() + y * (width * 3);
        }
        png_read_image(png_ptr, row_pointers.data());
        png_read_end(png_ptr, nullptr);
        png_destroy_read_struct(&png_ptr, &info_ptr, nullptr);
        std::fclose(fp);

        // Convert to YUV444P16
        frame.resize((int32_t)width, (int32_t)height, FrameBuffer::Format::YUV444P16);
        rgb_to_yuv444p16(image_data.data(), (int32_t)width, (int32_t)height, frame);

        return true;
    } catch (const std::exception& e) {
        error_message = e.what();
        return false;
    }
}

bool PNGLoader::get_png_dimensions(const std::string& filename, int32_t& width, 
                                   int32_t& height, std::string& error_message) {
    try {
        FILE* fp = std::fopen(filename.c_str(), "rb");
        if (!fp) {
            error_message = "Cannot open PNG file: " + filename;
            return false;
        }
        png_structp png_ptr = png_create_read_struct(PNG_LIBPNG_VER_STRING, nullptr, png_read_error, nullptr);
        if (!png_ptr) { std::fclose(fp); error_message = "Failed to create PNG read struct"; return false; }
        png_infop info_ptr = png_create_info_struct(png_ptr);
        if (!info_ptr) { png_destroy_read_struct(&png_ptr, nullptr, nullptr); std::fclose(fp); error_message = "Failed to create PNG info struct"; return false; }
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
        width = static_cast<int32_t>(w);
        height = static_cast<int32_t>(h);
        return true;
    } catch (const std::exception& e) {
        error_message = e.what();
        return false;
    }
}

void PNGLoader::get_expected_dimensions(const VideoParameters& params, int32_t& width, int32_t& height) {
    if (params.system == VideoSystem::PAL) {
        width = 720;
        height = 576;
    } else {
        width = 720;
        // Use 480 to match common NTSC digital image assets
        height = 480;
    }
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
        // Scale 8-bit to 16-bit
        uint16_t r16 = static_cast<uint16_t>(r8) * 257; // 255 -> 65535
        uint16_t g16 = static_cast<uint16_t>(g8) * 257;
        uint16_t b16 = static_cast<uint16_t>(b8) * 257;
        uint16_t y, u, v;
        ColorConverter::rgb_to_yuv_pixel(r16, g16, b16, y, u, v);
        y_plane[i] = y;
        u_plane[i] = u;
        v_plane[i] = v;
    }
}

} // namespace encode_orc
