/*
 * File:        audio_generator.cpp
 * Module:      encode-orc
 * Purpose:     Audio generation utilities implementation
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: 2026 Simon Inns
 */

#include "audio_generator.h"
#include <stdexcept>
#include <algorithm>
#include <cstring>

namespace encode_orc {

std::vector<int16_t> AudioGenerator::generate(WaveformType type,
                                              int32_t num_samples,
                                              int32_t sample_rate,
                                              double start_freq_hz,
                                              double end_freq_hz,
                                              double amplitude_percent,
                                               uint32_t seed,
                                               double balance) {
    // Clamp amplitude to valid range [0, 100]
    amplitude_percent = std::max(0.0, std::min(100.0, amplitude_percent));
    double amplitude_scale = amplitude_percent / 100.0;
    
    // Clamp balance to valid range [-100, 100]
    balance = std::max(-100.0, std::min(100.0, balance));
    
    std::vector<int16_t> result;
    
    switch (type) {
        case WaveformType::Sine:
            result = generate_sine(num_samples, sample_rate, start_freq_hz, end_freq_hz);
            break;
        case WaveformType::Square:
            result = generate_square(num_samples, sample_rate, start_freq_hz, end_freq_hz);
            break;
        case WaveformType::Sawtooth:
            result = generate_sawtooth(num_samples, sample_rate, start_freq_hz, end_freq_hz);
            break;
        case WaveformType::White:
            result = generate_white_noise(num_samples, seed);
            break;
        case WaveformType::Pink:
            result = generate_pink_noise(num_samples, seed);
            break;
        case WaveformType::Brown:
            result = generate_brown_noise(num_samples, seed);
            break;
        default:
            throw std::invalid_argument("Unknown waveform type");
    }
    
    // Scale amplitude and apply balance
    // Balance: -100 = left only, 0 = centered, +100 = right only
    double left_gain = (balance <= 0.0) ? 1.0 : (100.0 - balance) / 100.0;
    double right_gain = (balance >= 0.0) ? 1.0 : (100.0 + balance) / 100.0;
    
    if (amplitude_scale != 1.0 || balance != 0.0) {
        for (size_t i = 0; i < result.size(); i += 2) {
            // Left channel
            result[i] = clamp_int16(static_cast<int32_t>(result[i] * amplitude_scale * left_gain));
            // Right channel
            if (i + 1 < result.size()) {
                result[i + 1] = clamp_int16(static_cast<int32_t>(result[i + 1] * amplitude_scale * right_gain));
            }
        }
    }
    
    return result;
}

std::vector<int16_t> AudioGenerator::generate_silence(int32_t num_samples) {
    return std::vector<int16_t>(num_samples * 2, 0);
}

AudioGenerator::WaveformType AudioGenerator::string_to_type(const std::string& type_str) {
    if (type_str == "sine") return WaveformType::Sine;
    if (type_str == "square") return WaveformType::Square;
    if (type_str == "sawtooth") return WaveformType::Sawtooth;
    if (type_str == "pink") return WaveformType::Pink;
    if (type_str == "white") return WaveformType::White;
    if (type_str == "brown") return WaveformType::Brown;
    throw std::invalid_argument("Unknown waveform type: " + type_str);
}

std::vector<int16_t> AudioGenerator::generate_sine(int32_t num_samples, int32_t sample_rate,
                                                   double start_freq, double end_freq) {
    std::vector<int16_t> output(num_samples * 2);
    
    double phase = 0.0;
    const bool is_sweep = (std::abs(start_freq - end_freq) > 0.001);
    
    for (int32_t i = 0; i < num_samples; ++i) {
        // Calculate frequency for this sample (linear sweep if needed)
        double freq = start_freq;
        if (is_sweep) {
            double t = static_cast<double>(i) / static_cast<double>(num_samples);
            freq = start_freq + (end_freq - start_freq) * t;
        }
        
        // Generate sample
        double phase_step = kTwoPi * freq / static_cast<double>(sample_rate);
        int16_t sample = static_cast<int16_t>(std::sin(phase) * kAmplitude);
        
        // Stereo (identical channels)
        output[i * 2] = sample;
        output[i * 2 + 1] = sample;
        
        // Update phase
        phase += phase_step;
        if (phase >= kTwoPi) {
            phase -= kTwoPi;
        }
    }
    
    return output;
}

std::vector<int16_t> AudioGenerator::generate_square(int32_t num_samples, int32_t sample_rate,
                                                     double start_freq, double end_freq) {
    std::vector<int16_t> output(num_samples * 2);
    
    double phase = 0.0;
    const bool is_sweep = (std::abs(start_freq - end_freq) > 0.001);
    
    for (int32_t i = 0; i < num_samples; ++i) {
        // Calculate frequency for this sample (linear sweep if needed)
        double freq = start_freq;
        if (is_sweep) {
            double t = static_cast<double>(i) / static_cast<double>(num_samples);
            freq = start_freq + (end_freq - start_freq) * t;
        }
        
        // Generate square wave: positive when phase < π, negative when phase >= π
        double phase_step = kTwoPi * freq / static_cast<double>(sample_rate);
        int16_t sample = (phase < M_PI) ? 
            static_cast<int16_t>(kAmplitude) : 
            static_cast<int16_t>(-kAmplitude);
        
        // Stereo (identical channels)
        output[i * 2] = sample;
        output[i * 2 + 1] = sample;
        
        // Update phase
        phase += phase_step;
        if (phase >= kTwoPi) {
            phase -= kTwoPi;
        }
    }
    
    return output;
}

std::vector<int16_t> AudioGenerator::generate_sawtooth(int32_t num_samples, int32_t sample_rate,
                                                       double start_freq, double end_freq) {
    std::vector<int16_t> output(num_samples * 2);
    
    double phase = 0.0;
    const bool is_sweep = (std::abs(start_freq - end_freq) > 0.001);
    
    for (int32_t i = 0; i < num_samples; ++i) {
        // Calculate frequency for this sample (linear sweep if needed)
        double freq = start_freq;
        if (is_sweep) {
            double t = static_cast<double>(i) / static_cast<double>(num_samples);
            freq = start_freq + (end_freq - start_freq) * t;
        }
        
        // Generate sawtooth wave: linear ramp from -1 to +1
        double phase_step = kTwoPi * freq / static_cast<double>(sample_rate);
        double normalized = (phase / kTwoPi) * 2.0 - 1.0;  // Map [0, 2π] to [-1, +1]
        int16_t sample = static_cast<int16_t>(normalized * kAmplitude);
        
        // Stereo (identical channels)
        output[i * 2] = sample;
        output[i * 2 + 1] = sample;
        
        // Update phase
        phase += phase_step;
        if (phase >= kTwoPi) {
            phase -= kTwoPi;
        }
    }
    
    return output;
}

std::vector<int16_t> AudioGenerator::generate_white_noise(int32_t num_samples, uint32_t seed) {
    std::vector<int16_t> output(num_samples * 2);
    
    std::mt19937 gen(seed == 0 ? std::random_device{}() : seed);
    std::normal_distribution<double> dist(0.0, kAmplitude * 0.3);  // 30% amplitude
    
    for (int32_t i = 0; i < num_samples; ++i) {
        int16_t sample = clamp_int16(static_cast<int32_t>(dist(gen)));
        output[i * 2] = sample;
        output[i * 2 + 1] = sample;
    }
    
    return output;
}

std::vector<int16_t> AudioGenerator::generate_pink_noise(int32_t num_samples, uint32_t seed) {
    std::vector<int16_t> output(num_samples * 2);
    
    std::mt19937 gen(seed == 0 ? std::random_device{}() : seed);
    std::uniform_real_distribution<double> dist(-1.0, 1.0);
    
    // Voss-McCartney algorithm for pink noise
    // Use 16 generators for good quality
    constexpr int num_generators = 16;
    double generators[num_generators] = {0};
    int32_t counter = 0;
    
    for (int32_t i = 0; i < num_samples; ++i) {
        // Update generators based on counter bits
        int32_t changed_bits = counter ^ (counter + 1);
        for (int g = 0; g < num_generators; ++g) {
            if (changed_bits & (1 << g)) {
                generators[g] = dist(gen);
            }
        }
        counter++;
        
        // Sum all generators
        double sum = 0.0;
        for (int g = 0; g < num_generators; ++g) {
            sum += generators[g];
        }
        
        // Normalize and scale
        int16_t sample = clamp_int16(static_cast<int32_t>(sum * kAmplitude * 0.1));
        output[i * 2] = sample;
        output[i * 2 + 1] = sample;
    }
    
    return output;
}

std::vector<int16_t> AudioGenerator::generate_brown_noise(int32_t num_samples, uint32_t seed) {
    std::vector<int16_t> output(num_samples * 2);
    
    std::mt19937 gen(seed == 0 ? std::random_device{}() : seed);
    std::normal_distribution<double> dist(0.0, 100.0);  // Small steps
    
    double value = 0.0;
    const double decay = 0.998;  // Slight decay to prevent drift
    
    for (int32_t i = 0; i < num_samples; ++i) {
        // Random walk (integrate white noise)
        value = value * decay + dist(gen);
        
        // Clamp to prevent excessive drift
        value = std::max(-10000.0, std::min(10000.0, value));
        
        int16_t sample = clamp_int16(static_cast<int32_t>(value));
        output[i * 2] = sample;
        output[i * 2 + 1] = sample;
    }
    
    return output;
}

int16_t AudioGenerator::clamp_int16(int32_t value) {
    if (value > 32767) return 32767;
    if (value < -32768) return -32768;
    return static_cast<int16_t>(value);
}

}  // namespace encode_orc
