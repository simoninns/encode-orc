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

void ColorBurstGenerator::generate_ntsc_burst(uint16_t* line_buffer, int32_t line_number, int32_t field_number) {
    // NTSC color burst
    // Position: Starts approximately 5.3 µs after sync (back porch)
    // Duration: 9 cycles of subcarrier (approximately 2.5 µs at 3.58 MHz)
    // Amplitude: 40 IRE peak-to-peak (±20 IRE about blanking level)
    // Phase: fixed at +180°
    
    int32_t burst_start = params_.colour_burst_start;
    int32_t burst_end = params_.colour_burst_end;
    
    // Calculate burst amplitude: ±20 IRE (40 IRE peak-to-peak)
    int32_t luma_range = white_level_ - blanking_level_;
    int32_t burst_amplitude = static_cast<int32_t>((20.0 / 100.0) * luma_range);
    
    // Burst phase: 180° (fixed for NTSC)
    double burst_phase_offset = PI;
    
    // Generate color burst
    for (int32_t sample = burst_start; sample < burst_end; ++sample) {
        double phase = calculate_ntsc_phase(field_number, line_number, sample) + burst_phase_offset;
        double burst_signal = std::sin(phase);
        
        int32_t sample_value = blanking_level_ + 
                              static_cast<int32_t>(burst_amplitude * burst_signal);
        line_buffer[sample] = clamp_to_16bit(sample_value);
    }
}

void ColorBurstGenerator::generate_pal_burst(uint16_t* line_buffer, int32_t line_number, int32_t field_number) {
    // PAL color burst
    // Position: Starts approximately 5.6 µs after sync (back porch)
    // Duration: 10 cycles of subcarrier (approximately 2.25 µs)
    // Amplitude: ±150 mV (300mV peak-to-peak for 700mV white reference)
    // Phase: Swinging burst (V-switch modulated at ±135°)
    
    int32_t burst_start = params_.colour_burst_start;
    int32_t burst_end = params_.colour_burst_end;
    
    // Calculate V-switch for burst phase
    int32_t v_switch = get_pal_v_switch(field_number, line_number);
    
    // Burst phase alternates with V-switch: ±135°
    double burst_phase_offset = v_switch * (135.0 * PI / 180.0);
    
    // Calculate burst amplitude: 3/14 of luma range (gives ±300mV for 700mV white)
    int32_t luma_range = white_level_ - blanking_level_;
    int32_t burst_amplitude = static_cast<int32_t>((3.0 / 14.0) * luma_range);
    
    // Generate color burst with constant amplitude
    for (int32_t sample = burst_start; sample < burst_end; ++sample) {
        double phase = calculate_pal_phase(field_number, line_number, sample) + burst_phase_offset;
        double burst_signal = std::sin(phase);
        
        int32_t sample_value = blanking_level_ + 
                              static_cast<int32_t>(burst_amplitude * burst_signal);
        line_buffer[sample] = clamp_to_16bit(sample_value);
    }
}

} // namespace encode_orc
