/*
 * File:        color_conversion.h
 * Module:      encode-orc
 * Purpose:     Color space conversion utilities
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: 2026 Simon Inns
 */

#ifndef ENCODE_ORC_COLOR_CONVERSION_H
#define ENCODE_ORC_COLOR_CONVERSION_H

#include "frame_buffer.h"
#include <cstdint>

namespace encode_orc {

/**
 * @brief Color space conversion utilities
 * 
 * Provides conversion between RGB and YUV color spaces for PAL/NTSC encoding.
 */
class ColorConverter {
public:
    /**
     * @brief Convert RGB frame to YUV (PAL/SECAM - Rec. 601)
     * @param rgb_frame Input frame in RGB48 format
     * @return Output frame in YUV444P16 format
     * 
     * Uses ITU-R BT.601 (PAL/SECAM) matrix:
     * Y  =  0.299*R + 0.587*G + 0.114*B
     * U  = -0.147*R - 0.289*G + 0.436*B + 0.5
     * V  =  0.615*R - 0.515*G - 0.100*B + 0.5
     */
    static FrameBuffer rgb_to_yuv_pal(const FrameBuffer& rgb_frame);
    
    /**
     * @brief Convert RGB frame to YIQ (NTSC)
     * @param rgb_frame Input frame in RGB48 format
     * @return Output frame in YUV444P16 format (actually YIQ, but same storage)
     * 
     * Uses NTSC YIQ matrix:
     * Y =  0.299*R + 0.587*G + 0.114*B
     * I =  0.596*R - 0.275*G - 0.321*B + 0.5
     * Q =  0.212*R - 0.523*G + 0.311*B + 0.5
     */
    static FrameBuffer rgb_to_yiq_ntsc(const FrameBuffer& rgb_frame);
    
    /**
     * @brief Convert YUV frame to RGB (PAL/SECAM - Rec. 601)
     * @param yuv_frame Input frame in YUV444P16 format
     * @return Output frame in RGB48 format
     * 
     * Uses ITU-R BT.601 (PAL/SECAM) inverse matrix
     */
    static FrameBuffer yuv_to_rgb_pal(const FrameBuffer& yuv_frame);
    
    /**
     * @brief Convert YIQ frame to RGB (NTSC)
     * @param yiq_frame Input frame in YUV444P16 format (actually YIQ)
     * @return Output frame in RGB48 format
     */
    static FrameBuffer yiq_to_rgb_ntsc(const FrameBuffer& yiq_frame);

private:
    /**
     * @brief Clamp value to 16-bit unsigned range
     */
    static uint16_t clamp_to_16bit(double value) {
        if (value < 0.0) return 0;
        if (value > 65535.0) return 65535;
        return static_cast<uint16_t>(value + 0.5);  // Round to nearest
    }
    
    /**
     * @brief Clamp normalized value (0.0-1.0) to 16-bit range
     */
    static uint16_t clamp_normalized(double value) {
        if (value < 0.0) value = 0.0;
        if (value > 1.0) value = 1.0;
        return static_cast<uint16_t>(value * 65535.0 + 0.5);
    }
};

} // namespace encode_orc

#endif // ENCODE_ORC_COLOR_CONVERSION_H
