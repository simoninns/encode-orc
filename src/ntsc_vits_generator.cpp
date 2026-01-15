/*
 * File:        ntsc_vits_generator.cpp
 * Module:      encode-orc
 * Purpose:     NTSC VITS (Vertical Interval Test Signal) generator implementation
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: 2026 Simon Inns
 */

#include "ntsc_vits_generator.h"
#include <algorithm>
#include <cstring>

namespace encode_orc {

NTSCVITSGenerator::NTSCVITSGenerator(const VideoParameters& params)
    : params_(params) {
    
    // Set signal levels
    sync_level_ = 0x0000;  // Sync tip at -40 to -43 IRE (0V)
    blanking_level_ = params_.blanking_16b_ire;
    black_level_ = params_.black_16b_ire;
    white_level_ = params_.white_16b_ire;
    
    // Set subcarrier parameters
    subcarrier_freq_ = params_.fSC;  // 3.579545 MHz for NTSC
    sample_rate_ = params_.sample_rate;
    
    // Calculate samples per line (63.556 µs for NTSC)
    samples_per_line_ = sample_rate_ * 63.556e-6;
    samples_per_us_ = sample_rate_ / 1.0e6;
}

int32_t NTSCVITSGenerator::ire_to_sample(double ire) const {
    // NTSC: 0 IRE = blanking, 100 IRE = white
    // Sync is at -40 to -43 IRE
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

double NTSCVITSGenerator::calculate_phase(int32_t field_number, int32_t line_number, int32_t sample) const {
    // Calculate absolute line number in NTSC
    // NTSC has 262.5 lines per field (525 total lines)
    bool is_first_field = (field_number % 2) == 0;
    int32_t frame_line = is_first_field ? (line_number * 2) : (line_number * 2 + 1);
    
    int32_t absolute_line = (field_number * 262) + frame_line;
    
    // NTSC: Subcarrier at 3.579545 MHz, 227.5 cycles per line (approximately)
    // Precise calculation: fSC / line_rate
    double cycles_per_line = subcarrier_freq_ / (30000.0 / 1001.0 / 525.0);
    double prev_cycles = absolute_line * cycles_per_line;
    
    // Phase for this sample
    double time_phase = 2.0 * PI * subcarrier_freq_ * sample / sample_rate_;
    return 2.0 * PI * prev_cycles + time_phase;
}

void NTSCVITSGenerator::generate_sync_pulse(uint16_t* line_buffer) {
    // NTSC horizontal sync: ~4.7 µs
    int32_t sync_samples = static_cast<int32_t>(4.7 * samples_per_us_);
    
    for (int32_t i = 0; i < sync_samples; ++i) {
        line_buffer[i] = static_cast<uint16_t>(sync_level_);
    }
}

void NTSCVITSGenerator::generate_color_burst(uint16_t* line_buffer, int32_t field_number, int32_t line_number) {
    // Color burst: starts at ~5.3 µs, duration ~2.5 µs (9 cycles for NTSC)
    int32_t burst_start = params_.colour_burst_start;
    int32_t burst_end = params_.colour_burst_end;
    
    // NTSC burst amplitude: 40 IRE (3/7 of luma range, approximately)
    int32_t luma_range = white_level_ - blanking_level_;
    int32_t burst_amplitude = static_cast<int32_t>((3.0 / 7.0) * luma_range);
    
    // Burst phase: 180° ± some tolerance (typically 180° reference)
    double burst_phase_offset = PI;  // 180°
    
    for (int32_t sample = burst_start; sample < burst_end; ++sample) {
        double phase = calculate_phase(field_number, line_number, sample) + burst_phase_offset;
        double burst_signal = std::sin(phase);
        
        int32_t sample_value = blanking_level_ + 
                              static_cast<int32_t>(burst_amplitude * burst_signal);
        line_buffer[sample] = clamp_to_16bit(sample_value);
    }
}

void NTSCVITSGenerator::generate_flat_level(uint16_t* line_buffer, double start_time, double end_time, double ire) {
    int32_t start_sample = static_cast<int32_t>(start_time * samples_per_us_);
    int32_t end_sample = static_cast<int32_t>(end_time * samples_per_us_);
    int32_t level = ire_to_sample(ire);
    
    for (int32_t i = start_sample; i < end_sample && i < params_.field_width; ++i) {
        line_buffer[i] = clamp_to_16bit(level);
    }
}

void NTSCVITSGenerator::generate_2t_pulse(uint16_t* line_buffer, double center_time, double peak_ire) {
    // 2T pulse: 250ns half-amplitude width
    // Use a raised-cosine window whose half-amplitude point occurs at 250ns
    
    int32_t center_sample = static_cast<int32_t>(center_time * samples_per_us_);
    // Use 0.5µs full window so the half-amplitude point (t=0.5) is 0.25µs = 250ns
    double pulse_width = 0.5;  // microseconds
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

void NTSCVITSGenerator::generate_12_5t_pulse(uint16_t* line_buffer, double center_time, double peak_ire,
                                             int32_t field_number, int32_t line_number) {
    // 12.5T pulse: 12.5 cycles of subcarrier at 3.579545 MHz with triangular envelope
    // Duration = 12.5 / 3.579545 MHz ≈ 3.49 µs
    // Envelope: triangular (ramps up to peak at center, then ramps down)
    
    double pulse_duration = 12.5 / subcarrier_freq_ * 1.0e6;  // in microseconds
    double start_time = center_time - pulse_duration / 2.0;
    double end_time = center_time + pulse_duration / 2.0;
    
    int32_t start_sample = static_cast<int32_t>(start_time * samples_per_us_);
    int32_t end_sample = static_cast<int32_t>(end_time * samples_per_us_);
    int32_t center_sample = static_cast<int32_t>(center_time * samples_per_us_);
    
    int32_t blanking = ire_to_sample(0.0);
    int32_t peak_level = ire_to_sample(peak_ire);
    int32_t amplitude = (peak_level - blanking) / 2;  // Half for modulation
    
    // Generate 12.5 cycles of subcarrier with triangular envelope
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

void NTSCVITSGenerator::generate_modulated_staircase(uint16_t* line_buffer, double /* start_time */,
                                                     const double* step_times, const double* step_levels,
                                                     int num_steps, double chroma_amplitude, double chroma_phase,
                                                     int32_t field_number, int32_t line_number) {
    // Generate staircase with modulated chroma
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
            // Apply envelope at step edges (~400ns rise/fall time)
            double t_from_start = (sample - start_sample) / samples_per_us_;
            double t_from_end = (end_sample - sample) / samples_per_us_;
            double envelope = 1.0;
            
            if (t_from_start < 0.0004) {
                // 400ns rise envelope
                envelope = 0.5 * (1.0 - std::cos(PI * t_from_start / 0.0004));
            } else if (t_from_end < 0.0004) {
                // 400ns fall envelope
                envelope = 0.5 * (1.0 - std::cos(PI * t_from_end / 0.0004));
            }
            
            // Calculate chroma with phase offset
            double phase = calculate_phase(field_number, line_number, sample) + phase_offset;
            double chroma = std::cos(phase);
            
            int32_t value = luma_level + static_cast<int32_t>(chroma_amp * envelope * chroma);
            line_buffer[sample] = clamp_to_16bit(value);
        }
    }
}

void NTSCVITSGenerator::generate_modulated_pedestal(uint16_t* line_buffer, double start_time, double duration,
                                                    double luma_low, double luma_high, double chroma_pp,
                                                    double chroma_phase, int32_t field_number, int32_t line_number) {
    int32_t start_sample = static_cast<int32_t>(start_time * samples_per_us_);
    int32_t end_sample = static_cast<int32_t>((start_time + duration) * samples_per_us_);
    
    int32_t low_level = ire_to_sample(luma_low);
    int32_t high_level = ire_to_sample(luma_high);
    int32_t pedestal = (low_level + high_level) / 2;
    
    double phase_offset = chroma_phase * PI / 180.0;
    
    int32_t chroma_amp = static_cast<int32_t>((chroma_pp / 100.0) * (white_level_ - blanking_level_) / 2.0);
    
    for (int32_t sample = start_sample; sample < end_sample && sample < params_.field_width; ++sample) {
        // Apply envelope at edges (~400ns rise/fall time)
        double t_from_start = (sample - start_sample) / samples_per_us_;
        double t_from_end = (end_sample - sample) / samples_per_us_;
        double envelope = 1.0;
        
        if (t_from_start < 0.0004) {
            // 400ns rise envelope
            envelope = 0.5 * (1.0 - std::cos(PI * t_from_start / 0.0004));
        } else if (t_from_end < 0.0004) {
            // 400ns fall envelope
            envelope = 0.5 * (1.0 - std::cos(PI * t_from_end / 0.0004));
        }
        
        double phase = calculate_phase(field_number, line_number, sample) + phase_offset;
        double chroma = std::cos(phase);
        
        int32_t value = pedestal + static_cast<int32_t>(chroma_amp * envelope * chroma);
        line_buffer[sample] = clamp_to_16bit(value);
    }
}

void NTSCVITSGenerator::generate_multiburst_packet(uint16_t* line_buffer, double start_time, double duration,
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

void NTSCVITSGenerator::generate_vir_line19(uint16_t* line_buffer, int32_t field_number) {
    // VIR (Vertical Interval Reference) Signal for NTSC (FCC 73-699, CCIR 314-4)
    // Line 19 (and 282): Structure as described in FCC documentation
    // Note: Encoder has already set blanking level and added sync + color burst
    // This function only adds the test signal content (pedestal references, not blanking fills):
    // - 12-36 µs: chrominance reference with 50-90 IRE pedestal (±20 IRE about 70 IRE center, 24µs total)
    // - 36-48 µs: luminance reference (50 IRE, 12µs)
    // - 48-60 µs: black reference (7.5 IRE, 12µs)
    
    // 12-36 µs: Chrominance reference on 50-90 IRE pedestal (modulated, ±20 IRE about 70 IRE center)
    // This is a modulated pedestal with 70 IRE center and 40 IRE peak-to-peak (50-90 IRE range)
    generate_modulated_pedestal(line_buffer, 12.0, 24.0, 50.0, 90.0, 40.0, -90.0, field_number, 19);
    
    // 36-48 µs: Luminance reference pedestal (50 IRE, 12µs)
    generate_flat_level(line_buffer, 36.0, 48.0, 50.0);
    
    // 48-60 µs: Black reference (7.5 IRE, 12µs)
    generate_flat_level(line_buffer, 48.0, 60.0, 7.5);
}

void NTSCVITSGenerator::generate_ntc7_composite_line17(uint16_t* line_buffer, int32_t field_number) {
    // NTC-7 Composite Test Signal for NTSC (Figure 8.40)
    // Line 17 (also used as line 283)
    // Note: Encoder has already set blanking level and added sync + color burst
    // This function adds the test signal content:
    // - 100 IRE white bar: 12–30 µs (18 µs width, 125±5 ns rise/fall)
    // - 2T pulse: 100 IRE peak, centered at 34 µs, 250 ns half-amplitude width
    // - 12.5T chrominance pulse: 100 IRE peak, centered at 37 µs, 1562.5 ns half-amplitude width
    // - 6-step modulated staircase: 0–90 IRE with 40 IRE peak-to-peak subcarrier at 0° phase
    //   Steps at: 42, 46, 49, 52, 55, 58, 61 µs (levels: 0, 20, 40, 60, 80, 90 IRE)
    // - 61–62 µs: 90 IRE
    // - 62 µs+: returns to 0 IRE blanking
    
    // 100 IRE white bar: 12–30 µs (with 125ns rise/fall times)
    generate_flat_level(line_buffer, 12.0125, 29.9875, 100.0);
    
    // 2T pulse centered at 34 µs (250 ns half-amplitude width)
    generate_2t_pulse(line_buffer, 34.0, 100.0);
    
    // 12.5T chrominance pulse centered at 37 µs (1562.5 ns half-amplitude width)
    generate_12_5t_pulse(line_buffer, 37.0, 100.0, field_number, 17);
    
    // 6-step modulated staircase from 42–61 µs
    // Steps at 42, 46, 49, 52, 55, 58, 61 µs with levels 0, 20, 40, 60, 80, 90 IRE
    // 40 IRE peak-to-peak chroma modulation at 0° phase
    double step_times[] = {42.0, 46.0, 49.0, 52.0, 55.0, 58.0, 61.0};
    double step_levels[] = {0.0, 20.0, 40.0, 60.0, 80.0, 90.0};
    generate_modulated_staircase(line_buffer, 42.0, step_times, step_levels, 7,
                                40.0, 0.0, field_number, 17);
    
    // 61–62 µs: 90 IRE (already set by last staircase step, extend it)
    generate_flat_level(line_buffer, 61.0, 62.0, 90.0);
    
    // 62 µs onward returns to 0 IRE blanking (already set by encoder)
}

void NTSCVITSGenerator::generate_ntc7_combination_line20(uint16_t* line_buffer, int32_t field_number) {
    // NTC-7 Combination Test Signal for NTSC (Figure 8.43)
    // Line 20 (also used as line 280)
    // Note: Encoder has already set blanking level and added sync + color burst
    // This function adds the test signal content:
    // - White flag: 100 IRE, 12-16 µs (4 µs width)
    // - 50 IRE pedestal: 16-18 µs (before multiburst)
    // - Multiburst on 50 IRE pedestal with 50 IRE peak-to-peak:
    //   0.5 MHz (18-23 µs, 5 µs), then 50 IRE gap (23-24 µs)
    //   1.0 MHz (24-27 µs, 3 µs), then 50 IRE gap (27-28 µs)
    //   2.0 MHz (28-31 µs, 3 µs), then 50 IRE gap (31-32 µs)
    //   3.0 MHz (32-35 µs, 3 µs), then 50 IRE gap (35-36 µs)
    //   3.6 MHz (36-39 µs, 3 µs), then 50 IRE gap (39-40 µs)
    //   4.2 MHz (40-43 µs, 3 µs), then 50 IRE pedestal (43-46 µs)
    // - 3-step modulated pedestal on 50 IRE base:
    //   46-50 µs: 20 IRE P-P at -90° phase
    //   50-54 µs: 40 IRE P-P at -90° phase
    //   54-60 µs: 80 IRE P-P at -90° phase
    // - 60-61 µs: 50 IRE
    // - 61 µs+: returns to 0 IRE blanking
    
    // White flag: 12-16 µs at 100 IRE
    generate_flat_level(line_buffer, 12.0, 16.0, 100.0);
    
    // 50 IRE pedestal: 16-18 µs
    generate_flat_level(line_buffer, 16.0, 18.0, 50.0);
    
    // Multiburst packets on 50 IRE pedestal (50 IRE ±25 IRE = 25-75 IRE)
    // 0.5 MHz: 18-23 µs (5 µs)
    generate_multiburst_packet(line_buffer, 18.0, 5.0, 0.5, 50.0, 50.0);
    generate_flat_level(line_buffer, 23.0, 24.0, 50.0);  // Gap
    
    // 1.0 MHz: 24-27 µs (3 µs)
    generate_multiburst_packet(line_buffer, 24.0, 3.0, 1.0, 50.0, 50.0);
    generate_flat_level(line_buffer, 27.0, 28.0, 50.0);  // Gap
    
    // 2.0 MHz: 28-31 µs (3 µs)
    generate_multiburst_packet(line_buffer, 28.0, 3.0, 2.0, 50.0, 50.0);
    generate_flat_level(line_buffer, 31.0, 32.0, 50.0);  // Gap
    
    // 3.0 MHz: 32-35 µs (3 µs)
    generate_multiburst_packet(line_buffer, 32.0, 3.0, 3.0, 50.0, 50.0);
    generate_flat_level(line_buffer, 35.0, 36.0, 50.0);  // Gap
    
    // 3.6 MHz: 36-39 µs (3 µs)
    generate_multiburst_packet(line_buffer, 36.0, 3.0, 3.6, 50.0, 50.0);
    generate_flat_level(line_buffer, 39.0, 40.0, 50.0);  // Gap
    
    // 4.2 MHz: 40-43 µs (3 µs)
    generate_multiburst_packet(line_buffer, 40.0, 3.0, 4.2, 50.0, 50.0);
    
    // 50 IRE pedestal: 43-46 µs (before modulated steps)
    generate_flat_level(line_buffer, 43.0, 46.0, 50.0);
    
    // 3-step modulated pedestal (all on 50 IRE base with modulation at -90° phase)
    // Step 1: 46-50 µs, 20 IRE P-P (40-60 IRE range)
    generate_modulated_pedestal(line_buffer, 46.0, 4.0, 40.0, 60.0, 20.0, -90.0, field_number, 20);
    
    // Step 2: 50-54 µs, 40 IRE P-P (30-70 IRE range)
    generate_modulated_pedestal(line_buffer, 50.0, 4.0, 30.0, 70.0, 40.0, -90.0, field_number, 20);
    
    // Step 3: 54-60 µs, 80 IRE P-P (10-90 IRE range)
    generate_modulated_pedestal(line_buffer, 54.0, 6.0, 10.0, 90.0, 80.0, -90.0, field_number, 20);
    
    // 60-61 µs: 50 IRE
    generate_flat_level(line_buffer, 60.0, 61.0, 50.0);
    
    // 61 µs onward returns to 0 IRE blanking (already set by encoder)
}

} // namespace encode_orc
