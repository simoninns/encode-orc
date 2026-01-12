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
        params.sample_rate = 4.0 * params.fSC;
        params.field_width = 1135;
        params.field_height = 313;
        params.colour_burst_start = 98;
        params.colour_burst_end = 138;
        params.active_video_start = 185;
        params.active_video_end = 1107;
        params.white_16b_ire = 0xD300;
        params.black_16b_ire = 0x4000;
        params.blanking_16b_ire = 0x4000;
        params.is_subcarrier_locked = true;
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
        params.sample_rate = 4.0 * params.fSC;
        params.field_width = 910;
        params.field_height = 263;
        params.colour_burst_start = 89;  // Approximate (19 cycles after 0H)
        params.colour_burst_end = 125;   // 9 cycles duration
        params.active_video_start = 152;
        params.active_video_end = 910;
        params.white_16b_ire = 0xC800;
        params.black_16b_ire = 0x4680;   // With 7.5 IRE setup
        params.blanking_16b_ire = 0x3C00;
        params.is_subcarrier_locked = true;
        params.is_mapped = false;
        params.is_widescreen = false;
        return params;
    }
};

} // namespace encode_orc

#endif // ENCODE_ORC_VIDEO_PARAMETERS_H
