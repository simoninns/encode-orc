/*
 * File:        field_effect.cpp
 * Module:      encode-orc
 * Purpose:     Field effects implementation (Phase 6)
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: 2026 Simon Inns
 */

#include "field_effect.h"
#include <cmath>
#include <algorithm>
#include <random>
#include <cstring>

namespace encode_orc {

// ============================================================================
// NoiseGenerator Implementation
// ============================================================================

NoiseGenerator::NoiseGenerator(double noise_level_db)
    : noise_level_db_(noise_level_db), snr_db_(40.0), use_snr_mode_(false) {}

NoiseGenerator NoiseGenerator::from_snr(double snr_db) {
    NoiseGenerator gen;
    gen.set_snr_db(snr_db);
    return gen;
}

void NoiseGenerator::set_noise_level_db(double noise_level_db) {
    noise_level_db_ = noise_level_db;
    use_snr_mode_ = false;
}

void NoiseGenerator::set_snr_db(double snr_db) {
    snr_db_ = snr_db;
    use_snr_mode_ = true;
}

void NoiseGenerator::set_seed(uint32_t seed) {
    seed_ = seed;
}

double NoiseGenerator::calculate_noise_from_snr(double signal_rms, double snr_db) {
    // SNR_db = 20 * log10(signal_rms / noise_rms)
    // Rearranging: noise_rms = signal_rms / 10^(SNR_db / 20)
    return signal_rms / std::pow(10.0, snr_db / 20.0);
}

double NoiseGenerator::generate_gaussian_random() {
    // Box-Muller transform to generate Gaussian random numbers
    static thread_local std::mt19937 rng(seed_);
    static thread_local std::uniform_real_distribution<double> uniform(0.0, 1.0);
    
    double u1 = uniform(rng);
    double u2 = uniform(rng);
    
    // Avoid log(0)
    if (u1 < 1e-10) u1 = 1e-10;
    if (u2 < 1e-10) u2 = 1e-10;
    
    double z = std::sqrt(-2.0 * std::log(u1)) * std::cos(2.0 * M_PI * u2);
    return z;
}

void NoiseGenerator::apply(Field& field, const FieldEffectContext& context) {
    if (!enabled_) return;
    
    // Calculate noise RMS from SNR if in SNR mode
    double noise_rms = 0.0;
    
    if (use_snr_mode_) {
        // Calculate signal RMS from white and black levels
        // Assume linear signal: black at 0, white at max_value
        double signal_peak = (context.signal_level_white - context.signal_level_black) / 2.0;
        double signal_rms = signal_peak / std::sqrt(2.0);  // RMS of sine wave
        
        if (signal_rms > 0) {
            noise_rms = calculate_noise_from_snr(signal_rms, snr_db_);
        }
    } else {
        // Convert dB to linear scale
        // noise_linear = 10^(noise_db / 20)
        noise_rms = std::pow(10.0, noise_level_db_ / 20.0) * 32768.0;  // Normalized to 16-bit
    }
    
    // Seed a local RNG so each field gets reproducible noise
    std::mt19937 rng(seed_ + context.field_number);
    std::uniform_real_distribution<double> uniform(0.0, 1.0);
    auto gaussian_random = [&]() {
        // Box-Muller transform
        double u1 = uniform(rng);
        double u2 = uniform(rng);
        if (u1 < 1e-10) u1 = 1e-10;
        if (u2 < 1e-10) u2 = 1e-10;
        return std::sqrt(-2.0 * std::log(u1)) * std::cos(2.0 * M_PI * u2);
    };
    
    // Add Gaussian noise to all samples
    auto& data = field.data();
    for (auto& sample : data) {
        double gaussian = gaussian_random();
        double noise = gaussian * noise_rms;
        
        // Add noise and clamp to valid range [0, 65535]
        double noisy_sample = static_cast<double>(sample) + noise;
        noisy_sample = std::max(0.0, std::min(65535.0, noisy_sample));
        
        sample = static_cast<uint16_t>(noisy_sample + 0.5);  // Round to nearest
    }
}

// ============================================================================
// DropoutSimulator Implementation
// ============================================================================

DropoutSimulator::DropoutSimulator(double density, uint32_t seed)
    : pattern_(RANDOM), density_(density), seed_(seed) {}

void DropoutSimulator::add_dropout_line(int32_t line_number) {
    dropout_lines_.push_back(line_number);
}

bool DropoutSimulator::should_dropout_line(int32_t line_number) {
    static thread_local std::mt19937 rng(seed_);
    static thread_local std::uniform_real_distribution<double> uniform(0.0, 1.0);
    
    // Reseed for reproducibility
    rng.seed(seed_ + line_number);
    
    return uniform(rng) < density_;
}

void DropoutSimulator::apply(Field& field, const FieldEffectContext& /* context */) {
    if (!enabled_) return;
    
    // Constants for composite video blanking level (16-bit scale)
    const uint16_t blanking_level = 4096;  // ~0.3V in 16-bit scale
    
    auto& data = field.data();
    int32_t width = field.width();
    
    // Determine which lines to drop out based on pattern
    std::vector<int32_t> lines_to_dropout;
    
    switch (pattern_) {
        case RANDOM: {
            // Randomly select lines to drop out
            for (int32_t line = 0; line < field.height(); ++line) {
                if (should_dropout_line(line)) {
                    lines_to_dropout.push_back(line);
                }
            }
            break;
        }
        case PERIODIC: {
            // Drop every N-th line (density controls period)
            int32_t period = static_cast<int32_t>(1.0 / density_);
            for (int32_t line = 0; line < field.height(); line += period) {
                lines_to_dropout.push_back(line);
            }
            break;
        }
        case SPECIFIC_LINES: {
            lines_to_dropout = dropout_lines_;
            break;
        }
    }
    
    // Fill selected lines with blanking level
    for (int32_t line : lines_to_dropout) {
        if (line >= 0 && line < field.height()) {
            std::fill_n(&data[line * width], width, blanking_level);
        }
    }
}

// ============================================================================
// PhaseErrorSimulator Implementation
// ============================================================================

PhaseErrorSimulator::PhaseErrorSimulator(double phase_jitter_samples,
                                         double frequency_hz,
                                         uint32_t seed)
    : phase_jitter_samples_(phase_jitter_samples),
      frequency_hz_(frequency_hz),
      seed_(seed) {}

void PhaseErrorSimulator::set_phase_jitter(double phase_jitter_samples) {
    phase_jitter_samples_ = phase_jitter_samples;
}

void PhaseErrorSimulator::set_frequency(double frequency_hz) {
    frequency_hz_ = frequency_hz;
}

void PhaseErrorSimulator::apply(Field& field, const FieldEffectContext& context) {
    if (!enabled_) return;
    
    auto& data = field.data();
    int32_t width = field.width();
    int32_t height = field.height();
    
    // Create a seeded RNG for reproducible phase errors
    std::mt19937 rng(seed_ + context.field_number);
    std::normal_distribution<double> gaussian(0.0, phase_jitter_samples_);
    
    // Apply phase jitter to each line
    for (int32_t line = 0; line < height; ++line) {
        // Calculate phase modulation for this line
        double phase_mod = std::sin(2.0 * M_PI * frequency_hz_ * context.field_number +
                                    2.0 * M_PI * line / height);
        
        // Add random jitter
        double jitter = gaussian(rng);
        double total_shift = phase_mod * phase_jitter_samples_ + jitter;
        
        // Clamp shift to valid range
        int32_t pixel_shift = static_cast<int32_t>(total_shift);
        pixel_shift = std::max(-width + 1, std::min(width - 1, pixel_shift));
        
        if (pixel_shift == 0) continue;  // No shift needed
        
        // Apply horizontal shift to this line
        std::vector<uint16_t> line_data(data.begin() + line * width,
                                         data.begin() + (line + 1) * width);
        
        if (pixel_shift > 0) {
            // Shift right: move data right, fill left with blanking
            std::fill(data.begin() + line * width,
                     data.begin() + line * width + pixel_shift,
                     4096);  // Blanking level
            std::copy(line_data.begin(),
                     line_data.end() - pixel_shift,
                     data.begin() + line * width + pixel_shift);
        } else {
            // Shift left: move data left, fill right with blanking
            std::copy(line_data.begin() - pixel_shift,
                     line_data.end(),
                     data.begin() + line * width);
            std::fill(data.begin() + line * width + (width + pixel_shift),
                     data.begin() + (line + 1) * width,
                     4096);  // Blanking level
        }
    }
}

} // namespace encode_orc
