/*
 * File:        audio_generator.h
 * Module:      encode-orc
 * Purpose:     Audio generation utilities
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: 2026 Simon Inns
 */

#ifndef ENCODE_ORC_AUDIO_GENERATOR_H
#define ENCODE_ORC_AUDIO_GENERATOR_H

#include <cstdint>
#include <vector>
#include <string>
#include <random>
#include <cmath>

namespace encode_orc {

/**
 * @brief Audio generator for various waveforms and noise types
 * 
 * Generates audio samples for different sound types including:
 * - Sine waves (pure tone)
 * - Square waves
 * - Sawtooth waves
 * - Pink noise
 * - White noise
 * - Brown noise (Brownian/red noise)
 */
class AudioGenerator {
public:
    enum class WaveformType {
        Sine,
        Square,
        Sawtooth,
        Pink,
        White,
        Brown
    };

    /**
     * @brief Generate audio samples for a given waveform type
     * @param type Waveform type to generate
     * @param num_samples Number of stereo samples to generate (total samples = num_samples * 2)
     * @param sample_rate Sample rate in Hz (e.g., 44100)
     * @param start_freq_hz Starting frequency in Hz (ignored for noise types)
     * @param end_freq_hz Ending frequency in Hz (ignored for noise types)
     * @param amplitude_percent Amplitude as percentage (0-100), default 75
     * @param seed Random seed for noise generation (0 = use random seed)
     * @param balance Stereo balance: -100 (left only), 0 (centered), +100 (right only), default 0
     * @return Vector of interleaved stereo 16-bit PCM samples (L, R, L, R, ...)
     */
    static std::vector<int16_t> generate(WaveformType type,
                                        int32_t num_samples,
                                        int32_t sample_rate,
                                        double start_freq_hz = 440.0,
                                        double end_freq_hz = 440.0,
                                        double amplitude_percent = 75.0,
                                        uint32_t seed = 0,
                                        double balance = 0.0);

    /**
     * @brief Generate silence
     * @param num_samples Number of stereo samples (total samples = num_samples * 2)
     * @return Vector of interleaved stereo 16-bit PCM samples (all zeros)
     */
    static std::vector<int16_t> generate_silence(int32_t num_samples);

    /**
     * @brief Convert waveform type string to enum
     * @param type_str String representation ("sine", "square", etc.)
     * @return WaveformType enum value
     * @throws std::invalid_argument if type_str is invalid
     */
    static WaveformType string_to_type(const std::string& type_str);

private:
    static constexpr double kAmplitude = 0.8 * 32767.0;  // 80% of max to avoid clipping
    static constexpr double kTwoPi = 6.28318530717958647692;

    static std::vector<int16_t> generate_sine(int32_t num_samples, int32_t sample_rate,
                                              double start_freq, double end_freq);
    static std::vector<int16_t> generate_square(int32_t num_samples, int32_t sample_rate,
                                                double start_freq, double end_freq);
    static std::vector<int16_t> generate_sawtooth(int32_t num_samples, int32_t sample_rate,
                                                  double start_freq, double end_freq);
    static std::vector<int16_t> generate_white_noise(int32_t num_samples, uint32_t seed);
    static std::vector<int16_t> generate_pink_noise(int32_t num_samples, uint32_t seed);
    static std::vector<int16_t> generate_brown_noise(int32_t num_samples, uint32_t seed);

    static int16_t clamp_int16(int32_t value);
};

}  // namespace encode_orc

#endif  // ENCODE_ORC_AUDIO_GENERATOR_H
