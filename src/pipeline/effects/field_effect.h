/*
 * File:        field_effect.h
 * Module:      encode-orc
 * Purpose:     Field effects abstract base class (Phase 6)
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: 2026 Simon Inns
 */

#ifndef ENCODE_ORC_FIELD_EFFECT_H
#define ENCODE_ORC_FIELD_EFFECT_H

#include "field.h"
#include <cstdint>
#include <string>
#include <memory>

namespace encode_orc {

/**
 * @brief Context passed to field effects during application
 */
struct FieldEffectContext {
    int32_t field_number = 0;      ///< Field number (0-based) for frame-dependent effects
    int32_t line_number = 0;       ///< Current line number within field
    bool is_first_field = true;    ///< Is this the first (odd) or second (even) field?
    double signal_level_white = 0.0;  ///< White level in current signal (for SNR calculation)
    double signal_level_black = 0.0;  ///< Black level in current signal (for SNR calculation)
};

/**
 * @brief Abstract base class for field effects (noise, dropouts, artifacts)
 * 
 * Field effects are applied AFTER active video encoding, allowing simulation of
 * tape artifacts (noise, dropouts, phase errors, head clog, etc.).
 * 
 * This is Phase 6 of the architecture refactoring.
 */
class FieldEffect {
public:
    virtual ~FieldEffect() = default;
    
    /**
     * @brief Apply the effect to a field
     * @param field The field to modify (in-place)
     * @param context Context information about the field
     */
    virtual void apply(Field& field, const FieldEffectContext& context) = 0;
    
    /**
     * @brief Get effect type name (for logging/debugging)
     */
    virtual std::string effect_type() const = 0;
    
    /**
     * @brief Check if this effect is enabled
     */
    virtual bool is_enabled() const { return enabled_; }
    
    /**
     * @brief Enable or disable the effect
     */
    virtual void set_enabled(bool enabled) { enabled_ = enabled; }
    
protected:
    bool enabled_ = true;
};

/**
 * @brief Gaussian noise generator for simulating tape noise
 * 
 * Adds Gaussian (white) noise to video samples. Can be configured either by:
 * - Direct noise level in dB relative to a reference signal
 * - SNR specification (Signal-to-Noise Ratio) which calculates noise from signal levels
 */
class NoiseGenerator : public FieldEffect {
public:
    /**
     * @brief Construct noise generator with specified noise level
     * @param noise_level_db Noise RMS amplitude in dB (default -40 dB for 40 dB SNR)
     */
    explicit NoiseGenerator(double noise_level_db = -40.0);
    
    /**
     * @brief Construct noise generator with SNR specification
     * @param snr_db Desired signal-to-noise ratio in dB
     * @return NoiseGenerator configured for specified SNR
     */
    static NoiseGenerator from_snr(double snr_db);
    
    /**
     * @brief Set noise level in dB
     * @param noise_level_db RMS noise amplitude in dB
     */
    void set_noise_level_db(double noise_level_db);
    
    /**
     * @brief Set desired SNR (will calculate noise level from signal)
     * @param snr_db Signal-to-Noise Ratio in dB
     * 
     * SNR is defined as: SNR_db = 20 * log10(signal_rms / noise_rms)
     * This calculates the required noise_rms from signal_rms and snr_db
     */
    void set_snr_db(double snr_db);
    
    /**
     * @brief Set random seed for reproducible noise
     * @param seed Random seed value
     */
    void set_seed(uint32_t seed);
    
    /**
     * @brief Apply Gaussian noise to the field
     */
    void apply(Field& field, const FieldEffectContext& context) override;
    
    /**
     * @brief Get effect type
     */
    std::string effect_type() const override { return "noise"; }
    
private:
    double noise_level_db_ = -40.0;
    double snr_db_ = 40.0;
    bool use_snr_mode_ = false;
    uint32_t seed_ = 42;
    
    /**
     * @brief Generate Gaussian random number with Box-Muller transform
     * @return Gaussian random number with mean=0, std=1
     */
    double generate_gaussian_random();
    
    /**
     * @brief Calculate RMS noise amplitude from SNR and signal level
     * @param signal_rms RMS amplitude of signal
     * @param snr_db Signal-to-Noise Ratio in dB
     * @return RMS noise amplitude
     */
    static double calculate_noise_from_snr(double signal_rms, double snr_db);
};

/**
 * @brief Dropout simulator for tape artifacts
 * 
 * Simulates tape dropouts by replacing specified lines or random lines
 * with blanking level (black).
 */
class DropoutSimulator : public FieldEffect {
public:
    enum DropoutPattern {
        RANDOM,          ///< Random dropouts with specified density
        PERIODIC,        ///< Periodic dropouts (every N lines)
        SPECIFIC_LINES   ///< Drop out specific line numbers
    };
    
    /**
     * @brief Construct dropout simulator with random pattern
     * @param density Dropout density (0.0 to 1.0, fraction of lines affected)
     * @param seed Random seed for reproducible patterns
     */
    DropoutSimulator(double density = 0.01, uint32_t seed = 42);
    
    /**
     * @brief Set random seed
     */
    void set_seed(uint32_t seed) { seed_ = seed; }
    
    /**
     * @brief Set dropout pattern
     */
    void set_pattern(DropoutPattern pattern) { pattern_ = pattern; }
    
    /**
     * @brief Set dropout density (for RANDOM pattern)
     */
    void set_density(double density) { density_ = density; }
    
    /**
     * @brief Add specific line number to drop out (for SPECIFIC_LINES pattern)
     */
    void add_dropout_line(int32_t line_number);
    
    /**
     * @brief Apply dropouts to the field
     */
    void apply(Field& field, const FieldEffectContext& context) override;
    
    /**
     * @brief Get effect type
     */
    std::string effect_type() const override { return "dropout"; }
    
private:
    DropoutPattern pattern_ = RANDOM;
    double density_ = 0.01;
    uint32_t seed_ = 42;
    std::vector<int32_t> dropout_lines_;
    
    /**
     * @brief Check if a line should be dropped out (for RANDOM pattern)
     */
    bool should_dropout_line(int32_t line_number);
};

/**
 * @brief Phase error simulator for VCR/tape playback effects
 * 
 * Simulates time-base errors (phase jitter) that occur during playback
 * from analog tape. Creates "wobble" in the video signal.
 */
class PhaseErrorSimulator : public FieldEffect {
public:
    /**
     * @brief Construct phase error simulator
     * @param phase_jitter_samples Maximum phase jitter in samples
     * @param frequency_hz Frequency of phase modulation
     * @param seed Random seed
     */
    PhaseErrorSimulator(double phase_jitter_samples = 10.0,
                       double frequency_hz = 1.0,
                       uint32_t seed = 42);
    
    /**
     * @brief Set phase jitter amplitude
     */
    void set_phase_jitter(double phase_jitter_samples);
    
    /**
     * @brief Set phase modulation frequency
     */
    void set_frequency(double frequency_hz);
    
    /**
     * @brief Apply phase errors to the field
     */
    void apply(Field& field, const FieldEffectContext& context) override;
    
    /**
     * @brief Get effect type
     */
    std::string effect_type() const override { return "phase-error"; }
    
private:
    double phase_jitter_samples_ = 10.0;
    double frequency_hz_ = 1.0;
    uint32_t seed_ = 42;
};

} // namespace encode_orc

#endif // ENCODE_ORC_FIELD_EFFECT_H
