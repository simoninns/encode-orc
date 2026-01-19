/*
 * File:        video_loader_base.h
 * Module:      encode-orc
 * Purpose:     Common base class and utilities for video loaders
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: 2026 Simon Inns
 */

#ifndef ENCODE_ORC_VIDEO_LOADER_BASE_H
#define ENCODE_ORC_VIDEO_LOADER_BASE_H

#include "frame_buffer.h"
#include "video_parameters.h"
#include <cstdint>
#include <algorithm>

namespace encode_orc {

/**
 * @brief Common utilities for video frame loaders
 * 
 * This class provides shared functionality for all video loader implementations:
 * - Color space conversion functions (luma and chroma normalization)
 * - Frame buffer padding and upsampling
 * - Studio range to normalized range conversion
 * - Common validation functions
 */
class VideoLoaderUtils {
public:
    // ========== Studio Range Constants ==========
    
    // 8-bit studio range (used in MP4, some raw files)
    static constexpr uint8_t STUDIO_LUMA_MIN_8BIT = 16;
    static constexpr uint8_t STUDIO_LUMA_MAX_8BIT = 235;
    static constexpr uint8_t STUDIO_CHROMA_MIN_8BIT = 16;
    static constexpr uint8_t STUDIO_CHROMA_MAX_8BIT = 240;
    static constexpr uint8_t STUDIO_CHROMA_NEUTRAL_8BIT = 128;
    
    // 10-bit studio range (used in MOV files, ProRes, v210)
    static constexpr uint16_t STUDIO_LUMA_MIN_10BIT = 64;
    static constexpr uint16_t STUDIO_LUMA_MAX_10BIT = 940;
    static constexpr uint16_t STUDIO_CHROMA_MIN_10BIT = 64;
    static constexpr uint16_t STUDIO_CHROMA_MAX_10BIT = 960;
    static constexpr uint16_t STUDIO_CHROMA_NEUTRAL_10BIT = 512;
    
    // Normalized output ranges (what encoder expects)
    static constexpr uint16_t NORMALIZED_LUMA_MIN_10BIT = 64;
    static constexpr uint16_t NORMALIZED_LUMA_MAX_10BIT = 940;
    static constexpr uint16_t NORMALIZED_CHROMA_MAX_10BIT = 896;
    static constexpr uint16_t NORMALIZED_CHROMA_NEUTRAL_10BIT = 448;
    
    // ========== Luma Conversion Functions ==========
    
    /**
     * @brief Convert 8-bit studio luma to 10-bit studio luma
     * 
     * Simple scaling: 8-bit value << 2 (multiply by 4)
     * Range: [16-235] → [64-940]
     */
    static uint16_t luma_8bit_to_10bit(uint8_t value) {
        return static_cast<uint16_t>(static_cast<uint16_t>(value) << 2);
    }
    
    /**
     * @brief Keep 10-bit studio luma (clamp to valid studio range)
     */
    static uint16_t luma_10bit_studio(uint16_t value) {
        return std::min<uint16_t>(value, 1023);
    }
    
    // ========== Chroma Conversion Functions ==========
    
    /**
     * @brief Convert 8-bit studio chroma to normalized 10-bit chroma
     * 
     * Conversion: (value - 16) * 4
     * Input studio range: [16-240] (neutral at 128)
     * Output normalized range: [0-896] (neutral at 448)
     */
    static uint16_t chroma_8bit_to_normalized(uint8_t studio_value) {
        uint8_t clamped = std::clamp(studio_value, STUDIO_CHROMA_MIN_8BIT, STUDIO_CHROMA_MAX_8BIT);
        int32_t delta = clamped - STUDIO_CHROMA_MIN_8BIT;
        int32_t scaled = delta * 4;
        return static_cast<uint16_t>(std::min(static_cast<int32_t>(NORMALIZED_CHROMA_MAX_10BIT), scaled));
    }
    
    /**
     * @brief Convert 10-bit studio chroma to normalized 10-bit chroma
     * 
     * Conversion: (value - 64) * 896 / 896 (which is just the delta)
     * Input studio range: [64-960] (neutral at 512)
     * Output normalized range: [0-896] (neutral at 448)
     */
    static uint16_t chroma_10bit_to_normalized(uint16_t studio_value) {
        uint16_t capped = std::min<uint16_t>(studio_value, STUDIO_CHROMA_MAX_10BIT);
        int32_t delta = capped - STUDIO_CHROMA_MIN_10BIT;
        int32_t scaled = (delta * NORMALIZED_CHROMA_MAX_10BIT) / NORMALIZED_CHROMA_MAX_10BIT;  // delta
        return static_cast<uint16_t>(std::min(static_cast<int32_t>(NORMALIZED_CHROMA_MAX_10BIT), scaled));
    }
    
    /**
     * @brief Keep 10-bit studio chroma as normalized form
     * 
     * This is used when chroma is already in the correct studio range
     * and we just need to ensure it's in normalized form (0-896).
     */
    static uint16_t chroma_10bit_studio_to_normalized(uint16_t studio_value) {
        return chroma_10bit_to_normalized(studio_value);
    }
    
