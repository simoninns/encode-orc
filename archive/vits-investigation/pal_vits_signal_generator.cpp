/*
 * File:        pal_vits_signal_generator.cpp
 * Module:      encode-orc
 * Purpose:     PAL VITS signal generation implementation
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: 2026 Simon Inns
 */

#include "vits_signal_generator.h"
#include <cmath>
#include <cstring>

namespace encode_orc {

PALVITSSignalGenerator::PALVITSSignalGenerator(const VideoParameters& params)
    : params_(params)
{
    // Initialize signal levels from video parameters
    sync_level_ = 0x0000;                       // -300mV
    blanking_level_ = params_.blanking_16b_ire; // 0mV (0x4000)
    black_level_ = params_.black_16b_ire;       // 0mV (same as blanking in PAL)
    white_level_ = params_.white_16b_ire;       // 700mV (0xE000)
    
    // Subcarrier parameters
    subcarrier_freq_ = params_.fSC;             // 4.433618.75 MHz
    sample_rate_ = params_.sample_rate;         // 17.734475 MHz
    samples_per_cycle_ = sample_rate_ / subcarrier_freq_;
}

void PALVITSSignalGenerator::generateBaseLine(uint16_t* line_buffer) {
    // Fill entire line with blanking level
    for (int32_t i = 0; i < params_.field_width; ++i) {
        line_buffer[i] = static_cast<uint16_t>(blanking_level_);
    }
    
    // Generate horizontal sync pulse
    generateSyncPulse(line_buffer);
}

void PALVITSSignalGenerator::generateBaseLineWithBurst(uint16_t* line_buffer, 
                                                        uint16_t line_number,
                                                        int32_t field_number) {
    // Generate base line (sync + blanking)
    generateBaseLine(line_buffer);
    
    // Add color burst
    generateColorBurstInternal(line_buffer, line_number, field_number);
}

void PALVITSSignalGenerator::generateSyncPulse(uint16_t* line_buffer) {
    // PAL horizontal sync: ~4.7 µs duration
    // At 17.734475 MHz: 4.7µs × 17734475 = ~83 samples
    // Standard sync is approximately 74 samples (start of line)
    constexpr int32_t SYNC_DURATION = 74;
    
    for (int32_t i = 0; i < SYNC_DURATION && i < params_.field_width; ++i) {
        line_buffer[i] = static_cast<uint16_t>(sync_level_);
    }
}

void PALVITSSignalGenerator::generateColorBurstInternal(uint16_t* line_buffer,
                                                         uint16_t /* line_number */,
                                                         int32_t field_number) {
    // PAL color burst parameters
    // - Duration: 10 cycles of subcarrier (~2.26 µs)
    // - Phase: 135° reference phase
    // - Amplitude: ±150mV (peak-to-peak ~300mV, standard PAL burst)
    // - Position: Samples 98-138 (standard PAL burst location)
    // - V-Switch: Polarity inverted based on field
    
    constexpr double BURST_PHASE_DEGREES = 135.0;
    constexpr double BURST_PHASE = BURST_PHASE_DEGREES * PI / 180.0;
    constexpr double BURST_AMPLITUDE = 150.0 / 1000.0;  // 150mV as fraction of full scale (300mV peak-to-peak)
    
    // Calculate V-switch for this field
    int32_t v_switch = getVSwitch(field_number);
    
    // Calculate burst amplitude in 16-bit scale
    // Full scale is 1000mV (sync to max), so 300mV is 0.3 of range
    int32_t burst_amp = static_cast<int32_t>((white_level_ - sync_level_) * BURST_AMPLITUDE);
    
    // Generate 10 cycles of burst
    int32_t burst_start = params_.colour_burst_start;
    int32_t burst_end = params_.colour_burst_end;
    
    for (int32_t sample = burst_start; sample < burst_end && sample < params_.field_width; ++sample) {
        // Calculate phase for this sample
        double phase = 2.0 * PI * (sample / samples_per_cycle_) + BURST_PHASE;
        
        // Generate burst signal with V-switch
        double burst_signal = std::sin(phase) * v_switch;
        int32_t value = blanking_level_ + static_cast<int32_t>(burst_signal * burst_amp);
        
        line_buffer[sample] = clampTo16bit(value);
    }
}

void PALVITSSignalGenerator::fillActiveRegion(uint16_t* line_buffer, float level) {
    // Clamp level to 0.0-1.0 range
    if (level < 0.0f) level = 0.0f;
    if (level > 1.0f) level = 1.0f;
    
    // Calculate signal value (from black to white)
    int32_t value = black_level_ + static_cast<int32_t>((white_level_ - black_level_) * level);
    uint16_t sample = clampTo16bit(value);
    
    // Fill active video region (samples 185-1107 for PAL)
    for (int32_t i = params_.active_video_start; i < params_.active_video_end && i < params_.field_width; ++i) {
        line_buffer[i] = sample;
    }
}

uint16_t PALVITSSignalGenerator::clampTo16bit(int32_t value) {
    if (value < 0) return 0;
    if (value > 65535) return 65535;
    return static_cast<uint16_t>(value);
}

int32_t PALVITSSignalGenerator::getVSwitch(int32_t field_number) const {
    // PAL V-switch alternates every field (8 field sequence for full PAL)
    // Simplified 4-field sequence: +1, +1, -1, -1
    int32_t phase = field_number % 4;
    return (phase < 2) ? 1 : -1;
}

// ============================================================================
// Reference Signal Generation
// ============================================================================

void PALVITSSignalGenerator::generateWhiteReference(uint16_t* line_buffer,
                                                    uint16_t line_number,
                                                    int32_t field_number) {
    generateBaseLineWithBurst(line_buffer, line_number, field_number);
    fillActiveRegion(line_buffer, 1.0f);  // 100 IRE / 1.0 luma
}

void PALVITSSignalGenerator::generate75GrayReference(uint16_t* line_buffer,
                                                     uint16_t line_number,
                                                     int32_t field_number) {
    generateBaseLineWithBurst(line_buffer, line_number, field_number);
    fillActiveRegion(line_buffer, 0.75f);  // 75 IRE / 0.75 luma
}

void PALVITSSignalGenerator::generate50GrayReference(uint16_t* line_buffer,
                                                     uint16_t line_number,
                                                     int32_t field_number) {
    generateBaseLineWithBurst(line_buffer, line_number, field_number);
    fillActiveRegion(line_buffer, 0.5f);  // 50 IRE / 0.5 luma
}

void PALVITSSignalGenerator::generateBlackReference(uint16_t* line_buffer,
                                                    uint16_t line_number,
                                                    int32_t field_number) {
    generateBaseLineWithBurst(line_buffer, line_number, field_number);
    fillActiveRegion(line_buffer, 0.0f);  // 0 IRE / 0.0 luma (blanking level)
}

// ============================================================================
// Color Burst Generation
// ============================================================================

void PALVITSSignalGenerator::generateColorBurst(uint16_t* line_buffer,
                                                uint16_t line_number,
                                                int32_t field_number) {
    // Generate base line with sync and color burst
    generateBaseLineWithBurst(line_buffer, line_number, field_number);
    
    // Fill active region with blanking level (no additional signal)
    fillActiveRegion(line_buffer, 0.0f);
}

// ============================================================================
// Frequency Response Signals
// ============================================================================

void PALVITSSignalGenerator::generateMultiburst(uint16_t* line_buffer,
                                                const std::vector<float>& frequencies,
                                                uint16_t line_number,
                                                int32_t field_number) {
    generateBaseLineWithBurst(line_buffer, line_number, field_number);
    
    // Multiburst signal parameters
    // - Amplitude: 50 IRE (half of full scale)
    // - Each burst: ~0.3 µs duration
    // - Frequencies: typically 0.5, 1.0, 2.0, 3.0, 4.2 MHz for PAL
    
    constexpr float BURST_AMPLITUDE = 0.5f;  // 50% of full scale
    
    int32_t active_width = params_.active_video_end - params_.active_video_start;
    int32_t burst_width = active_width / static_cast<int32_t>(frequencies.size());
    
    for (size_t i = 0; i < frequencies.size(); ++i) {
        int32_t burst_start = params_.active_video_start + static_cast<int32_t>(i) * burst_width;
        int32_t burst_end = burst_start + burst_width;
        
        if (burst_end > params_.active_video_end) {
            burst_end = params_.active_video_end;
        }
        
        // Convert frequency from MHz to Hz
        double freq_hz = frequencies[i] * 1e6;
        
        // Generate sine wave at specified frequency
        for (int32_t sample = burst_start; sample < burst_end && sample < params_.field_width; ++sample) {
            double time = static_cast<double>(sample - burst_start) / sample_rate_;
            double phase = 2.0 * PI * freq_hz * time;
            double signal = std::sin(phase) * BURST_AMPLITUDE;
            
            int32_t value = black_level_ + static_cast<int32_t>((white_level_ - black_level_) * (0.5 + signal * 0.5));
            line_buffer[sample] = clampTo16bit(value);
        }
    }
}

void PALVITSSignalGenerator::generateStaircase(uint16_t* line_buffer,
                                              uint8_t numSteps,
                                              uint16_t line_number,
                                              int32_t field_number) {
    generateBaseLineWithBurst(line_buffer, line_number, field_number);
    
    // Staircase signal: multiple discrete luminance levels
    // Standard levels: 0, 14.3, 28.6, 42.9, 57.1, 71.4, 85.7, 100 IRE
    
    if (numSteps == 0) numSteps = 8;  // Default to 8 steps
    
    int32_t active_width = params_.active_video_end - params_.active_video_start;
    int32_t step_width = active_width / numSteps;
    
    for (uint8_t step = 0; step < numSteps; ++step) {
        int32_t step_start = params_.active_video_start + step * step_width;
        int32_t step_end = step_start + step_width;
        
        if (step_end > params_.active_video_end) {
            step_end = params_.active_video_end;
        }
        
        // Calculate step level (0.0 to 1.0)
        float level = static_cast<float>(step) / static_cast<float>(numSteps - 1);
        int32_t value = black_level_ + static_cast<int32_t>((white_level_ - black_level_) * level);
        uint16_t sample = clampTo16bit(value);
        
        // Fill step
        for (int32_t i = step_start; i < step_end && i < params_.field_width; ++i) {
            line_buffer[i] = sample;
        }
    }
}

// ============================================================================
// LaserDisc-Specific Signals (IEC 60856-1986)
// ============================================================================

void PALVITSSignalGenerator::generateInsertionTestSignal(uint16_t* line_buffer,
                                                         uint16_t line_number,
                                                         int32_t field_number) {
    generateBaseLineWithBurst(line_buffer, line_number, field_number);
    
    // Insertion Test Signal (ITS) for LaserDisc
    // This is a timing reference for LaserDisc playback head stability
    // IEC 60856-1986 specifies this as a pulse or modulated signal
    // 
    // Implementation: Generate a series of pulses at specific intervals
    // (Exact specification would require access to IEC 60856-1986 pages 20-21)
    
    // For now, implement as a series of narrow pulses at regular intervals
    constexpr int32_t PULSE_WIDTH = 20;     // ~1.1 µs pulse width
    constexpr int32_t PULSE_SPACING = 100;  // Spacing between pulses
    
    int32_t active_start = params_.active_video_start;
    int32_t active_end = params_.active_video_end;
    
    for (int32_t pos = active_start; pos < active_end; pos += PULSE_SPACING) {
        for (int32_t i = 0; i < PULSE_WIDTH && (pos + i) < active_end; ++i) {
            line_buffer[pos + i] = static_cast<uint16_t>(white_level_);
        }
    }
}

void PALVITSSignalGenerator::generateDifferentialGainPhase(uint16_t* line_buffer,
                                                          float chromaAmplitude,
                                                          float backgroundLuma,
                                                          uint16_t line_number,
                                                          int32_t field_number) {
    generateBaseLineWithBurst(line_buffer, line_number, field_number);
    
    // Differential gain and phase test signal
    // Chroma signal at specified amplitude overlaid on luminance background
    // Used to measure chroma gain/phase variation with luminance
    
    // Clamp parameters
    if (chromaAmplitude < 0.0f) chromaAmplitude = 0.0f;
    if (chromaAmplitude > 1.0f) chromaAmplitude = 1.0f;
    if (backgroundLuma < 0.0f) backgroundLuma = 0.0f;
    if (backgroundLuma > 1.0f) backgroundLuma = 1.0f;
    
    // Calculate background level
    int32_t bg_level = black_level_ + static_cast<int32_t>((white_level_ - black_level_) * backgroundLuma);
    
    // Calculate chroma amplitude (typically 300mV for 100%)
    int32_t chroma_amp = static_cast<int32_t>((white_level_ - black_level_) * chromaAmplitude * 0.43);
    
    // Fill active region with background + chroma modulation
    for (int32_t sample = params_.active_video_start; 
         sample < params_.active_video_end && sample < params_.field_width; 
         ++sample) {
        
        // Generate chroma at subcarrier frequency
        double phase = 2.0 * PI * (sample / samples_per_cycle_);
        double chroma = std::sin(phase);
        
        int32_t value = bg_level + static_cast<int32_t>(chroma * chroma_amp);
        line_buffer[sample] = clampTo16bit(value);
    }
}

void PALVITSSignalGenerator::generateCrossColorReference(uint16_t* line_buffer,
                                                        uint16_t line_number,
                                                        int32_t field_number) {
    generateBaseLineWithBurst(line_buffer, line_number, field_number);
    
    // Cross-color distortion reference signal
    // Chroma overlay on luminance steps to detect cross-color artifacts
    
    // Create luminance steps across active region
    int32_t active_width = params_.active_video_end - params_.active_video_start;
    int32_t step_width = active_width / 4;  // 4 luminance levels
    
    for (int32_t step = 0; step < 4; ++step) {
        int32_t step_start = params_.active_video_start + step * step_width;
        int32_t step_end = step_start + step_width;
        
        if (step_end > params_.active_video_end) {
            step_end = params_.active_video_end;
        }
        
        // Luminance levels: 0%, 33%, 67%, 100%
        float luma_level = static_cast<float>(step) / 3.0f;
        int32_t luma_value = black_level_ + static_cast<int32_t>((white_level_ - black_level_) * luma_level);
        
        // Add high-frequency chroma modulation
        int32_t chroma_amp = (white_level_ - black_level_) / 10;  // 10% chroma
        
        for (int32_t sample = step_start; sample < step_end && sample < params_.field_width; ++sample) {
            // High-frequency chroma (near luma bandwidth edge)
            double phase = 2.0 * PI * (sample / samples_per_cycle_) * 1.5;  // 1.5× subcarrier
            double chroma = std::sin(phase);
            
            int32_t value = luma_value + static_cast<int32_t>(chroma * chroma_amp);
            line_buffer[sample] = clampTo16bit(value);
        }
    }
}

// ============================================================================
// IEC 60856-1986 Specific VITS Signals
// ============================================================================

void PALVITSSignalGenerator::generateIEC60856Line19(uint16_t* line_buffer,
                                                    uint16_t line_number,
                                                    int32_t field_number) {
    // Line 19: Luminance amplitude, transient response, K-factor tests
    // IEC 60856 Figure 7 timing (from ~6 to ~32 µs):
    //   ~6-11 µs:   B2 (white reference bar, 0.70V)
    //   ~11-13 µs:  B1 (2T sine-squared pulse, 200ns)
    //   ~13-18 µs:  F (20T carrier-borne pulse, 2000ns)
    //   ~18-20 µs:  gap
    //   ~20-31 µs:  D1 (6-step staircase)
    //   ~31-32 µs:  return to blanking
    
    generateBaseLineWithBurst(line_buffer, line_number, field_number);
    
    const int32_t active_start = params_.active_video_start;  // 185
    const int32_t active_end = params_.active_video_end;      // 1107
    
    // Helper to convert µs duration to samples
    auto us_to_samples = [this](double us) {
        return static_cast<int32_t>(us * sample_rate_ / 1e6);
    };
    
    // Timing window (per IEC 60856, Figure 7):
    // Measurements from start of ACTIVE VIDEO (sample 185):
    // Active video starts 12 µs after line start, so:
    // 12→22 µs absolute = 0→10 µs relative (B2)
    // 22→24 µs absolute = 10→12 µs relative (B1)
    // 24→28 µs absolute = 12→16 µs relative (F)
    // 28→52 µs absolute = 16→40 µs relative (D1)
    
    int32_t b2_start = active_start + us_to_samples(0.0);
    int32_t b2_rise_end = b2_start + us_to_samples(0.1);      // Rise time: 100 ns
    int32_t b2_end = active_start + us_to_samples(10.0);
    int32_t b2_fall_start = b2_end - us_to_samples(0.1);      // Fall starts 100 ns before end
    int32_t b2_rise_time = b2_rise_end - b2_start;
    
    int32_t b1_start = active_start + us_to_samples(10.0);
    int32_t b1_pulse_width = us_to_samples(0.2);  // 200 ns ±6 ns actual pulse width
    
    int32_t f_start = active_start + us_to_samples(12.0);
    int32_t f_pulse_width = us_to_samples(2.0);   // 2000 ns ±60 ns actual pulse width
    
    int32_t d1_start = active_start + us_to_samples(16.0);
    int32_t d1_width = us_to_samples(24.0);  // 16-40 µs = 24 µs
    
    // B2: White Reference Bar (0.70 Vpp ± 0.5%) with 100 ns rise/fall
    // Rise: 100 ±10 ns
    for (int32_t i = 0; i < b2_rise_time && (b2_start + i) < active_end; ++i) {
        int32_t sample = b2_start + i;
        double t = static_cast<double>(i) / static_cast<double>(b2_rise_time);
        int32_t value = blanking_level_ + static_cast<int32_t>((white_level_ - blanking_level_) * t);
        line_buffer[sample] = clampTo16bit(value);
    }
    
    // Flat portion at white level
    for (int32_t sample = b2_rise_end; sample < b2_fall_start && sample < active_end; ++sample) {
        line_buffer[sample] = static_cast<uint16_t>(white_level_);
    }
    
    // Fall: 100 ±10 ns
    int32_t b2_fall_time = b2_end - b2_fall_start;
    for (int32_t i = 0; i < b2_fall_time && (b2_fall_start + i) < active_end; ++i) {
        int32_t sample = b2_fall_start + i;
        double t = static_cast<double>(i) / static_cast<double>(b2_fall_time);
        int32_t value = blanking_level_ + static_cast<int32_t>((white_level_ - blanking_level_) * (1.0 - t));
        line_buffer[sample] = clampTo16bit(value);
    }
    
    // B1: 2T Sine-Squared Pulse (200 ± 6 ns)
    // Envelope: sin²(πt/T), rises from blanking to white and back
    for (int32_t i = 0; i < b1_pulse_width; ++i) {
        int32_t sample = b1_start + i;
        if (sample >= active_end) break;
        
        // Sine-squared envelope: starts at 0, peaks at 1, returns to 0
        double t = static_cast<double>(i) / static_cast<double>(b1_pulse_width);
        double envelope = std::sin(PI * t);
        envelope = envelope * envelope;  // sin²(πt/T)
        
        int32_t value = blanking_level_ + static_cast<int32_t>((white_level_ - blanking_level_) * envelope);
        line_buffer[sample] = clampTo16bit(value);
    }
    
    // F: 20T Carrier-Borne Sine-Squared Pulse (2000 ± 60 ns)
    // Luminance envelope sin²(πt/T) modulated by subcarrier
    for (int32_t i = 0; i < f_pulse_width; ++i) {
        int32_t sample = f_start + i;
        if (sample >= active_end) break;
        
        // Sine-squared envelope
        double t = static_cast<double>(i) / static_cast<double>(f_pulse_width);
        double envelope = std::sin(PI * t);
        envelope = envelope * envelope;
        
        // Subcarrier modulation at sample position
        // Phase advances relative to line start
        double phase = 2.0 * PI * (sample / samples_per_cycle_);
        double subcarrier = std::sin(phase);
        
        // Composite: envelope with subcarrier ripple
        // Base is halfway up (0.5) plus envelope variation (0.5), 
        // with superimposed subcarrier modulation
        double signal = 0.5 + 0.35 * envelope + 0.15 * (envelope * subcarrier);
        signal = std::min(1.0, std::max(0.0, signal));  // Clamp to 0-1
        
        int32_t value = blanking_level_ + static_cast<int32_t>((white_level_ - blanking_level_) * signal);
        line_buffer[sample] = clampTo16bit(value);
    }
    
    // D1: 6-Level Staircase (0%, 20%, 40%, 60%, 80%, 100%)
    // 6 evenly-spaced steps across the D1 region
    const int32_t num_levels = 6;
    const int32_t step_width = d1_width / num_levels;
    const int32_t rise_time = 4;  // ~235 ns rise time
    
    int32_t pos = d1_start;
    int32_t d1_end = d1_start + d1_width;  // Calculate end position
    for (int32_t level = 0; level < num_levels && pos < d1_end; ++level) {
        // Target level: 0/5, 1/5, 2/5, 3/5, 4/5, 5/5
        float target_luma = static_cast<float>(level) / static_cast<float>(num_levels - 1);
        int32_t target_value = blanking_level_ + static_cast<int32_t>((white_level_ - blanking_level_) * target_luma);
        
        // Previous level for smooth transition
        int32_t prev_value = (level > 0) ? 
            blanking_level_ + static_cast<int32_t>((white_level_ - blanking_level_) * 
            (static_cast<float>(level - 1) / static_cast<float>(num_levels - 1))) :
            blanking_level_;
        
        // Rise time: linear ramp from previous to target level
        for (int32_t r = 0; r < rise_time && pos < d1_end && pos < active_end; ++r) {
            double t = static_cast<double>(r) / static_cast<double>(rise_time);
            int32_t value = prev_value + static_cast<int32_t>((target_value - prev_value) * t);
            line_buffer[pos++] = clampTo16bit(value);
        }
        
        // Flat portion at target level
        int32_t flat_width = step_width - rise_time;
        for (int32_t f = 0; f < flat_width && pos < d1_end && pos < active_end; ++f) {
            line_buffer[pos++] = static_cast<uint16_t>(target_value);
        }
    }
}

void PALVITSSignalGenerator::generateIEC60856Line20(uint16_t* line_buffer,
                                                    uint16_t line_number,
                                                    int32_t field_number) {
    // Line 20: Frequency response (multiburst)
    // Contains: C1 (white ref) + C2 (black ref) + C3 (multiburst)
    
    generateBaseLineWithBurst(line_buffer, line_number, field_number);
    
    const int32_t active_start = params_.active_video_start;  // 185
    const int32_t active_end = params_.active_video_end;      // 1107
    
    // Helper to convert µs duration to samples
    auto us_to_samples = [this](double us) {
        return static_cast<int32_t>(us * sample_rate_ / 1e6);
    };
    
    // Timing window (per IEC 60856, Figure 8):
    // Measurements from start of ACTIVE VIDEO (sample 185):
    // Active video starts 12 µs after line start, so:
    // 12→16 µs absolute = 0→4 µs relative
    // 16→20 µs absolute = 4→8 µs relative
    // 20→60 µs absolute = 8→48 µs relative
    
    int32_t c1_start = active_start + us_to_samples(0.0);
    int32_t c1_rise_end = c1_start + us_to_samples(0.2);      // 200 ns rise
    int32_t c1_end = active_start + us_to_samples(4.0);
    int32_t c1_fall_start = c1_end - us_to_samples(0.2);      // 200 ns fall
    int32_t c1_rise_time = c1_rise_end - c1_start;
    int32_t c1_fall_time = c1_end - c1_fall_start;
    
    int32_t c2_start = active_start + us_to_samples(4.0);
    int32_t c2_rise_end = c2_start + us_to_samples(0.2);      // 200 ns rise
    int32_t c2_end = active_start + us_to_samples(8.0);
    int32_t c2_fall_start = c2_end - us_to_samples(0.2);      // 200 ns fall
    int32_t c2_rise_time = c2_rise_end - c2_start;
    int32_t c2_fall_time = c2_end - c2_fall_start;
    
    int32_t c3_start = active_start + us_to_samples(8.0);
    int32_t c3_end = active_start + us_to_samples(48.0);
    
    // C1: White Reference (80% of 0.70V ± 1%) with rise/fall
    const uint16_t c1_level = static_cast<uint16_t>(blanking_level_ + static_cast<int32_t>((white_level_ - blanking_level_) * 0.80));
    
    // C1 rise
    for (int32_t i = 0; i < c1_rise_time && (c1_start + i) < active_end; ++i) {
        int32_t sample = c1_start + i;
        double t = static_cast<double>(i) / static_cast<double>(c1_rise_time);
        int32_t value = blanking_level_ + static_cast<int32_t>((c1_level - blanking_level_) * t);
        line_buffer[sample] = clampTo16bit(value);
    }
    
    // C1 flat
    for (int32_t i = c1_rise_end; i < c1_fall_start && i < active_end; ++i) {
        line_buffer[i] = c1_level;
    }
    
    // C1 fall
    for (int32_t i = 0; i < c1_fall_time && (c1_fall_start + i) < active_end; ++i) {
        int32_t sample = c1_fall_start + i;
        double t = static_cast<double>(i) / static_cast<double>(c1_fall_time);
        int32_t value = c1_level - static_cast<int32_t>((c1_level - blanking_level_) * t);
        line_buffer[sample] = clampTo16bit(value);
    }
    
    // C2: Black Reference (20% of 0.70V ± 1%) with rise/fall
    const uint16_t c2_level = static_cast<uint16_t>(blanking_level_ + static_cast<int32_t>((white_level_ - blanking_level_) * 0.20));
    
    // C2 rise
    for (int32_t i = 0; i < c2_rise_time && (c2_start + i) < active_end; ++i) {
        int32_t sample = c2_start + i;
        double t = static_cast<double>(i) / static_cast<double>(c2_rise_time);
        int32_t value = blanking_level_ + static_cast<int32_t>((c2_level - blanking_level_) * t);
        line_buffer[sample] = clampTo16bit(value);
    }
    
    // C2 flat
    for (int32_t i = c2_rise_end; i < c2_fall_start && i < active_end; ++i) {
        line_buffer[i] = c2_level;
    }
    
    // C2 fall
    for (int32_t i = 0; i < c2_fall_time && (c2_fall_start + i) < active_end; ++i) {
        int32_t sample = c2_fall_start + i;
        double t = static_cast<double>(i) / static_cast<double>(c2_fall_time);
        int32_t value = c2_level - static_cast<int32_t>((c2_level - blanking_level_) * t);
        line_buffer[sample] = clampTo16bit(value);
    }
    
    // C3: Multiburst at specific frequencies (60% of 0.70V ± 1%)
    // Frequencies: 0.5, 1.3, 2.3, 4.2, 4.8, 5.8 MHz (±2%)
    // Centered around 50% gray with ±30% amplitude
    // No rise/fall envelope - just sine wave bursts
    const float frequencies[] = {0.5f, 1.3f, 2.3f, 4.2f, 4.8f, 5.8f};
    const int32_t num_bursts = sizeof(frequencies) / sizeof(frequencies[0]);
    int32_t c3_duration = c3_end - c3_start;
    const int32_t burst_duration = c3_duration / num_bursts;
    
    const int32_t gray_50pct = blanking_level_ + static_cast<int32_t>((white_level_ - blanking_level_) * 0.50);
    const int32_t burst_amplitude = static_cast<int32_t>((white_level_ - blanking_level_) * 0.30);
    
    // C3 multiburst with sine wave bursts
    int32_t pos = c3_start;
    for (int32_t b = 0; b < num_bursts && pos < c3_end; ++b) {
        double freq_hz = frequencies[b] * 1e6;  // MHz to Hz
        int32_t burst_end = pos + burst_duration;
        if (burst_end > c3_end) burst_end = c3_end;
        
        for (int32_t i = pos; i < burst_end; ++i) {
            // Time relative to start of this burst
            double time = static_cast<double>(i - pos) / sample_rate_;
            double phase = 2.0 * PI * freq_hz * time;
            double signal = std::sin(phase);
            
            int32_t value = gray_50pct + static_cast<int32_t>(signal * burst_amplitude);
            line_buffer[i] = clampTo16bit(value);
        }
        pos = burst_end;
    }
}

void PALVITSSignalGenerator::generateIEC60856Line332(uint16_t* line_buffer,
                                                     uint16_t line_number,
                                                     int32_t field_number) {
    // Line 332: Differential gain and phase
    // IEC 60856 Figure 9 timing (from ~5 to ~32 µs):
    //   ~5-11 µs:   B2 (white reference bar, 0.70V)
    //   ~11-13 µs:  B1 (2T sine-squared pulse, 200ns)
    //   ~13-15 µs:  gap
    //   ~15-31 µs:  D2 (6-step staircase with chroma subcarrier superimposed)
    //   ~31-32 µs:  return to blanking
    
    generateBaseLineWithBurst(line_buffer, line_number, field_number);
    
    const int32_t active_end = params_.active_video_end;      // 1107
    
    // Helper to convert µs duration to samples
    auto us_to_samples = [this](double us) {
        return static_cast<int32_t>(us * sample_rate_ / 1e6);
    };
    
    // Timing window (per IEC 60856, Figure 9):
    // Absolute timing from line start (0 µs = start of sync):
    // 10 → 22 µs: White bar (B2)
    // 22 → 26 µs: 2T pulse (B1)
    // 26 → 30 µs: Flat interval
    // 30 → 62 µs: Composite staircase (D2)
    // 62 → 64 µs: Return to blanking
    
    int32_t b2_start = us_to_samples(10.0);
    int32_t b2_rise_end = b2_start + us_to_samples(0.235);    // Rise time: 235 ns (Thomson filter)
    int32_t b2_end = us_to_samples(22.0);                     // B2 ends at 22 µs
    int32_t b2_fall_start = b2_end - us_to_samples(0.235);    // Fall starts 235 ns before end
    int32_t b2_rise_time = b2_rise_end - b2_start;
    int32_t b2_fall_time = b2_end - b2_fall_start;
    
    int32_t b1_start = us_to_samples(22.0);
    int32_t b1_width = us_to_samples(4.0);   // 22-26 µs = 4 µs
    
    int32_t d2_start = us_to_samples(30.0);
    int32_t d2_width = us_to_samples(32.0);  // 30-62 µs = 32 µs
    
    // B2: White Reference Bar (0.70 Vpp ± 0.5%) with 235 ns rise/fall (Thomson filter)
    // Rise from blanking level to white level over 235 ns
    for (int32_t i = 0; i < b2_rise_time; ++i) {
        int32_t sample = b2_start + i;
        if (sample >= active_end) break;
        
        double t = static_cast<double>(i) / static_cast<double>(b2_rise_time);
        int32_t value = blanking_level_ + static_cast<int32_t>((white_level_ - blanking_level_) * t);
        line_buffer[sample] = clampTo16bit(value);
    }
    
    // Flat portion at white level
    for (int32_t sample = b2_rise_end; sample < b2_fall_start && sample < active_end; ++sample) {
        line_buffer[sample] = static_cast<uint16_t>(white_level_);
    }
    
    // Fall: 235 ns
    for (int32_t i = 0; i < b2_fall_time; ++i) {
        int32_t sample = b2_fall_start + i;
        if (sample >= active_end) break;
        
        double t = static_cast<double>(i) / static_cast<double>(b2_fall_time);
        int32_t value = blanking_level_ + static_cast<int32_t>((white_level_ - blanking_level_) * (1.0 - t));
        line_buffer[sample] = clampTo16bit(value);
    }
    
    // B1: 2T Sine-Squared Pulse (200 ± 6 ns)
    for (int32_t i = 0; i < b1_width; ++i) {
        int32_t sample = b1_start + i;
        if (sample >= active_end) break;
        
        double t = static_cast<double>(i) / static_cast<double>(b1_width);
        double envelope = std::sin(PI * t);
        envelope = envelope * envelope;  // sin²(πt/T)
        
        int32_t value = blanking_level_ + static_cast<int32_t>((white_level_ - blanking_level_) * envelope);
        line_buffer[sample] = clampTo16bit(value);
    }
    
    // D2: Composite Staircase (6 levels) with Superimposed Chroma Subcarrier
    // Spec: Chroma amplitude 0.28V ± 5%, Phase 60° ± 5°
    // Rise/Fall: 1 µs ± 5% (~18 samples)
    const int32_t num_levels = 6;
    const int32_t step_width = d2_width / num_levels;
    const int32_t rise_time = 18;  // ~1 µs rise/fall time
    
    // Chroma parameters per spec
    double chroma_phase = 60.0 * PI / 180.0;  // 60° phase offset from burst
    
    // Chroma amplitude: 0.28V / 0.70V = 0.4 of luma range
    // But expressed as ±0.28V swing = ±40% of the 0.70V white level
    const int32_t luma_range = white_level_ - blanking_level_;
    const int32_t chroma_amp = static_cast<int32_t>(luma_range * 0.40);  // 0.28V / 0.70V
    
    int32_t pos = d2_start;
    int32_t d2_end = d2_start + d2_width;  // Calculate end position
    for (int32_t level = 0; level < num_levels && pos < d2_end; ++level) {
        // Target luminance level: evenly spaced from 0% to 100%
        float target_luma = static_cast<float>(level) / static_cast<float>(num_levels - 1);
        int32_t target_luma_value = blanking_level_ + static_cast<int32_t>(luma_range * target_luma);
        
        // Previous level for smooth transition
        int32_t prev_luma_value = (level > 0) ?
            blanking_level_ + static_cast<int32_t>(luma_range *
            (static_cast<float>(level - 1) / static_cast<float>(num_levels - 1))) :
            blanking_level_;
        
        // Rise time: linear ramp from previous to target luminance with chroma overlay
        for (int32_t r = 0; r < rise_time && pos < d2_end && pos < active_end; ++r) {
            double t = static_cast<double>(r) / static_cast<double>(rise_time);
            int32_t luma = prev_luma_value + static_cast<int32_t>((target_luma_value - prev_luma_value) * t);
            
            // Superimpose chroma subcarrier
            double phase = 2.0 * PI * (pos / samples_per_cycle_) + chroma_phase;
            double chroma_signal = std::sin(phase);
            int32_t value = luma + static_cast<int32_t>(chroma_signal * chroma_amp);
            line_buffer[pos++] = clampTo16bit(value);
        }
        
        // Flat portion at target level with chroma
        int32_t flat_width = step_width - rise_time;
        for (int32_t f = 0; f < flat_width && pos < d2_end && pos < active_end; ++f) {
            // Superimpose chroma subcarrier
            double phase = 2.0 * PI * (pos / samples_per_cycle_) + chroma_phase;
            double chroma_signal = std::sin(phase);
            int32_t value = target_luma_value + static_cast<int32_t>(chroma_signal * chroma_amp);
            line_buffer[pos++] = clampTo16bit(value);
        }
    }
}

void PALVITSSignalGenerator::generateIEC60856Line333(uint16_t* line_buffer,
                                                     uint16_t line_number,
                                                     int32_t field_number) {
    // Line 333: Chrominance amplitude and linearity
    // Contains: G1 (3-level chroma bars) + E (chroma reference)
    
    generateBaseLineWithBurst(line_buffer, line_number, field_number);
    
    const int32_t active_end = params_.active_video_end;
    
    // Helper to convert µs duration to samples
    auto us_to_samples = [this](double us) {
        return static_cast<int32_t>(us * sample_rate_ / 1e6);
    };
    
    // Timing window (per IEC 60856, Figure 10):
    // Absolute timing from line start (0 µs = start of sync):
    // 10 → 28 µs: Three-level chroma bar (G1)
    // 28 → 34 µs: Grey reference
    // 34 → 60 µs: Chroma reference (E)
    // 60 → 64 µs: Return to blanking
    
    int32_t g1_start = us_to_samples(10.0);
    int32_t g1_rise_end = us_to_samples(12.0);  // Rise ends at 12 µs (1 µs rise time from blanking to grey)
    int32_t g1_end = us_to_samples(28.0);       // G1 ends at 28 µs
    int32_t g1_rise_time = g1_rise_end - g1_start;
    int32_t g1_bar_width = g1_end - g1_rise_end;  // Remaining 16 µs for 3 bars
    const int32_t bar_width = g1_bar_width / 3;
    
    // G1: Three-Level Chrominance Bars (on 50% gray background)
    // Chroma amplitudes: 20%, 60%, 100% of 0.70V (±1%)
    // Grey background: 50% of 0.70V ± 1%
    // Rise/Fall time: 1µs ± 5% (~18 samples)
    const int32_t grey_level = blanking_level_ + static_cast<int32_t>((white_level_ - blanking_level_) * 0.5);
    const int32_t bar_rise_time = us_to_samples(1.0);  // ~1µs rise/fall between bar transitions
    
    int32_t pos = g1_start;
    
    // Initial rise time from blanking to grey level (10-12 µs absolute)
    for (int32_t i = 0; i < g1_rise_time && pos < g1_end; ++i) {
        if (pos >= active_end) break;
        
        double t = static_cast<double>(i) / static_cast<double>(g1_rise_time);
        
        // Rise to grey with minimal chroma during transition
        double phase = 2.0 * PI * (pos / samples_per_cycle_);
        double chroma_signal = std::sin(phase);
        int32_t chroma_amp = static_cast<int32_t>((white_level_ - blanking_level_) * 0.20 * 0.5);
        int32_t value = blanking_level_ + static_cast<int32_t>((grey_level - blanking_level_) * t) + 
                        static_cast<int32_t>(chroma_signal * chroma_amp * t);
        line_buffer[pos++] = clampTo16bit(value);
    }
    
    pos = g1_rise_end;  // Start bars after rise time
    
    // Chroma levels: 20%, 60%, 100% (as percentages of 0.70V)
    // When rendered as amplitude: ±10%, ±30%, ±50% around 50% gray
    const float chroma_percentages[] = {0.20f, 0.60f, 1.00f};
    
    for (int32_t bar = 0; bar < 3 && pos < g1_end; ++bar) {
        // Chroma amplitude for this bar
        int32_t chroma_amp = static_cast<int32_t>((white_level_ - blanking_level_) * chroma_percentages[bar] * 0.5);
        int32_t bar_end = pos + bar_width;
        if (bar_end > g1_end) bar_end = g1_end;
        
        // Rise time (transition from previous amplitude)
        if (bar > 0 && pos + bar_rise_time <= g1_end) {
            int32_t prev_amp = static_cast<int32_t>((white_level_ - blanking_level_) * chroma_percentages[bar - 1] * 0.5);
            for (int32_t r = 0; r < bar_rise_time && pos < g1_end && pos < active_end; ++r) {
                double t = static_cast<double>(r) / static_cast<double>(bar_rise_time);
                int32_t current_amp = prev_amp + static_cast<int32_t>((chroma_amp - prev_amp) * t);
                
                double phase = 2.0 * PI * (pos / samples_per_cycle_);
                double chroma_signal = std::sin(phase);
                int32_t value = grey_level + static_cast<int32_t>(chroma_signal * current_amp);
                line_buffer[pos++] = clampTo16bit(value);
            }
        }
        
        // Flat portion at this chroma amplitude
        for (int32_t i = pos; i < bar_end && i < active_end; ++i) {
            double phase = 2.0 * PI * (i / samples_per_cycle_);
            double chroma_signal = std::sin(phase);
            int32_t value = grey_level + static_cast<int32_t>(chroma_signal * chroma_amp);
            line_buffer[i] = clampTo16bit(value);
        }
        pos = bar_end;
    }
    
    // Grey reference: 28-34 µs absolute
    int32_t grey_start = us_to_samples(28.0);
    int32_t grey_rise_end = us_to_samples(29.0);  // 1 µs rise
    int32_t grey_end = us_to_samples(33.0);       // leaves 1 µs fall
    int32_t grey_rise_time = grey_rise_end - grey_start;
    int32_t grey_fall_start = grey_end;
    int32_t grey_fall_time = us_to_samples(1.0);
    
    // E: Chrominance Reference Signal: 34-60 µs absolute
    int32_t e_start = us_to_samples(34.0);
    int32_t e_rise_end = us_to_samples(35.0);     // 1 µs rise
    int32_t e_end = us_to_samples(59.0);          // leaves 1 µs fall
    int32_t e_rise_time = e_rise_end - e_start;
    int32_t e_fall_start = e_end;
    int32_t e_fall_time = us_to_samples(1.0);
    
    // Grey reference (50% gray at 0% chroma amplitude - just the luma level)
    // Rise: from blanking to grey level
    for (int32_t i = 0; i < grey_rise_time && (grey_start + i) < active_end; ++i) {
        int32_t sample = grey_start + i;
        double t = static_cast<double>(i) / static_cast<double>(grey_rise_time);
        int32_t value = blanking_level_ + static_cast<int32_t>((grey_level - blanking_level_) * t);
        line_buffer[sample] = clampTo16bit(value);
    }
    
    // Flat grey level
    for (int32_t sample = grey_rise_end; sample < grey_fall_start && sample < active_end; ++sample) {
        line_buffer[sample] = static_cast<uint16_t>(grey_level);
    }
    
    // Fall: from grey back to blanking
    for (int32_t i = 0; i < grey_fall_time && (grey_fall_start + i) < active_end; ++i) {
        int32_t sample = grey_fall_start + i;
        double t = static_cast<double>(i) / static_cast<double>(grey_fall_time);
        int32_t value = grey_level - static_cast<int32_t>((grey_level - blanking_level_) * t);
        line_buffer[sample] = clampTo16bit(value);
    }
    
    // E: Chrominance Reference Signal
    // Amplitude: 60% of 0.70V ± 1% (±30% around 50% gray)
    // Rise time: 1 µs
    const int32_t chroma_ref_amp = static_cast<int32_t>((white_level_ - blanking_level_) * 0.60 * 0.5);
    
    // E rise: from blanking to 50% gray with chroma
    for (int32_t i = 0; i < e_rise_time && (e_start + i) < active_end; ++i) {
        int32_t sample = e_start + i;
        double t = static_cast<double>(i) / static_cast<double>(e_rise_time);
        
        double phase = 2.0 * PI * (sample / samples_per_cycle_);
        double chroma_signal = std::sin(phase);
        int32_t value = blanking_level_ + static_cast<int32_t>((grey_level - blanking_level_) * t) + 
                        static_cast<int32_t>(chroma_signal * chroma_ref_amp * t);
        line_buffer[sample] = clampTo16bit(value);
    }
    
    // E flat: chroma reference at 50% gray
    for (int32_t i = e_rise_end; i < e_fall_start && i < active_end; ++i) {
        double phase = 2.0 * PI * (i / samples_per_cycle_);
        double chroma_signal = std::sin(phase);
        int32_t value = grey_level + static_cast<int32_t>(chroma_signal * chroma_ref_amp);
        line_buffer[i] = clampTo16bit(value);
    }
    
    // E fall: from 50% gray with chroma back to blanking
    for (int32_t i = 0; i < e_fall_time && (e_fall_start + i) < active_end; ++i) {
        int32_t sample = e_fall_start + i;
        double t = static_cast<double>(i) / static_cast<double>(e_fall_time);
        
        double phase = 2.0 * PI * (sample / samples_per_cycle_);
        double chroma_signal = std::sin(phase);
        int32_t value = grey_level - static_cast<int32_t>((grey_level - blanking_level_) * t) + 
                        static_cast<int32_t>(chroma_signal * chroma_ref_amp * (1.0 - t));
        line_buffer[sample] = clampTo16bit(value);
    }
}

} // namespace encode_orc
