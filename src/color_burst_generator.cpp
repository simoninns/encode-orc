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

void ColorBurstGenerator::generate_ntsc_burst(uint16_t* line_buffer, int32_t line_number, int32_t field_number) {
    // NTSC color burst
    // Position: Starts approximately 5.3 µs after sync (back porch)
    // Duration: 9 cycles of subcarrier (approximately 2.5 µs at 3.58 MHz) at 100% amplitude
    // Amplitude: 40 IRE peak-to-peak (±20 IRE about blanking level)
    // Phase: fixed at +180°
    // Envelope: Rise/fall shaped over 3 cycles (outside the 100% region)
    
    int32_t burst_start = params_.colour_burst_start;
    int32_t burst_end = params_.colour_burst_end;
    
    // Calculate burst amplitude: ±20 IRE (40 IRE peak-to-peak)
    int32_t luma_range = white_level_ - blanking_level_;
    int32_t burst_amplitude = static_cast<int32_t>((20.0 / 100.0) * luma_range);
    
    // Burst phase: 180° (fixed for NTSC)
    double burst_phase_offset = PI;
    
    // Envelope shaping: 3 cycles of subcarrier for rise and fall (same ramp speed)
    // But positioned so 2 cycles are outside and 1 cycle is inside the burst region
    double cycle_time_ns = (1.0 / subcarrier_freq_) * 1e9;
    const double rise_time_ns = 3.0 * cycle_time_ns;
    const double fall_time_ns = 3.0 * cycle_time_ns;
    
    // Calculate sample range (2 cycles outside, 1 cycle inside on each side)
    int32_t cycles_outside = static_cast<int32_t>((2.0 * cycle_time_ns / 1e9) * sample_rate_);
    int32_t ramp_start = burst_start - cycles_outside;
    int32_t ramp_end = burst_end + cycles_outside;
    
    // Generate color burst with envelope shaping
    for (int32_t sample = ramp_start; sample < ramp_end; ++sample) {
        double phase = calculate_ntsc_phase(field_number, line_number, sample) + burst_phase_offset;
        double burst_signal = std::sin(phase);
        
        // Apply envelope shaping
        double envelope = calculate_envelope(sample, burst_start, burst_end, rise_time_ns, fall_time_ns);
        
        int32_t sample_value = blanking_level_ + 
                              static_cast<int32_t>(burst_amplitude * envelope * burst_signal);
        line_buffer[sample] = clamp_to_16bit(sample_value);
    }
}

void ColorBurstGenerator::generate_pal_burst(uint16_t* line_buffer, int32_t line_number, int32_t field_number) {
    // PAL color burst
    // Position: Starts approximately 5.6 µs after sync (back porch)
    // Duration: 10 cycles of subcarrier (approximately 2.25 µs) at 100% amplitude
    // Amplitude: ±150 mV (300mV peak-to-peak for 700mV white reference)
    // Phase: Swinging burst (V-switch modulated at ±135°)
    // Envelope: Rise/fall shaped over 3 cycles (outside the 100% region)
    
    int32_t burst_start = params_.colour_burst_start;
    int32_t burst_end = params_.colour_burst_end;
    
    // Calculate V-switch for burst phase
    int32_t v_switch = get_pal_v_switch(field_number, line_number);
    
    // Burst phase alternates with V-switch: ±135°
    double burst_phase_offset = v_switch * (135.0 * PI / 180.0);
    
    // Calculate burst amplitude: 3/14 of luma range (gives ±300mV for 700mV white)
    int32_t luma_range = white_level_ - blanking_level_;
    int32_t burst_amplitude = static_cast<int32_t>((3.0 / 14.0) * luma_range);
    
    // Envelope shaping: 3 cycles of subcarrier for rise and fall (same ramp speed)
    // But positioned so 2 cycles are outside and 1 cycle is inside the burst region
    double cycle_time_ns = (1.0 / subcarrier_freq_) * 1e9;
    const double rise_time_ns = 3.0 * cycle_time_ns;
    const double fall_time_ns = 3.0 * cycle_time_ns;
    
    // Calculate sample range (2 cycles outside, 1 cycle inside on each side)
    int32_t cycles_outside = static_cast<int32_t>((2.0 * cycle_time_ns / 1e9) * sample_rate_);
    int32_t ramp_start = burst_start - cycles_outside;
    int32_t ramp_end = burst_end + cycles_outside;
    
    // Generate color burst with envelope shaping
    for (int32_t sample = ramp_start; sample < ramp_end; ++sample) {
        double phase = calculate_pal_phase(field_number, line_number, sample) + burst_phase_offset;
        double burst_signal = std::sin(phase);
        
        // Apply envelope shaping
        double envelope = calculate_envelope(sample, burst_start, burst_end, rise_time_ns, fall_time_ns);
        
        int32_t sample_value = blanking_level_ + 
                              static_cast<int32_t>(burst_amplitude * envelope * burst_signal);
        line_buffer[sample] = clamp_to_16bit(sample_value);
    }
}

} // namespace encode_orc
