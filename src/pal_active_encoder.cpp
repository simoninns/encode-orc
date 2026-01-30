/*
 * File:        pal_active_encoder.cpp
 * Module:      encode-orc
 * Purpose:     PAL active video encoder implementation
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: 2026 Simon Inns
 */

#include "pal_active_encoder.h"
#include <cmath>
#include <algorithm>
#include <cstring>

namespace encode_orc {

// Constants
static constexpr double PI = 3.14159265358979323846;

PALActiveEncoder::PALActiveEncoder(const VideoParameters& params,
                                   bool enable_chroma_filter,
                                   bool enable_luma_filter)
    : params_(params) {
    
    // Set signal levels
    sync_level_ = 0x0000;
    blanking_level_ = params_.blanking_16b_ire;
    black_level_ = params_.black_16b_ire;
    white_level_ = params_.white_16b_ire;
    
    // Set subcarrier parameters
    subcarrier_freq_ = params_.fSC;
    sample_rate_ = params_.sample_rate;
    samples_per_cycle_ = sample_rate_ / subcarrier_freq_;
    
    // Initialize filters if requested
    if (enable_chroma_filter) {
        chroma_filter_ = Filters::create_pal_uv_filter();
    }
    if (enable_luma_filter) {
        luma_filter_ = Filters::create_pal_uv_filter();
    }
}

int32_t PALActiveEncoder::calculate_v_switch(int32_t line_number, int32_t field_number, bool is_first_field) const {
    // Convert field line number to frame line number (1-625 in PAL)
    int32_t frame_line = is_first_field ? (line_number * 2 + 1) : (line_number * 2 + 2);
    
    // Calculate absolute line number in 8-field sequence
    int32_t field_id = field_number % 8;
    int32_t prev_lines = ((field_id / 2) * 625) + ((field_id % 2) * 313) + (frame_line / 2);
    
    // PAL V-switch alternates every line
    return (prev_lines % 2 == 0) ? 1 : -1;
}

double PALActiveEncoder::calculate_phase(int32_t line_number, int32_t field_number, bool is_first_field) const {
    // Convert field line number to frame line number (1-625 in PAL)
    int32_t frame_line = is_first_field ? (line_number * 2 + 1) : (line_number * 2 + 2);
    
    // Calculate absolute line number in 8-field sequence
    int32_t field_id = field_number % 8;
    int32_t prev_lines = ((field_id / 2) * 625) + ((field_id % 2) * 313) + (frame_line / 2);
    
    // PAL subcarrier phase calculation (following ld-chroma-encoder):
    // prevCycles = number of complete cycles since sequence start
    // phase = 2π * (fSC * t + prevCycles) where prevCycles accumulates by 283.7516 per line
    double prev_cycles = prev_lines * 283.7516;
    
    double phase_step = 2.0 * PI * (subcarrier_freq_ / sample_rate_);
    double phase = (2.0 * PI * prev_cycles) + static_cast<double>(params_.active_video_start) * phase_step;
    
    return phase;
}

uint16_t PALActiveEncoder::clamp_to_16bit(int32_t value) const {
    if (value < 0) return 0;
    if (value > 65535) return 65535;
    return static_cast<uint16_t>(value);
}

void PALActiveEncoder::encode_active_line(uint16_t* line_buffer,
                                          const uint16_t* y_line,
                                          const uint16_t* u_line,
                                          const uint16_t* v_line,
                                          int32_t line_number,
                                          int32_t field_number,
                                          bool is_first_field,
                                          int32_t width,
                                          bool studio_range_input) {
    // Resolve source pointers; only allocate filtered buffers when filters are enabled.
    const uint16_t* y_data = y_line;
    const uint16_t* u_data = u_line;
    const uint16_t* v_data = v_line;

    if (luma_filter_) {
        thread_local std::vector<uint16_t> y_filtered;
        y_filtered.resize(width);
        std::copy(y_line, y_line + width, y_filtered.begin());
        luma_filter_.value().apply(y_filtered);
        y_data = y_filtered.data();
    }

    if (chroma_filter_) {
        thread_local std::vector<uint16_t> u_filtered;
        thread_local std::vector<uint16_t> v_filtered;
        u_filtered.resize(width);
        v_filtered.resize(width);
        std::copy(u_line, u_line + width, u_filtered.begin());
        std::copy(v_line, v_line + width, v_filtered.begin());
        chroma_filter_.value().apply(u_filtered);
        chroma_filter_.value().apply(v_filtered);
        u_data = u_filtered.data();
        v_data = v_filtered.data();
    }
    
    // Get V-switch for this line
    int32_t v_switch = calculate_v_switch(line_number, field_number, is_first_field);
    
    // Calculate initial phase
    double phase = calculate_phase(line_number, field_number, is_first_field);
    double phase_step = 2.0 * PI * (subcarrier_freq_ / sample_rate_);
    
    double sin_phase = std::sin(phase);
    double cos_phase = std::cos(phase);
    const double sin_step = std::sin(phase_step);
    const double cos_step = std::cos(phase_step);

    // Scale active video portion
    int32_t active_start = params_.active_video_start;
    int32_t active_end = params_.active_video_end;
    int32_t active_width = active_end - active_start;
    
    const double pixel_step = static_cast<double>(width) / active_width;
    double pixel_pos = 0.0;

    for (int32_t sample = active_start; sample < active_end; ++sample) {
        int32_t pixel_x = static_cast<int32_t>(pixel_pos);
        pixel_pos += pixel_step;

        if (pixel_x >= width) pixel_x = width - 1;

        const uint16_t y = y_data[pixel_x];
        const uint16_t u = u_data[pixel_x];
        const uint16_t v = v_data[pixel_x];

        int32_t luma_range = white_level_ - black_level_;
        int32_t luma_scaled;

        if (studio_range_input) {
            // Preserve sub-black: don't clamp luma_scaled, allow negative values
            luma_scaled = black_level_ + ((static_cast<int32_t>(y) - 64) * luma_range) / 876;
        } else {
            double y_norm = static_cast<double>(y) / 65535.0;
            luma_scaled = black_level_ + static_cast<int32_t>(y_norm * luma_range);
        }

        // PAL chroma constants
        const double U_MAX = 0.436010;
        const double V_MAX = 0.614975;
        double u_norm;
        double v_norm;
        if (studio_range_input) {
            u_norm = ((static_cast<double>(u) / 896.0) - 0.5) * 2.0 * U_MAX;
            v_norm = ((static_cast<double>(v) / 896.0) - 0.5) * 2.0 * V_MAX;
        } else {
            u_norm = ((static_cast<double>(u) / 65535.0) - 0.5) * 2.0 * U_MAX;
            v_norm = ((static_cast<double>(v) / 65535.0) - 0.5) * 2.0 * V_MAX;
        }

        // PAL chroma with V-switch
        double chroma = (u_norm * sin_phase) + (v_norm * v_switch * cos_phase);
        int32_t chroma_scaled = static_cast<int32_t>(chroma * luma_range);

        int32_t composite = luma_scaled + chroma_scaled;
        line_buffer[sample] = clamp_to_16bit(composite);

        // Update phase using rotation matrix (more accurate than direct calculation)
        double next_sin = (sin_phase * cos_step) + (cos_phase * sin_step);
        double next_cos = (cos_phase * cos_step) - (sin_phase * sin_step);
        sin_phase = next_sin;
        cos_phase = next_cos;
    }
}

uint16_t PALActiveEncoder::yuv_to_composite(uint16_t y, uint16_t u, uint16_t v,
                                            double phase, bool studio_range_input) {
    int32_t luma_range = white_level_ - black_level_;
    int32_t luma_scaled;
    
    if (studio_range_input) {
        // Studio codes: 64→black_level, 940→white_level, preserve sub-black (allows negative)
        luma_scaled = black_level_ + ((static_cast<int32_t>(y) - 64) * luma_range) / 876;
    } else {
        // Full-range: 0-65535 → normalized to black-white range
        double y_norm = static_cast<double>(y) / 65535.0;
        luma_scaled = black_level_ + static_cast<int32_t>(y_norm * luma_range);
    }
    
    // PAL chroma constants
    const double U_MAX = 0.436010;
    const double V_MAX = 0.614975;
    double u_norm, v_norm;
    if (studio_range_input) {
        // Studio chroma: 0-896 range (64-960 studio codes)
        u_norm = ((static_cast<double>(u) / 896.0) - 0.5) * 2.0 * U_MAX;
        v_norm = ((static_cast<double>(v) / 896.0) - 0.5) * 2.0 * V_MAX;
    } else {
        // Full-range: 0-65535
        u_norm = ((static_cast<double>(u) / 65535.0) - 0.5) * 2.0 * U_MAX;
        v_norm = ((static_cast<double>(v) / 65535.0) - 0.5) * 2.0 * V_MAX;
    }
    
    // Note: V-switch is NOT applied in this helper - it's applied in encode_active_line
    // This is by design to keep the helper function flexible
    double chroma = (u_norm * std::sin(phase)) + (v_norm * std::cos(phase));
    int32_t chroma_scaled = static_cast<int32_t>(chroma * luma_range);

    int32_t composite = luma_scaled + chroma_scaled;
    return clamp_to_16bit(composite);
}

}  // namespace encode_orc
