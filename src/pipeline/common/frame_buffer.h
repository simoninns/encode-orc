/*
 * File:        frame_buffer.h
 * Module:      encode-orc
 * Purpose:     RGB and YUV frame buffer management
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: 2026 Simon Inns
 */

#ifndef ENCODE_ORC_FRAME_BUFFER_H
#define ENCODE_ORC_FRAME_BUFFER_H

#include <cstdint>
#include <vector>
#include <stdexcept>

namespace encode_orc {

/**
 * @brief RGB pixel format (16-bit per component)
 */
struct RGB48Pixel {
    uint16_t r;
    uint16_t g;
    uint16_t b;
};

/**
 * @brief YUV pixel format (16-bit per component)
 */
struct YUV444P16Pixel {
    uint16_t y;
    uint16_t u;
    uint16_t v;
};

/**
 * @brief Input frame buffer for RGB or YUV data
 * 
 * This class stores a single progressive frame of video data in RGB48 or YUV444P16 format.
 * The data layout is planar for YUV (Y plane, then U plane, then V plane) and
 * interleaved for RGB.
 */
class FrameBuffer {
public:
    /**
     * @brief Pixel format type
     */
    enum class Format {
        RGB48,      // 16-bit RGB, interleaved (R, G, B, R, G, B, ...)
        YUV444P16   // 16-bit YUV, planar (Y..., U..., V...)
    };
    
    /**
     * @brief Construct an empty frame buffer
     */
    FrameBuffer() = default;
    
    /**
     * @brief Construct a frame buffer with specified dimensions and format
     * @param width Frame width in pixels
     * @param height Frame height in lines
     * @param format Pixel format (RGB48 or YUV444P16)
     */
    FrameBuffer(int32_t width, int32_t height, Format format = Format::RGB48)
        : width_(width), height_(height), format_(format) {
        resize(width, height, format);
    }
    
    /**
     * @brief Get frame width in pixels
     */
    int32_t width() const { return width_; }
    
    /**
     * @brief Get frame height in lines
     */
    int32_t height() const { return height_; }
    
    /**
     * @brief Get pixel format
     */
    Format format() const { return format_; }
    
    /**
     * @brief Get total number of samples (components)
     */
    size_t size() const { return data_.size(); }
    
    /**
     * @brief Access raw data (mutable)
     */
    std::vector<uint16_t>& data() { return data_; }
    
    /**
     * @brief Access raw data (const)
     */
    const std::vector<uint16_t>& data() const { return data_; }
    
    /**
     * @brief Get pointer to raw data
     */
    uint16_t* data_ptr() { return data_.data(); }
    
    /**
     * @brief Get const pointer to raw data
     */
    const uint16_t* data_ptr() const { return data_.data(); }
    
    /**
     * @brief Resize the frame buffer
     * @param width New width in pixels
     * @param height New height in lines
     * @param format Pixel format
     */
    void resize(int32_t width, int32_t height, Format format = Format::RGB48) {
        width_ = width;
        height_ = height;
        format_ = format;
        
        // For both RGB48 and YUV444P16, we need 3 components per pixel
        data_.resize(width * height * 3);
    }
    
    /**
     * @brief Set an RGB pixel
     * @param x Horizontal position (0-indexed)
     * @param y Vertical position (0-indexed)
     * @param r Red component (0-65535)
     * @param g Green component (0-65535)
     * @param b Blue component (0-65535)
     */
    void set_rgb_pixel(int32_t x, int32_t y, uint16_t r, uint16_t g, uint16_t b) {
        if (format_ != Format::RGB48) {
            throw std::runtime_error("Frame buffer is not in RGB48 format");
        }
        int32_t index = (y * width_ + x) * 3;
        data_[index] = r;
        data_[index + 1] = g;
        data_[index + 2] = b;
    }
    
    /**
     * @brief Get an RGB pixel
     * @param x Horizontal position (0-indexed)
     * @param y Vertical position (0-indexed)
     */
    RGB48Pixel get_rgb_pixel(int32_t x, int32_t y) const {
        if (format_ != Format::RGB48) {
            throw std::runtime_error("Frame buffer is not in RGB48 format");
        }
        int32_t index = (y * width_ + x) * 3;
        return {data_[index], data_[index + 1], data_[index + 2]};
    }
    
    /**
     * @brief Set a YUV pixel (planar layout)
     * @param x Horizontal position (0-indexed)
     * @param y Vertical position (0-indexed)
     * @param Y Luma component (0-65535)
     * @param U U (Cb) component (0-65535)
     * @param V V (Cr) component (0-65535)
     */
    void set_yuv_pixel(int32_t x, int32_t y, uint16_t Y, uint16_t U, uint16_t V) {
        if (format_ != Format::YUV444P16) {
            throw std::runtime_error("Frame buffer is not in YUV444P16 format");
        }
        int32_t pixel_count = width_ * height_;
        int32_t index = y * width_ + x;
        data_[index] = Y;
        data_[pixel_count + index] = U;
        data_[pixel_count * 2 + index] = V;
    }
    
    /**
     * @brief Get a YUV pixel (planar layout)
     * @param x Horizontal position (0-indexed)
     * @param y Vertical position (0-indexed)
     */
    YUV444P16Pixel get_yuv_pixel(int32_t x, int32_t y) const {
        if (format_ != Format::YUV444P16) {
            throw std::runtime_error("Frame buffer is not in YUV444P16 format");
        }
        int32_t pixel_count = width_ * height_;
        int32_t index = y * width_ + x;
        return {data_[index], data_[pixel_count + index], data_[pixel_count * 2 + index]};
    }
    
    /**
     * @brief Fill entire buffer with a constant value
     * @param value Value to fill with
     */
    void fill(uint16_t value) {
        std::fill(data_.begin(), data_.end(), value);
    }
    
    /**
     * @brief Clear the buffer (set all values to 0)
     */
    void clear() {
        std::fill(data_.begin(), data_.end(), 0);
    }

private:
    int32_t width_ = 0;
    int32_t height_ = 0;
    Format format_ = Format::RGB48;
    std::vector<uint16_t> data_;
};

} // namespace encode_orc

#endif // ENCODE_ORC_FRAME_BUFFER_H