    // ========== Frame Buffer Padding and Utilities ==========
    
    /**
     * @brief Pad and optionally upsample YUV frame buffer
     * 
     * Common operation for all loaders: input frames may be narrower than target
     * width (e.g., 360 instead of 720), requiring padding to the left and right.
     * Also handles chroma upsampling from 4:2:0 or 4:2:2 to 4:4:4.
     * 
     * @param target_width Target output width (usually 720 for PAL/NTSC)
     * @param actual_width Actual input width
     * @param height Frame height
     * @param frame Target frame buffer (pre-allocated)
     * @param y_plane Input Y plane data
     * @param u_plane Input U plane data
     * @param v_plane Input V plane data
     * @param chroma_h_factor Horizontal chroma subsampling (1=4:4:4, 2=4:2:2, 2=4:2:0)
     * @param chroma_v_factor Vertical chroma subsampling (1=4:4:4, 1=4:2:2, 2=4:2:0)
     * @param neutral_y Neutral Y value for padding
     * @param neutral_u Neutral U value for padding
     * @param neutral_v Neutral V value for padding
     */
    static void pad_and_upsample_yuv(int32_t target_width,
                                     int32_t actual_width,
                                     int32_t height,
                                     FrameBuffer& frame,
                                     const uint16_t* y_plane,
                                     const uint16_t* u_plane,
                                     const uint16_t* v_plane,
                                     int32_t chroma_h_factor = 1,
                                     int32_t chroma_v_factor = 1,
                                     uint16_t neutral_y = 64,
                                     uint16_t neutral_u = 448,
                                     uint16_t neutral_v = 448);
    
    /**
     * @brief Pad and optionally upsample YUV frame buffer (8-bit variant)
     * 
     * Same as above but for 8-bit input data.
     */
    static void pad_and_upsample_yuv_8bit(int32_t target_width,
                                          int32_t actual_width,
                                          int32_t height,
                                          FrameBuffer& frame,
                                          const uint8_t* y_plane,
                                          const uint8_t* u_plane,
                                          const uint8_t* v_plane,
                                          int32_t chroma_h_factor = 1,
                                          int32_t chroma_v_factor = 1,
                                          uint8_t neutral_y = 16,
                                          uint8_t neutral_u = 128,
                                          uint8_t neutral_v = 128);
    
    // ========== Format Validation ==========
    
    /**
     * @brief Validate frame rate matches video system
     * 
     * @param frame_rate Measured frame rate in Hz
     * @param system Target video system (PAL/NTSC)
     * @param tolerance_fps Allowed tolerance (default ±0.1 fps)
     * @return true if frame rate matches expected system
     */
    static bool validate_frame_rate(double frame_rate, VideoSystem system, 
                                    double tolerance_fps = 0.1);
    
    /**
     * @brief Get expected frame rate for video system
     * 
     * @param system Video system
     * @return Expected frame rate in Hz (25.0 for PAL, 29.97 for NTSC)
     */
    static double get_expected_frame_rate(VideoSystem system);
};

/**
 * @brief Base class for video file loaders
 * 
 * Provides common interface for all file-based video loaders.
 * Concrete implementations (MOVLoader, MP4Loader, etc.) inherit from this
 * and implement format-specific functionality.
 */
class VideoLoaderBase {
public:
    virtual ~VideoLoaderBase() = default;
    
    /**
     * @brief Get video dimensions
     */
    virtual bool get_dimensions(int32_t& width, int32_t& height) const = 0;
    
    /**
     * @brief Get total frame count
     */
    virtual int32_t get_frame_count() const = 0;
    
    /**
     * @brief Check if loader is open and ready
     */
    virtual bool is_open() const = 0;
    
    /**
     * @brief Validate loader dimensions match expectations
     * 
     * @param expected_width Expected width
     * @param expected_height Expected height
     * @param error_message Error message on mismatch
     * @return true if dimensions match, false otherwise
     */
    bool validate_dimensions(int32_t expected_width, int32_t expected_height,
                            std::string& error_message) const;
    
    /**
     * @brief Validate loader frame range is within bounds
     * 
     * @param start_frame Starting frame
     * @param num_frames Number of frames
     * @param error_message Error message if out of bounds
     * @return true if range is valid, false otherwise
     */
    bool validate_frame_range(int32_t start_frame, int32_t num_frames,
                             std::string& error_message) const;
    
    /**
     * @brief Validate frame rate matches video system
     * 
     * @param system Target video system
     * @param error_message Error/warning message
     * @return true if valid, false on error
     */
    virtual bool validate_format(VideoSystem system, std::string& error_message) = 0;

protected:
    int32_t width_ = 0;
    int32_t height_ = 0;
    int32_t frame_count_ = -1;
    double frame_rate_ = 0.0;
    bool is_open_ = false;
};

} // namespace encode_orc

#endif // ENCODE_ORC_VIDEO_LOADER_BASE_H
