/*
 * File:        color_burst_generator.cpp
 * Module:      encode-orc
 * Purpose:     Shared color burst generation utility implementation
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: 2026 Simon Inns
 */

#include "color_burst_generator.h"

namespace encode_orc {

ColorBurstGenerator::ColorBurstGenerator(const VideoParameters& params)
    : params_(params) {
    
    // Set signal levels
    sync_level_ = 0x0000;  // Sync tip at 0V
    blanking_level_ = params_.blanking_16b_ire;
    white_level_ = params_.white_16b_ire;
    
    // Set subcarrier parameters
    subcarrier_freq_ = params_.fSC;
    sample_rate_ = params_.sample_rate;
}

double ColorBurstGenerator::calculate_ntsc_phase(int32_t field_number, int32_t line_number, int32_t sample) const {
    // NTSC has 262.5 lines per field. Model absolute line count as a double
    // to preserve the half-line offset between fields, which produces the
    // 4-field color framing sequence (±90° per field).
    const double lines_per_field = 262.5;
    const double cycles_per_line = 227.5;  // NTSC subcarrier cycles per line

    // Absolute lines elapsed before this line within the full sequence
    double prev_lines = static_cast<double>(field_number) * lines_per_field + static_cast<double>(line_number);

    // Total subcarrier cycles elapsed before this sample
    double prev_cycles = prev_lines * cycles_per_line;

    // Time term for this sample position within the line
    double time_phase = 2.0 * PI * subcarrier_freq_ * static_cast<double>(sample) / sample_rate_;
    return 2.0 * PI * prev_cycles + time_phase;
}

double ColorBurstGenerator::calculate_pal_phase(int32_t field_number, int32_t line_number, int32_t sample) const {
    // Calculate absolute line number in PAL 8-field sequence
    bool is_first_field = (field_number % 2) == 0;
    int32_t frame_line = is_first_field ? (line_number * 2 + 1) : (line_number * 2 + 2);
    
    int32_t field_id = field_number % 8;
    int32_t prev_lines = ((field_id / 2) * 625) + ((field_id % 2) * 313) + (frame_line / 2);
    
    // PAL: 283.7516 subcarrier cycles per line
    double prev_cycles = prev_lines * 283.7516;
    
    // Phase for this sample
    double time_phase = 2.0 * PI * subcarrier_freq_ * sample / sample_rate_;
    return 2.0 * PI * prev_cycles + time_phase;
}

int32_t ColorBurstGenerator::get_pal_v_switch(int32_t field_number, int32_t line_number) const {
    // Calculate absolute line number
    bool is_first_field = (field_number % 2) == 0;
    int32_t frame_line = is_first_field ? (line_number * 2 + 1) : (line_number * 2 + 2);
    
    int32_t field_id = field_number % 8;
    int32_t prev_lines = ((field_id / 2) * 625) + ((field_id % 2) * 313) + (frame_line / 2);
    
    // V-switch alternates every line
    return (prev_lines % 2) == 0 ? 1 : -1;
}

double ColorBurstGenerator::calculate_envelope(int32_t sample, int32_t burst_start, int32_t burst_end,
                                               double rise_time_ns, double fall_time_ns) const {
    // Convert time to samples
    double rise_samples = (rise_time_ns / 1e9) * sample_rate_;
    double fall_samples = (fall_time_ns / 1e9) * sample_rate_;
    
    // Calculate where the ramps start and end
    // Rise: starts at (burst_start - 2/3 * rise_time), ends at (burst_start + 1/3 * rise_time)
    // Fall: starts at (burst_end - 1/3 * fall_time), ends at (burst_end + 2/3 * fall_time)
    int32_t rise_start = burst_start - static_cast<int32_t>(rise_samples * 2.0 / 3.0);
    int32_t rise_end = burst_start + static_cast<int32_t>(rise_samples / 3.0);
    int32_t fall_start = burst_end - static_cast<int32_t>(fall_samples / 3.0);
    int32_t fall_end = burst_end + static_cast<int32_t>(fall_samples * 2.0 / 3.0);
    
    double envelope = 1.0;
    
    // Before rise starts
    if (sample < rise_start) {
        envelope = 0.0;
    }
    // Rising edge (cosine-shaped S-curve: 0 to 1)
    else if (sample < rise_end) {
        int32_t position_in_rise = sample - rise_start;
        // Cosine rise: 0 at rise_start, 1 at rise_end
        // Using (1 - cos(πt)) / 2 for smooth S-curve from 0 to 1
        double t = static_cast<double>(position_in_rise) / rise_samples;
        envelope = (1.0 - std::cos(PI * t)) / 2.0;
    }
    // Falling edge (cosine-shaped S-curve: 1 to 0)
    else if (sample >= fall_start && sample < fall_end) {
        int32_t position_in_fall = sample - fall_start;
        // Cosine fall: 1 at fall_start, 0 at fall_end
        // Using (1 + cos(πt)) / 2 for smooth S-curve from 1 to 0
        double t = static_cast<double>(position_in_fall) / fall_samples;
        envelope = (1.0 + std::cos(PI * t)) / 2.0;
    }
    // After fall ends
    else if (sample >= fall_end) {
        envelope = 0.0;
    }
    // Middle section: full amplitude
    else {
        envelope = 1.0;
    }
    
    return envelope;
}

void ColorBurstGenerator::generate_ntsc_burst(uint16_t* line_buffer, int32_t line_number, 
                                               int32_t field_number, int32_t center_level, 
                                               int32_t amplitude) {
    // Generate burst with envelope shaping, then shift to the specified center level
    // The burst signal is generated relative to 0, then offset to center_level
    
    int32_t burst_start = params_.colour_burst_start;
    int32_t burst_end = params_.colour_burst_end;
    
    // NTSC burst phase is fixed at 180°
    double burst_phase_offset = PI;
    
    // Envelope shaping: 3 cycles rise/fall with cosine S-curve
    double cycle_time_ns = (1.0 / subcarrier_freq_) * 1e9;
    const double rise_time_ns = 3.0 * cycle_time_ns;
    const double fall_time_ns = 3.0 * cycle_time_ns;
    
    // Calculate ramp positions: 2 cycles outside, 1 cycle inside
    double rise_samples = (rise_time_ns / 1e9) * sample_rate_;
    double fall_samples = (fall_time_ns / 1e9) * sample_rate_;
    rise_samples = std::max(rise_samples, 1.0);
    fall_samples = std::max(fall_samples, 1.0);
    
    int32_t rise_start = burst_start - static_cast<int32_t>(rise_samples * 2.0 / 3.0);
    int32_t rise_end = burst_start + static_cast<int32_t>(rise_samples / 3.0);
    int32_t fall_start = burst_end - static_cast<int32_t>(fall_samples / 3.0);
    int32_t fall_end = burst_end + static_cast<int32_t>(fall_samples * 2.0 / 3.0);
    
    // Fill line with center level first
    std::fill_n(line_buffer, params_.field_width, static_cast<uint16_t>(center_level));
    
    // Generate burst with envelope and write with center offset
    for (int32_t sample = std::max(0, rise_start); sample < std::min(static_cast<int32_t>(params_.field_width), fall_end); ++sample) {
        double phase = calculate_ntsc_phase(field_number, line_number, sample) + burst_phase_offset;
        double burst_signal = std::sin(phase);
        
        // Apply envelope shaping
        double envelope = 0.0;
        if (sample >= rise_start && sample < rise_end) {
            double position_in_rise = static_cast<double>(sample - rise_start);
            double t_env = position_in_rise / rise_samples;
            t_env = std::min(1.0, t_env);
            envelope = (1.0 - std::cos(PI * t_env)) / 2.0;
        } else if (sample >= rise_end && sample < fall_start) {
            envelope = 1.0;
        } else if (sample >= fall_start && sample < fall_end) {
            double position_in_fall = static_cast<double>(sample - fall_start);
            double t_env = position_in_fall / fall_samples;
            t_env = std::min(1.0, t_env);
            envelope = (1.0 + std::cos(PI * t_env)) / 2.0;
        } else {
            envelope = 0.0;
        }
        
        // Shift burst signal to center level
        int32_t sample_value = center_level + static_cast<int32_t>(amplitude * envelope * burst_signal);
        line_buffer[sample] = clamp_to_16bit(sample_value);
    }
}

void ColorBurstGenerator::generate_pal_burst(uint16_t* line_buffer, int32_t line_number,
                                              int32_t field_number, int32_t center_level,
                                              int32_t amplitude) {
    // Generate burst with envelope shaping, then shift to the specified center level
    // The burst signal is generated relative to 0, then offset to center_level
    
    int32_t burst_start = params_.colour_burst_start;
    int32_t burst_end = params_.colour_burst_end;
    
    // Calculate PAL phase with V-switch
    bool is_first_field = (field_number % 2) == 0;
    int32_t frame_line = is_first_field ? (line_number * 2 + 1) : (line_number * 2 + 2);
    int32_t field_id = field_number % 8;
    int32_t prev_lines = ((field_id / 2) * 625) + ((field_id % 2) * 313) + (frame_line / 2);
    int32_t v_switch = (prev_lines % 2 == 0) ? 1 : -1;
    double burst_phase_offset = v_switch * (135.0 * PI / 180.0);
    
    // Envelope shaping: 3 cycles rise/fall with cosine S-curve
    double cycle_time_ns = (1.0 / subcarrier_freq_) * 1e9;
    const double rise_time_ns = 3.0 * cycle_time_ns;
    const double fall_time_ns = 3.0 * cycle_time_ns;
    
    // Calculate ramp positions: 2 cycles outside, 1 cycle inside
    double rise_samples = (rise_time_ns / 1e9) * sample_rate_;
    double fall_samples = (fall_time_ns / 1e9) * sample_rate_;
    rise_samples = std::max(rise_samples, 1.0);
    fall_samples = std::max(fall_samples, 1.0);
    
    int32_t rise_start = burst_start - static_cast<int32_t>(rise_samples * 2.0 / 3.0);
    int32_t rise_end = burst_start + static_cast<int32_t>(rise_samples / 3.0);
    int32_t fall_start = burst_end - static_cast<int32_t>(fall_samples / 3.0);
    int32_t fall_end = burst_end + static_cast<int32_t>(fall_samples * 2.0 / 3.0);
    
    // Fill line with center level first
    std::fill_n(line_buffer, params_.field_width, static_cast<uint16_t>(center_level));
    
    // Generate burst with envelope and write with center offset
    for (int32_t sample = std::max(0, rise_start); sample < std::min(static_cast<int32_t>(params_.field_width), fall_end); ++sample) {
        double phase = calculate_pal_phase(field_number, line_number, sample) + burst_phase_offset;
        double burst_signal = std::sin(phase);
        
        // Apply envelope shaping
        double envelope = 0.0;
        if (sample >= rise_start && sample < rise_end) {
            double position_in_rise = static_cast<double>(sample - rise_start);
            double t_env = position_in_rise / rise_samples;
            t_env = std::min(1.0, t_env);
            envelope = (1.0 - std::cos(PI * t_env)) / 2.0;
        } else if (sample >= rise_end && sample < fall_start) {
            envelope = 1.0;
        } else if (sample >= fall_start && sample < fall_end) {
            double position_in_fall = static_cast<double>(sample - fall_start);
            double t_env = position_in_fall / fall_samples;
            t_env = std::min(1.0, t_env);
            envelope = (1.0 + std::cos(PI * t_env)) / 2.0;
        } else {
            envelope = 0.0;
        }
        
        // Shift burst signal to center level
        int32_t sample_value = center_level + static_cast<int32_t>(amplitude * envelope * burst_signal);
        line_buffer[sample] = clamp_to_16bit(sample_value);
    }
}

} // namespace encode_orc
