/*
 * File:        ntsc_active_encoder.cpp
 * Module:      encode-orc
 * Purpose:     NTSC active video encoder implementation
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: 2026 Simon Inns
 */

#include "ntsc_active_encoder.h"
#include <cmath>
#include <algorithm>
#include <cstring>

namespace encode_orc {

// Constants
static constexpr double PI = 3.14159265358979323846;

NTSCActiveEncoder::NTSCActiveEncoder(const VideoParameters& params,
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
        chroma_filter_ = Filters::create_ntsc_uv_filter();
    }
    if (enable_luma_filter) {
        luma_filter_ = Filters::create_ntsc_uv_filter();
    }
}

double NTSCActiveEncoder::calculate_phase(int32_t line_number, int32_t field_number) const {
    // NTSC uses 262.5 lines per field and 227.5 cycles per line
    // to achieve proper 4-field color framing
    const double lines_per_field = 262.5;
    const double cycles_per_line = 227.5;

    double absolute_lines = static_cast<double>(field_number) * lines_per_field + static_cast<double>(line_number);
    double prev_cycles = absolute_lines * cycles_per_line;

    double phase_step = 2.0 * PI * (subcarrier_freq_ / sample_rate_);
    double phase = (2.0 * PI * prev_cycles) + static_cast<double>(params_.active_video_start) * phase_step;
    
    return phase;
}

uint16_t NTSCActiveEncoder::clamp_to_16bit(int32_t value) const {
    if (value < 0) return 0;
    if (value > 65535) return 65535;
    return static_cast<uint16_t>(value);
}

void NTSCActiveEncoder::encode_active_line(uint16_t* line_buffer,
                                           const uint16_t* y_line,
                                           const uint16_t* i_line,
                                           const uint16_t* q_line,
                                           int32_t line_number,
                                           int32_t field_number,
                                           bool /* is_first_field */,
                                           int32_t width,
                                           bool studio_range_input,
                                           uint16_t* y_buffer,
                                           uint16_t* c_buffer) {
    // Resolve source pointers; only allocate filtered buffers when filters are enabled.
    const uint16_t* y_data = y_line;
    const uint16_t* i_data = i_line;
    const uint16_t* q_data = q_line;

    if (luma_filter_) {
        thread_local std::vector<uint16_t> y_filtered;
        y_filtered.resize(width);
        std::copy(y_line, y_line + width, y_filtered.begin());
        luma_filter_.value().apply(y_filtered);
        y_data = y_filtered.data();
    }

    if (chroma_filter_) {
        thread_local std::vector<uint16_t> i_filtered;
        thread_local std::vector<uint16_t> q_filtered;
        i_filtered.resize(width);
        q_filtered.resize(width);
        std::copy(i_line, i_line + width, i_filtered.begin());
        std::copy(q_line, q_line + width, q_filtered.begin());
        chroma_filter_.value().apply(i_filtered);
        chroma_filter_.value().apply(q_filtered);
        i_data = i_filtered.data();
        q_data = q_filtered.data();
    }
    
    // Calculate initial phase
    double phase = calculate_phase(line_number, field_number);
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
        const uint16_t i = i_data[pixel_x];
        const uint16_t q = q_data[pixel_x];

        int32_t luma_range = white_level_ - black_level_;
        int32_t luma_scaled;

        if (studio_range_input) {
            // Preserve sub-black: don't clamp luma_scaled, allow negative values
            luma_scaled = black_level_ + ((static_cast<int32_t>(y) - 64) * luma_range) / 876;
        } else {
            double y_norm = static_cast<double>(y) / 65535.0;
            luma_scaled = black_level_ + static_cast<int32_t>(y_norm * luma_range);
        }

        // NTSC chroma constants (YIQ)
        const double I_MAX = 0.5957;
        const double Q_MAX = 0.5226;
        double i_norm;
        double q_norm;
        if (studio_range_input) {
            i_norm = ((static_cast<double>(i) / 896.0) - 0.5) * 2.0 * I_MAX;
            q_norm = ((static_cast<double>(q) / 896.0) - 0.5) * 2.0 * Q_MAX;
        } else {
            i_norm = ((static_cast<double>(i) / 65535.0) - 0.5) * 2.0 * I_MAX;
            q_norm = ((static_cast<double>(q) / 65535.0) - 0.5) * 2.0 * Q_MAX;
        }

        // NTSC chroma (no V-switch like PAL)
        double chroma = (i_norm * sin_phase) + (q_norm * cos_phase);
        int32_t chroma_scaled = static_cast<int32_t>(chroma * luma_range);
        
        // Clamp very small chroma oscillations to zero to eliminate wobble
        // caused by chroma filter ringing near neutral (I=Q=512) input values.
        // Threshold of ±50 levels suppresses the subcarrier oscillation (±38-44 levels)
        // while preserving actual color content (which would be much larger).
        if (chroma_scaled > -50 && chroma_scaled < 50) {
            chroma_scaled = 0;
        }

        int32_t composite = luma_scaled + chroma_scaled;
        line_buffer[sample] = clamp_to_16bit(composite);
        
        // If Y/C output is requested, populate separate Y and C buffers
        if (y_buffer != nullptr) {
            y_buffer[sample] = clamp_to_16bit(luma_scaled);
        }
        if (c_buffer != nullptr) {
            // Center pure chroma at 32768 (mid-16bit range) for Y/C output
            c_buffer[sample] = clamp_to_16bit(chroma_scaled + 32768);
        }

        // Update phase using rotation matrix (more accurate than direct calculation)
        double next_sin = (sin_phase * cos_step) + (cos_phase * sin_step);
        double next_cos = (cos_phase * cos_step) - (sin_phase * sin_step);
        sin_phase = next_sin;
        cos_phase = next_cos;
    }
}

uint16_t NTSCActiveEncoder::yuv_to_composite(uint16_t y, uint16_t u, uint16_t v,
                                             double phase, bool studio_range_input) {
    // Note: For NTSC, u/v are actually I/Q components
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
    
    // NTSC chroma constants (YIQ)
    const double I_MAX = 0.5957;
    const double Q_MAX = 0.5226;
    double i_norm, q_norm;
    if (studio_range_input) {
        // Studio chroma: 0-896 range (64-960 studio codes)
        i_norm = ((static_cast<double>(u) / 896.0) - 0.5) * 2.0 * I_MAX;
        q_norm = ((static_cast<double>(v) / 896.0) - 0.5) * 2.0 * Q_MAX;
    } else {
        // Full-range: 0-65535
        i_norm = ((static_cast<double>(u) / 65535.0) - 0.5) * 2.0 * I_MAX;
        q_norm = ((static_cast<double>(v) / 65535.0) - 0.5) * 2.0 * Q_MAX;
    }
    
    // NTSC chroma (no V-switch)
    double chroma = (i_norm * std::sin(phase)) + (q_norm * std::cos(phase));
    int32_t chroma_scaled = static_cast<int32_t>(chroma * luma_range);

    int32_t composite = luma_scaled + chroma_scaled;
    return clamp_to_16bit(composite);
}

}  // namespace encode_orc
