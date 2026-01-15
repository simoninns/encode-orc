/*
 * File:        ntsc_vits_generator.h
 * Module:      encode-orc
 * Purpose:     NTSC VITS (Vertical Interval Test Signal) generator
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: 2026 Simon Inns
 */

#ifndef ENCODE_ORC_NTSC_VITS_GENERATOR_H
#define ENCODE_ORC_NTSC_VITS_GENERATOR_H

#include "video_parameters.h"
#include <cstdint>
#include <vector>
#include <cmath>

namespace encode_orc {

/**
 * @brief NTSC VITS Signal Generator
 * 
 * Generates NTSC Vertical Interval Test Signals according to:
 * - FCC Recommendation 73-699 / CCIR Recommendation 314-4: Vertical Interval Reference (VIR)
 * - NTC-7 Composite Test Signal (BT.471) - Line 17 (or 283)
 * - NTC-7 Combination Test Signal - Line 20 (or 280)
 * 
 * Reference: Video Demystified, 5th Edition (ISBN 978-0-750-68395-1); FCC Recommendation 73-699; CCIR standards
 */
class NTSCVITSGenerator {
public:
    /**
     * @brief Construct an NTSC VITS generator
     * @param params Video parameters for NTSC
     */
    explicit NTSCVITSGenerator(const VideoParameters& params);
    
    /**
     * @brief Generate VIR (Vertical Interval Reference) signal
     * Lines 19 and 282: FCC Recommendation 73-699
     * Structure: 12µs blanking, 24µs chrominance reference (superimposed on 70 IRE pedestal),
     * 12µs luminance reference (70 IRE), 12µs black reference (7.5 IRE)
     * @param line_buffer Output buffer for one line
     * @param field_number Field number for phase calculation
     */
    void generate_vir_line19(uint16_t* line_buffer, int32_t field_number);
    
    /**
     * @brief Generate NTC-7 Composite Test Signal
     * Line 17 (also used as line 283): White bar, 2T pulse, 12.5T chrominance pulse,
     * 5-step modulated staircase (0-90 IRE with 40 IRE peak-to-peak subcarrier)
     * @param line_buffer Output buffer for one line
     * @param field_number Field number for phase calculation
     */
    void generate_ntc7_composite_line17(uint16_t* line_buffer, int32_t field_number);
    
    /**
     * @brief Generate NTC-7 Combination Test Signal
     * Line 20 (also used as line 280): White flag (100 IRE), multiburst (0.5-4.2 MHz),
     * 3-step modulated pedestal (20, 40, 80 IRE peak-to-peak at -90° phase)
     * @param line_buffer Output buffer for one line
     * @param field_number Field number for phase calculation
     */
    void generate_ntc7_combination_line20(uint16_t* line_buffer, int32_t field_number);

private:
    VideoParameters params_;
    
    // NTSC-specific constants
    static constexpr double PI = 3.141592653589793238463;
    
    // Signal levels (16-bit samples)
    int32_t sync_level_;
    int32_t blanking_level_;
    int32_t black_level_;
    int32_t white_level_;
    
    // Subcarrier parameters
    double subcarrier_freq_;
    double sample_rate_;
    double samples_per_line_;
    double samples_per_us_;
    
    /**
     * @brief Calculate IRE to 16-bit sample value
     * @param ire IRE value (0-100 for luma, -40 to -43 for sync)
     * @return 16-bit sample value
     */
    int32_t ire_to_sample(double ire) const;
    
    /**
     * @brief Calculate phase for a given field, line, and sample position
     * @param field_number Field number in sequence
     * @param line_number Line number within field
     * @param sample Sample position within line
     * @return Phase in radians
     */
    double calculate_phase(int32_t field_number, int32_t line_number, int32_t sample) const;
    
    /**
     * @brief Generate horizontal sync pulse
     * @param line_buffer Output buffer
     */
    void generate_sync_pulse(uint16_t* line_buffer);
    
