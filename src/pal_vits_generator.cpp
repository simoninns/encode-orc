/*
 * File:        pal_vits_generator.cpp
 * Module:      encode-orc
 * Purpose:     PAL VITS (Vertical Interval Test Signal) generator implementation
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: 2026 Simon Inns
 */

#include "pal_vits_generator.h"
#include <algorithm>
#include <cstring>

namespace encode_orc {

PALVITSGenerator::PALVITSGenerator(const VideoParameters& params)
    : params_(params) {
    
    // Set signal levels
    sync_level_ = 0x0000;  // Sync tip at 0 IRE (0V)
    blanking_level_ = params_.blanking_16b_ire;
    black_level_ = params_.black_16b_ire;
    white_level_ = params_.white_16b_ire;
    
    // Set subcarrier parameters
    subcarrier_freq_ = params_.fSC;  // 4.43361875 MHz for PAL
    sample_rate_ = params_.sample_rate;
    
    // Calculate samples per line (64 µs for PAL)
    samples_per_line_ = sample_rate_ * 64.0e-6;
    samples_per_us_ = sample_rate_ / 1.0e6;
}

int32_t PALVITSGenerator::ire_to_sample(double ire) const {
    // PAL: 0 IRE = blanking, 100 IRE = white
    // Sync is at -43 IRE
    if (ire < -43.0) ire = -43.0;
    if (ire > 100.0) ire = 100.0;
    
    if (ire < 0.0) {
        // Below blanking (sync region)
        double sync_range = blanking_level_ - sync_level_;
        return static_cast<int32_t>(blanking_level_ - ((-ire / 43.0) * sync_range));
    } else {
        // Above blanking (active video)
        double luma_range = white_level_ - blanking_level_;
        return static_cast<int32_t>(blanking_level_ + ((ire / 100.0) * luma_range));
    }
}

double PALVITSGenerator::calculate_phase(int32_t field_number, int32_t line_number, int32_t sample) const {
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

int32_t PALVITSGenerator::get_v_switch(int32_t field_number, int32_t line_number) const {
    // Calculate absolute line number
    bool is_first_field = (field_number % 2) == 0;
    int32_t frame_line = is_first_field ? (line_number * 2 + 1) : (line_number * 2 + 2);
    
    int32_t field_id = field_number % 8;
    int32_t prev_lines = ((field_id / 2) * 625) + ((field_id % 2) * 313) + (frame_line / 2);
    
    // V-switch alternates every line
    return (prev_lines % 2) == 0 ? 1 : -1;
}

void PALVITSGenerator::generate_sync_pulse(uint16_t* line_buffer) {
    // PAL horizontal sync: 4.7 µs
    int32_t sync_samples = static_cast<int32_t>(4.7 * samples_per_us_);
    
    for (int32_t i = 0; i < sync_samples; ++i) {
        line_buffer[i] = static_cast<uint16_t>(sync_level_);
    }
}

void PALVITSGenerator::generate_color_burst(uint16_t* line_buffer, int32_t field_number, int32_t line_number) {
    // Color burst: starts at ~5.6 µs, duration ~2.25 µs (10 cycles)
    int32_t burst_start = params_.colour_burst_start;
    int32_t burst_end = params_.colour_burst_end;
    
    // Calculate V-switch for burst phase
    int32_t v_switch = get_v_switch(field_number, line_number);
    
    // Burst phase alternates with V-switch: ±135°
    double burst_phase_offset = v_switch * (135.0 * PI / 180.0);
    
    // Burst amplitude: 3/14 of luma range (gives ±300mV for 700mV white)
    int32_t luma_range = white_level_ - blanking_level_;
    int32_t burst_amplitude = static_cast<int32_t>((3.0 / 14.0) * luma_range);
    
    for (int32_t sample = burst_start; sample < burst_end; ++sample) {
        double phase = calculate_phase(field_number, line_number, sample) + burst_phase_offset;
        double burst_signal = std::sin(phase);
        
        int32_t sample_value = blanking_level_ + 
                              static_cast<int32_t>(burst_amplitude * burst_signal);
        line_buffer[sample] = clamp_to_16bit(sample_value);
    }
}

void PALVITSGenerator::generate_flat_level(uint16_t* line_buffer, double start_time, double end_time, double ire) {
    int32_t start_sample = static_cast<int32_t>(start_time * samples_per_us_);
    int32_t end_sample = static_cast<int32_t>(end_time * samples_per_us_);
    int32_t level = ire_to_sample(ire);
    
    for (int32_t i = start_sample; i < end_sample && i < params_.field_width; ++i) {
        line_buffer[i] = clamp_to_16bit(level);
    }
}

void PALVITSGenerator::generate_2t_pulse(uint16_t* line_buffer, double center_time, double peak_ire) {
    // 2T pulse: 200ns half-amplitude width
    // Use a raised-cosine window whose half-amplitude point occurs at 200ns
    
    int32_t center_sample = static_cast<int32_t>(center_time * samples_per_us_);
    // Use 0.4µs full window so the half-amplitude point (t=0.5) is 0.2µs = 200ns
    double pulse_width = 0.4;  // microseconds
    int32_t width_samples = static_cast<int32_t>(pulse_width * samples_per_us_);
    
    int32_t peak_level = ire_to_sample(peak_ire);
    int32_t blanking = ire_to_sample(0.0);
    
    // Generate pulse with raised cosine shape
    for (int32_t i = -width_samples * 3; i <= width_samples * 3; ++i) {
        int32_t sample = center_sample + i;
        if (sample < 0 || sample >= params_.field_width) continue;
        
        double t = static_cast<double>(i) / width_samples;
        double amplitude = 0.5 * (1.0 + std::cos(PI * t));
        
        if (std::abs(t) > 1.0) amplitude = 0.0;
        
        int32_t value = blanking + static_cast<int32_t>(amplitude * (peak_level - blanking));
        line_buffer[sample] = clamp_to_16bit(value);
    }
}

void PALVITSGenerator::generate_10t_pulse(uint16_t* line_buffer, double center_time, double peak_ire,
                                          int32_t field_number, int32_t line_number) {
    // 10T pulse: 10 cycles of subcarrier at 4.43361875 MHz with triangular envelope
    // Duration = 10 / 4.43361875 MHz ≈ 2.256 µs
    // Envelope: triangular (ramps up to peak at center, then ramps down)
    
    double pulse_duration = 10.0 / subcarrier_freq_ * 1.0e6;  // in microseconds
    double start_time = center_time - pulse_duration / 2.0;
    double end_time = center_time + pulse_duration / 2.0;
    
    int32_t start_sample = static_cast<int32_t>(start_time * samples_per_us_);
    int32_t end_sample = static_cast<int32_t>(end_time * samples_per_us_);
    int32_t center_sample = static_cast<int32_t>(center_time * samples_per_us_);
    
    int32_t blanking = ire_to_sample(0.0);
    int32_t peak_level = ire_to_sample(peak_ire);
    int32_t amplitude = (peak_level - blanking) / 2;  // Half for modulation
    
    // Generate 10 cycles of subcarrier with triangular envelope
    for (int32_t sample = start_sample; sample < end_sample && sample < params_.field_width; ++sample) {
        double phase = calculate_phase(field_number, line_number, sample);
        
        // Apply triangular envelope
        double envelope;
        if (sample < center_sample) {
            // Rising edge: 0 to 1
            envelope = static_cast<double>(sample - start_sample) / (center_sample - start_sample);
        } else {
            // Falling edge: 1 to 0
            envelope = static_cast<double>(end_sample - sample) / (end_sample - center_sample);
        }
        
        // Ensure envelope is in [0, 1]
        if (envelope < 0.0) envelope = 0.0;
        if (envelope > 1.0) envelope = 1.0;
        
        double chroma_signal = std::sin(phase);
        int32_t value = blanking + static_cast<int32_t>(amplitude * envelope * (1.0 + chroma_signal));
        line_buffer[sample] = clamp_to_16bit(value);
    }
}

void PALVITSGenerator::generate_modulated_staircase(uint16_t* line_buffer, double /* start_time */,
                                                    const double* step_times, const double* step_levels,
                                                    int num_steps, double chroma_amplitude, double chroma_phase,
                                                    int32_t field_number, int32_t line_number) {
    // Generate staircase with modulated chroma
    int32_t v_switch = get_v_switch(field_number, line_number);
    double phase_offset = chroma_phase * PI / 180.0;  // Convert degrees to radians
    
    for (int step = 0; step < num_steps - 1; ++step) {
        double step_start = step_times[step];
        double step_end = step_times[step + 1];
        double luma_ire = step_levels[step];
        
        int32_t start_sample = static_cast<int32_t>(step_start * samples_per_us_);
        int32_t end_sample = static_cast<int32_t>(step_end * samples_per_us_);
        
        int32_t luma_level = ire_to_sample(luma_ire);
        int32_t chroma_amp = static_cast<int32_t>((chroma_amplitude / 100.0) * (white_level_ - blanking_level_) / 2.0);
        
        for (int32_t sample = start_sample; sample < end_sample && sample < params_.field_width; ++sample) {
            // Apply envelope at step edges (~1µs rise/fall time)
            double t_from_start = (sample - start_sample) / samples_per_us_;
            double t_from_end = (end_sample - sample) / samples_per_us_;
            double envelope = 1.0;
            
            if (t_from_start < 1.0) {
                envelope = 0.5 * (1.0 - std::cos(PI * t_from_start));
            } else if (t_from_end < 1.0) {
                envelope = 0.5 * (1.0 - std::cos(PI * t_from_end));
            }
            
            // Calculate chroma with V-switch and phase offset
            double phase = calculate_phase(field_number, line_number, sample) + phase_offset;
            double u_chroma = std::cos(phase);
            double v_chroma = v_switch * std::sin(phase);
            double chroma = (u_chroma + v_chroma) / std::sqrt(2.0);  // Simplified for 60° phase
            
            int32_t value = luma_level + static_cast<int32_t>(chroma_amp * envelope * chroma);
            line_buffer[sample] = clamp_to_16bit(value);
        }
    }
}

void PALVITSGenerator::generate_modulated_pedestal(uint16_t* line_buffer, double start_time, double duration,
                                                   double luma_low, double luma_high, double chroma_pp,
                                                   double chroma_phase, int32_t field_number, int32_t line_number) {
    int32_t start_sample = static_cast<int32_t>(start_time * samples_per_us_);
    int32_t end_sample = static_cast<int32_t>((start_time + duration) * samples_per_us_);
    
    int32_t low_level = ire_to_sample(luma_low);
    int32_t high_level = ire_to_sample(luma_high);
    int32_t pedestal = (low_level + high_level) / 2;
    
    int32_t v_switch = get_v_switch(field_number, line_number);
    double phase_offset = chroma_phase * PI / 180.0;
    
    int32_t chroma_amp = static_cast<int32_t>((chroma_pp / 100.0) * (white_level_ - blanking_level_) / 2.0);
    
    for (int32_t sample = start_sample; sample < end_sample && sample < params_.field_width; ++sample) {
        // Apply envelope at edges (~1µs rise/fall time)
        double t_from_start = (sample - start_sample) / samples_per_us_;
        double t_from_end = (end_sample - sample) / samples_per_us_;
        double envelope = 1.0;
        
        if (t_from_start < 1.0) {
            envelope = 0.5 * (1.0 - std::cos(PI * t_from_start));
        } else if (t_from_end < 1.0) {
            envelope = 0.5 * (1.0 - std::cos(PI * t_from_end));
        }
        
        double phase = calculate_phase(field_number, line_number, sample) + phase_offset;
        double u_chroma = std::cos(phase);
        double v_chroma = v_switch * std::sin(phase);
        double chroma = (u_chroma + v_chroma) / std::sqrt(2.0);
        
        int32_t value = pedestal + static_cast<int32_t>(chroma_amp * envelope * chroma);
        line_buffer[sample] = clamp_to_16bit(value);
    }
}

void PALVITSGenerator::generate_multiburst_packet(uint16_t* line_buffer, double start_time, double duration,
                                                  double frequency, double pedestal_ire, double amplitude_pp) {
    int32_t start_sample = static_cast<int32_t>(start_time * samples_per_us_);
    int32_t end_sample = static_cast<int32_t>((start_time + duration) * samples_per_us_);
    
    int32_t pedestal = ire_to_sample(pedestal_ire);
    int32_t amplitude = static_cast<int32_t>((amplitude_pp / 100.0) * (white_level_ - blanking_level_) / 2.0);
    
    double freq_hz = frequency * 1.0e6;  // Convert MHz to Hz
    
    for (int32_t sample = start_sample; sample < end_sample && sample < params_.field_width; ++sample) {
        // Generate sine wave at specified frequency
        // Start at zero phase for each packet
        double t = (sample - start_sample) / sample_rate_;
        double signal = std::sin(2.0 * PI * freq_hz * t);
        
        int32_t value = pedestal + static_cast<int32_t>(amplitude * signal);
        line_buffer[sample] = clamp_to_16bit(value);
    }
}

// ============================================================================
// VITS Line Generators
// ============================================================================

void PALVITSGenerator::generate_itu_composite_line19(uint16_t* line_buffer, int32_t field_number) {
    // ITU Composite Test Signal for PAL (Figure 8.41) - Line 19
    // Components:
    // - White flag: 100 IRE, 10 µs (12-22 µs)
    // - 2T pulse: 100 IRE, centered at 26 µs
    // - 5-step modulated staircase: 42.86 IRE subcarrier at 60° phase
    //   Steps at 0, 20, 40, 60, 80, 100 IRE (30-60 µs)
    
    // Start with blanking
    std::fill_n(line_buffer, params_.field_width, static_cast<uint16_t>(blanking_level_));
    
    // Sync and burst
    generate_sync_pulse(line_buffer);
    generate_color_burst(line_buffer, field_number, 19);
    
    // White flag: 100 IRE from 12-22 µs
    generate_flat_level(line_buffer, 12.0, 22.0, 100.0);
    
    // 2T pulse centered at 26 µs
    generate_2t_pulse(line_buffer, 26.0, 100.0);
    
    // 5-step modulated staircase from 30-60 µs (6 levels: 0, 20, 40, 60, 80, 100 IRE)
    // Timing from spec: 30, 40, 44, 48, 52, 56, 60 µs
    double step_times[] = {30.0, 40.0, 44.0, 48.0, 52.0, 56.0, 60.0};
    double step_levels[] = {0.0, 20.0, 40.0, 60.0, 80.0, 100.0};
    generate_modulated_staircase(line_buffer, 30.0, step_times, step_levels, 7, 
                                42.86, 60.0, field_number, 19);

    // Hold final 100 IRE level for 2 µs before returning to blanking
    generate_flat_level(line_buffer, 60.0, 62.0, 100.0);
}

void PALVITSGenerator::generate_uk_national_line332(uint16_t* line_buffer, int32_t field_number) {
    // UK PAL National Test Signal #1 (Figure 8.42) - Line 332
    // Components:
    // - White flag: 100 IRE, 10 µs (12-22 µs)
    // - 2T pulse: centered at 26 µs
    // - 10T chrominance pulse: centered at 30 µs
    // - 5-step modulated staircase: 21.43 IRE subcarrier at 60° phase (34-60 µs)
    
    std::fill_n(line_buffer, params_.field_width, static_cast<uint16_t>(blanking_level_));
    
    generate_sync_pulse(line_buffer);
    generate_color_burst(line_buffer, field_number, 332);
    
    // White flag: 100 IRE from 12-22 µs
    generate_flat_level(line_buffer, 12.0, 22.0, 100.0);
    
    // 2T pulse centered at 26 µs
    generate_2t_pulse(line_buffer, 26.0, 100.0);
    
    // 10T chrominance pulse centered at 30 µs
    generate_10t_pulse(line_buffer, 30.0, 100.0, field_number, 332);
    
    // 5-step modulated staircase from 34-60 µs (6 levels: 0, 20, 40, 60, 80, 100 IRE)
    // Timing: 34, 40, 44, 48, 52, 56, 60 µs
    double step_times[] = {34.0, 40.0, 44.0, 48.0, 52.0, 56.0, 60.0};
    double step_levels[] = {0.0, 20.0, 40.0, 60.0, 80.0, 100.0};
    generate_modulated_staircase(line_buffer, 34.0, step_times, step_levels, 7,
                                21.43, 60.0, field_number, 332);
}

void PALVITSGenerator::generate_itu_its_line20(uint16_t* line_buffer, int32_t field_number) {
    // ITU Combination ITS Test Signal (Figure 8.45) - Line 20
    // Components:
    // - 3-step modulated pedestal: 20, 60, 100 IRE peak-to-peak (14-28 µs)
    // - Extended subcarrier packet: 60 IRE peak-to-peak (34-60 µs)
    // Phase: 60° relative to U axis
    
    std::fill_n(line_buffer, params_.field_width, static_cast<uint16_t>(blanking_level_));
    
    generate_sync_pulse(line_buffer);
    generate_color_burst(line_buffer, field_number, 20);
    
    // Rise to 50 IRE prior to the first pedestal
    generate_flat_level(line_buffer, 12.0, 14.0, 50.0);

    // 3-step modulated pedestal with updated timing (14-28 µs)
    // Pedestals: 40-60 IRE (20 pp), 20-80 IRE (60 pp), 0-100 IRE (100 pp)
    generate_modulated_pedestal(line_buffer, 14.0, 4.0, 40.0, 60.0, 20.0, 60.0, field_number, 20);
    generate_modulated_pedestal(line_buffer, 18.0, 4.0, 20.0, 80.0, 60.0, 60.0, field_number, 20);
    generate_modulated_pedestal(line_buffer, 22.0, 6.0, 0.0, 100.0, 100.0, 60.0, field_number, 20);

    // Spacer at 50 IRE before the extended subcarrier packet
    generate_flat_level(line_buffer, 28.0, 34.0, 50.0);

    // Extended subcarrier packet: 50 IRE pedestal with 60 IRE pp (34-60 µs)
    generate_modulated_pedestal(line_buffer, 34.0, 26.0, 20.0, 80.0, 60.0, 60.0, field_number, 20);

    // Hold 50 IRE briefly before returning to blanking
    generate_flat_level(line_buffer, 60.0, 61.0, 50.0);
    generate_flat_level(line_buffer, 61.0, 64.0, 0.0);
}

void PALVITSGenerator::generate_multiburst_line333(uint16_t* line_buffer, int32_t field_number) {
    // ITU Multiburst Test Signal (Figure 8.38) - Line 333
    // Components:
    // - White flag: 80 IRE, 4 µs
    // - Six frequency packets: 0.5, 1.0, 2.0, 4.0, 4.8, 5.8 MHz
    //   Each on 50 IRE pedestal with 60 IRE peak-to-peak
    //   Timing: 24, 30, 36, 42, 48, 54 µs
    
    std::fill_n(line_buffer, params_.field_width, static_cast<uint16_t>(blanking_level_));
    
    generate_sync_pulse(line_buffer);
    generate_color_burst(line_buffer, field_number, 333);
    
    // Level plan:
    // 12-18 µs: 80 IRE (white flag)
    // 18-20 µs: 20 IRE
    // 20-62 µs: 50 IRE pedestal (with multiburst packets on top)
    // 62+ µs: return to 0 IRE
    generate_flat_level(line_buffer, 12.0, 18.0, 80.0);
    generate_flat_level(line_buffer, 18.0, 20.0, 20.0);
    generate_flat_level(line_buffer, 20.0, 62.0, 50.0);
    generate_flat_level(line_buffer, 62.0, 64.0, 0.0);

    // Multiburst packets (each ~5 µs duration with gaps) on 50 IRE pedestal
    double frequencies[] = {0.5, 1.0, 2.0, 4.0, 4.8, 5.8};
    double start_times[] = {24.0, 30.0, 36.0, 42.0, 48.0, 54.0};
    
    for (int i = 0; i < 6; ++i) {
        generate_multiburst_packet(line_buffer, start_times[i], 5.0, 
                                  frequencies[i], 50.0, 60.0);
    }
}

} // namespace encode_orc
