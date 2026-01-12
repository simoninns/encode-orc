/*
 * File:        color_conversion.cpp
 * Module:      encode-orc
 * Purpose:     Color space conversion utilities implementation
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: 2026 Simon Inns
 */

#include "color_conversion.h"
#include <stdexcept>

namespace encode_orc {

FrameBuffer ColorConverter::rgb_to_yuv_pal(const FrameBuffer& rgb_frame) {
    if (rgb_frame.format() != FrameBuffer::Format::RGB48) {
        throw std::runtime_error("Input frame must be in RGB48 format");
    }
    
    int32_t width = rgb_frame.width();
    int32_t height = rgb_frame.height();
    
    // Create output YUV frame
    FrameBuffer yuv_frame(width, height, FrameBuffer::Format::YUV444P16);
    
    // PAL/SECAM (Rec. 601) coefficients - must match ld-chroma-encoder exactly
    // for proper chroma decoder compatibility
    const double Y_R = 0.299;
    const double Y_G = 0.587;
    const double Y_B = 0.114;
    
    // U and V coefficients from ld-chroma-encoder/palencoder.cpp
    const double U_R = -0.147141;
    const double U_G = -0.288869;
    const double U_B = 0.436010;
    
    const double V_R = 0.614975;
    const double V_G = -0.514965;
    const double V_B = -0.100010;
    
    // Maximum ranges for U and V (determined by the coefficients above)
    // U: -0.436010 to +0.436010
    // V: -0.614975 to +0.614975
    const double U_MAX = 0.436010;
    const double V_MAX = 0.614975;
    
    // Convert each pixel
    for (int32_t y = 0; y < height; ++y) {
        for (int32_t x = 0; x < width; ++x) {
            // Get RGB pixel
            RGB48Pixel rgb = rgb_frame.get_rgb_pixel(x, y);
            
            // Normalize RGB to 0.0-1.0
            double r = static_cast<double>(rgb.r) / 65535.0;
            double g = static_cast<double>(rgb.g) / 65535.0;
            double b = static_cast<double>(rgb.b) / 65535.0;
            
            // Compute Y, U, V using PAL/SECAM coefficients
            double y_val = Y_R * r + Y_G * g + Y_B * b;
            double u_val = U_R * r + U_G * g + U_B * b;  // Range: -0.436 to +0.436
            double v_val = V_R * r + V_G * g + V_B * b;  // Range: -0.615 to +0.615
            
            // Convert to 16-bit:
            // Y: 0.0-1.0 → 0x0000-0xFFFF
            // U, V: scale using actual max values to 0x0000-0xFFFF range
            //       Map from [-MAX, +MAX] to [0.0, 1.0], then to 16-bit
            uint16_t y_16 = clamp_normalized(y_val);
            uint16_t u_16 = clamp_normalized((u_val / U_MAX) * 0.5 + 0.5);  // Map to 0-1
            uint16_t v_16 = clamp_normalized((v_val / V_MAX) * 0.5 + 0.5);  // Map to 0-1
            
            // Set YUV pixel
            yuv_frame.set_yuv_pixel(x, y, y_16, u_16, v_16);
        }
    }
    
    return yuv_frame;
}

FrameBuffer ColorConverter::rgb_to_yiq_ntsc(const FrameBuffer& rgb_frame) {
    if (rgb_frame.format() != FrameBuffer::Format::RGB48) {
        throw std::runtime_error("Input frame must be in RGB48 format");
    }
    
    int32_t width = rgb_frame.width();
    int32_t height = rgb_frame.height();
    
    // Create output YIQ frame (stored in YUV format structure)
    FrameBuffer yiq_frame(width, height, FrameBuffer::Format::YUV444P16);
    
    // NTSC YIQ coefficients
    const double Ky = 0.299;
    const double Kyg = 0.587;
    const double Kyb = 0.114;
    
    // Convert each pixel
    for (int32_t y = 0; y < height; ++y) {
        for (int32_t x = 0; x < width; ++x) {
            // Get RGB pixel
            RGB48Pixel rgb = rgb_frame.get_rgb_pixel(x, y);
            
            // Normalize RGB to 0.0-1.0
            double r = static_cast<double>(rgb.r) / 65535.0;
            double g = static_cast<double>(rgb.g) / 65535.0;
            double b = static_cast<double>(rgb.b) / 65535.0;
            
            // Convert to YIQ
            double y_val = Ky * r + Kyg * g + Kyb * b;
            double i_val = 0.596 * r - 0.275 * g - 0.321 * b + 0.5;
            double q_val = 0.212 * r - 0.523 * g + 0.311 * b + 0.5;
            
            // Clamp and convert to 16-bit
            uint16_t y_16 = clamp_normalized(y_val);
            uint16_t i_16 = clamp_normalized(i_val);
            uint16_t q_16 = clamp_normalized(q_val);
            
            // Set YIQ pixel (stored as YUV)
            yiq_frame.set_yuv_pixel(x, y, y_16, i_16, q_16);
        }
    }
    
    return yiq_frame;
}

FrameBuffer ColorConverter::yuv_to_rgb_pal(const FrameBuffer& yuv_frame) {
    if (yuv_frame.format() != FrameBuffer::Format::YUV444P16) {
        throw std::runtime_error("Input frame must be in YUV444P16 format");
    }
    
    int32_t width = yuv_frame.width();
    int32_t height = yuv_frame.height();
    
    // Create output RGB frame
    FrameBuffer rgb_frame(width, height, FrameBuffer::Format::RGB48);
    
    // ITU-R BT.601 (PAL/SECAM) coefficients
    const double Kr = 0.299;
    const double Kb = 0.114;
    
    // Convert each pixel
    for (int32_t y = 0; y < height; ++y) {
        for (int32_t x = 0; x < width; ++x) {
            // Get YUV pixel
            YUV444P16Pixel yuv = yuv_frame.get_yuv_pixel(x, y);
            
            // Normalize YUV to 0.0-1.0
            double y_val = static_cast<double>(yuv.y) / 65535.0;
            double u_val = static_cast<double>(yuv.u) / 65535.0 - 0.5;
            double v_val = static_cast<double>(yuv.v) / 65535.0 - 0.5;
            
            // Convert to RGB (Rec. 601 inverse)
            double r = y_val + v_val * 2.0 * (1.0 - Kr);
            double g = y_val - u_val * 2.0 * Kb * (1.0 - Kb) / (1.0 - Kr - Kb) 
                            - v_val * 2.0 * Kr * (1.0 - Kr) / (1.0 - Kr - Kb);
            double b = y_val + u_val * 2.0 * (1.0 - Kb);
            
            // Clamp and convert to 16-bit
            uint16_t r_16 = clamp_normalized(r);
            uint16_t g_16 = clamp_normalized(g);
            uint16_t b_16 = clamp_normalized(b);
            
            // Set RGB pixel
            rgb_frame.set_rgb_pixel(x, y, r_16, g_16, b_16);
        }
    }
    
    return rgb_frame;
}

FrameBuffer ColorConverter::yiq_to_rgb_ntsc(const FrameBuffer& yiq_frame) {
    if (yiq_frame.format() != FrameBuffer::Format::YUV444P16) {
        throw std::runtime_error("Input frame must be in YUV444P16 format (YIQ)");
    }
    
    int32_t width = yiq_frame.width();
    int32_t height = yiq_frame.height();
    
    // Create output RGB frame
    FrameBuffer rgb_frame(width, height, FrameBuffer::Format::RGB48);
    
    // Convert each pixel
    for (int32_t y = 0; y < height; ++y) {
        for (int32_t x = 0; x < width; ++x) {
            // Get YIQ pixel (stored as YUV)
            YUV444P16Pixel yiq = yiq_frame.get_yuv_pixel(x, y);
            
            // Normalize YIQ to proper range
            double y_val = static_cast<double>(yiq.y) / 65535.0;
            double i_val = static_cast<double>(yiq.u) / 65535.0 - 0.5;
            double q_val = static_cast<double>(yiq.v) / 65535.0 - 0.5;
            
            // Convert to RGB (NTSC YIQ inverse matrix)
            double r = y_val + 0.956 * i_val + 0.621 * q_val;
            double g = y_val - 0.272 * i_val - 0.647 * q_val;
            double b = y_val - 1.106 * i_val + 1.703 * q_val;
            
            // Clamp and convert to 16-bit
            uint16_t r_16 = clamp_normalized(r);
            uint16_t g_16 = clamp_normalized(g);
            uint16_t b_16 = clamp_normalized(b);
            
            // Set RGB pixel
            rgb_frame.set_rgb_pixel(x, y, r_16, g_16, b_16);
        }
    }
    
    return rgb_frame;
}

} // namespace encode_orc
