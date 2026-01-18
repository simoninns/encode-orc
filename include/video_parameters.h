/*
 * File:        video_parameters.h
 * Module:      encode-orc
 * Purpose:     Video system parameters and configuration
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: 2026 Simon Inns
 */

#ifndef ENCODE_ORC_VIDEO_PARAMETERS_H
#define ENCODE_ORC_VIDEO_PARAMETERS_H

#include <cstdint>
#include <string>
#include <optional>

namespace encode_orc {

/**
 * @brief Video system types
 */
enum class VideoSystem {
    PAL,      // 625-line PAL (Europe, Australia, etc.)
    NTSC,     // 525-line NTSC (North America, Japan, etc.)
    PAL_M     // 525-line PAL (Brazil)
};

/**
 * @brief Convert VideoSystem enum to string
 */
inline std::string video_system_to_string(VideoSystem system) {
    switch (system) {
        case VideoSystem::PAL: return "PAL";
        case VideoSystem::NTSC: return "NTSC";
        case VideoSystem::PAL_M: return "PAL_M";
        default: return "UNKNOWN";
    }
}

/**
 * @brief Video parameters matching ld-decode's VideoParameters structure
 * 
 * This structure contains all the technical parameters needed to describe
 * the video signal format, timing, and sample layout.
 */
struct VideoParameters {
    // System and decoder identification
    VideoSystem system = VideoSystem::PAL;
    std::string decoder = "encode-orc";
    
    // Timing parameters
    double fSC = 0.0;           // Subcarrier frequency in Hz
    double sample_rate = 0.0;   // Sample rate in Hz (typically 4 * fSC)
    
    // Field/frame dimensions
    int32_t field_width = 0;    // Width of each field in samples
    int32_t field_height = 0;   // Height of each field in lines
    int32_t number_of_sequential_fields = 0;
    
    // Active video region (in samples)
    int32_t active_video_start = 0;
    int32_t active_video_end = 0;
    
    // Color burst region (in samples)
    int32_t colour_burst_start = 0;
    int32_t colour_burst_end = 0;
    
    // Video signal levels (16-bit scale)
    int32_t white_16b_ire = 0;
    int32_t black_16b_ire = 0;
    int32_t blanking_16b_ire = 0;
    
    // Flags
    bool is_subcarrier_locked = false;  // true = subcarrier-locked, false = line-locked
    bool is_mapped = false;              // true if processed by ld-discmap
    bool is_widescreen = false;          // true for 16:9, false for 4:3
    
    /**
     * @brief Initialize PAL parameters (subcarrier-locked)
     */
    static VideoParameters create_pal_composite() {
        VideoParameters params;
        params.system = VideoSystem::PAL;
        params.fSC = 4433618.75;  // PAL subcarrier frequency
        params.sample_rate = 17734475.0;  // Exact 4×fSC for ld-decode compatibility
        params.field_width = 1135;
        params.field_height = 313;
        params.colour_burst_start = 98;
        params.colour_burst_end = 138;
        params.active_video_start = 185;
        params.active_video_end = 1107;
        
        // PAL signal levels (16-bit scale):
        // Sync tip: -300mV → 0x0000 (0)
        // Blanking: 0mV → 0x42E5 (17125)
        // Black: 0mV → 0x42E5 (17125)
        // White: 700mV → 0xD300 (54016)
        // Absolute max with chroma: 903.3mV → ~0xFFFF
        params.white_16b_ire = 0xD300;      // 54016 (peak white)
        params.black_16b_ire = 0x42E5;      // 17125
        params.blanking_16b_ire = 0x42E5;   // 17125
        params.is_subcarrier_locked = false;
        params.is_mapped = false;
        params.is_widescreen = false;
        return params;
    }
    
    /**
     * @brief Initialize NTSC parameters (subcarrier-locked)
     */
    static VideoParameters create_ntsc_composite() {
        VideoParameters params;
        params.system = VideoSystem::NTSC;
        params.fSC = 315.0e6 / 88.0;  // NTSC subcarrier frequency
        params.sample_rate = 14318181.818181818;  // Exact 4×fSC for ld-decode compatibility
        params.field_width = 910;
        params.field_height = 263;
        params.colour_burst_start = 74;   // Measured from real NTSC TBCs
        params.colour_burst_end = 110;    // Measured from real NTSC TBCs
        params.active_video_start = 134;  // Measured from real NTSC TBCs
        params.active_video_end = 894;    // Measured from real NTSC TBCs (leaves front porch)
        params.white_16b_ire = 0xC800;      // 51200
        params.black_16b_ire = 0x4568;      // 17768
        params.blanking_16b_ire = 0x3AD2;   // 15058
        params.is_subcarrier_locked = false;
        params.is_mapped = false;
        params.is_widescreen = false;
        return params;
    }
    
    /**
     * @brief Apply video level overrides to parameters
     * @param params VideoParameters to modify
     * @param blanking_override Optional blanking level override
     * @param black_override Optional black level override
     * @param white_override Optional white level override
     */
    static void apply_video_level_overrides(VideoParameters& params,
                                           std::optional<int32_t> blanking_override,
                                           std::optional<int32_t> black_override,
                                           std::optional<int32_t> white_override) {
        if (blanking_override.has_value()) {
            params.blanking_16b_ire = blanking_override.value();
        }
        if (black_override.has_value()) {
            params.black_16b_ire = black_override.value();
        }
        if (white_override.has_value()) {
            params.white_16b_ire = white_override.value();
        }
    }
};

} // namespace encode_orc

#endif // ENCODE_ORC_VIDEO_PARAMETERS_H