    /**
     * @brief Generate color burst
     * @param line_buffer Output buffer
     * @param field_number Field number for phase calculation
     * @param line_number Line number for phase calculation
     */
    void generate_color_burst(uint16_t* line_buffer, int32_t field_number, int32_t line_number);
    
    /**
     * @brief Generate flat luminance level
     * @param line_buffer Output buffer
     * @param start_time Start time in microseconds
     * @param end_time End time in microseconds
     * @param ire IRE level (0-100)
     */
    void generate_flat_level(uint16_t* line_buffer, double start_time, double end_time, double ire);
    
    /**
     * @brief Generate 2T pulse
     * @param line_buffer Output buffer
     * @param center_time Center time in microseconds
     * @param peak_ire Peak IRE level (typically 100)
     * 
     * For NTSC: 250ns half-amplitude width
     */
    void generate_2t_pulse(uint16_t* line_buffer, double center_time, double peak_ire);
    
    /**
     * @brief Generate 12.5T chrominance pulse
     * @param line_buffer Output buffer
     * @param center_time Center time in microseconds
     * @param peak_ire Peak IRE level (typically 100)
     * @param field_number Field number for phase calculation
     * @param line_number Line number for phase calculation
     * 
     * For NTSC: 12.5 cycles at ~3.579545 MHz, ~1562.5ns half-amplitude width
     */
    void generate_12_5t_pulse(uint16_t* line_buffer, double center_time, double peak_ire,
                              int32_t field_number, int32_t line_number);
    
    /**
     * @brief Generate modulated staircase
     * @param line_buffer Output buffer
     * @param start_time Start time in microseconds
     * @param step_times Array of step start times (in microseconds)
     * @param step_levels Array of step IRE levels
     * @param num_steps Number of steps
     * @param chroma_amplitude Chroma modulation amplitude (IRE peak-to-peak)
     * @param chroma_phase Chroma phase relative to burst (degrees)
     * @param field_number Field number for phase calculation
     * @param line_number Line number for phase calculation
     * 
     * For NTSC: NTC-7 composite test signal with specified chroma modulation
     */
    void generate_modulated_staircase(uint16_t* line_buffer, double start_time,
                                     const double* step_times, const double* step_levels,
                                     int num_steps, double chroma_amplitude, double chroma_phase,
                                     int32_t field_number, int32_t line_number);
    
    /**
     * @brief Generate modulated pedestal
     * @param line_buffer Output buffer
     * @param start_time Start time in microseconds
     * @param duration Duration in microseconds
     * @param luma_low Lower luma level (IRE)
     * @param luma_high Upper luma level (IRE)
     * @param chroma_pp Chroma peak-to-peak amplitude (IRE)
     * @param chroma_phase Chroma phase relative to burst (degrees)
     * @param field_number Field number for phase calculation
     * @param line_number Line number for phase calculation
     * 
     * For NTSC: NTC-7 combination test signal pedestal with specified chroma modulation
     */
    void generate_modulated_pedestal(uint16_t* line_buffer, double start_time, double duration,
                                    double luma_low, double luma_high, double chroma_pp,
                                    double chroma_phase, int32_t field_number, int32_t line_number);
    
    /**
     * @brief Generate multiburst packet
     * @param line_buffer Output buffer
     * @param start_time Start time in microseconds
     * @param duration Duration in microseconds
     * @param frequency Frequency in MHz
     * @param pedestal_ire Pedestal IRE level
     * @param amplitude_pp Peak-to-peak amplitude in IRE
     */
    void generate_multiburst_packet(uint16_t* line_buffer, double start_time, double duration,
                                   double frequency, double pedestal_ire, double amplitude_pp);
    
    /**
     * @brief Clamp value to 16-bit unsigned range
     */
    static uint16_t clamp_to_16bit(int32_t value) {
        if (value < 0) return 0;
        if (value > 65535) return 65535;
        return static_cast<uint16_t>(value);
    }
};

} // namespace encode_orc

#endif // ENCODE_ORC_NTSC_VITS_GENERATOR_H
