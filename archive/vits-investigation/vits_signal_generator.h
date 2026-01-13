/*
 * File:        vits_signal_generator.h
 * Module:      encode-orc
 * Purpose:     VITS (Vertical Interval Test Signal) generator base classes
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: 2026 Simon Inns
 */

#ifndef ENCODE_ORC_VITS_SIGNAL_GENERATOR_H
#define ENCODE_ORC_VITS_SIGNAL_GENERATOR_H

#include "video_parameters.h"
#include <cstdint>
#include <vector>
#include <memory>

namespace encode_orc {

/**
 * @brief VITS signal parameters
 * 
 * Common parameters used across different VITS signal types
 */
struct VITSSignalParams {
    uint16_t activeStartSample;   ///< Where signal content starts
    uint16_t activeEndSample;     ///< Where signal content ends
    uint16_t amplitude;           ///< Peak amplitude
    float    phase;               ///< Phase offset for chroma (radians)
    VideoSystem format;           ///< PAL or NTSC
};

/**
 * @brief Abstract base class for VITS signal generation
 * 
 * Defines the interface for all VITS signal generators.
 * Format-specific implementations (PAL, NTSC) inherit from this class.
 */
class VITSSignalGeneratorBase {
public:
    virtual ~VITSSignalGeneratorBase() = default;
    
    /**
     * @brief Generate white reference signal (100 IRE)
     * @param line_buffer Output buffer for one line (must be pre-allocated)
     * @param line_number Line number for phase calculation
     * @param field_number Field number for V-switch
     */
    virtual void generateWhiteReference(uint16_t* line_buffer,
                                       uint16_t line_number,
                                       int32_t field_number) = 0;
    
    /**
     * @brief Generate 75% gray reference signal (75 IRE)
     * @param line_buffer Output buffer for one line
     * @param line_number Line number for phase calculation
     * @param field_number Field number for V-switch
     */
    virtual void generate75GrayReference(uint16_t* line_buffer,
                                        uint16_t line_number,
                                        int32_t field_number) = 0;
    
    /**
     * @brief Generate 50% gray reference signal (50 IRE)
     * @param line_buffer Output buffer for one line
     * @param line_number Line number for phase calculation
     * @param field_number Field number for V-switch
     */
    virtual void generate50GrayReference(uint16_t* line_buffer,
                                        uint16_t line_number,
                                        int32_t field_number) = 0;
    
    /**
     * @brief Generate black reference signal (0 IRE)
     * @param line_buffer Output buffer for one line
     * @param line_number Line number for phase calculation
     * @param field_number Field number for V-switch
     */
    virtual void generateBlackReference(uint16_t* line_buffer,
                                       uint16_t line_number,
                                       int32_t field_number) = 0;
    
    /**
     * @brief Generate color burst signal
     * @param line_buffer Output buffer for one line
     * @param line_number Line number (for phase calculation)
     * @param field_number Field number (for V-switch in PAL)
     */
    virtual void generateColorBurst(uint16_t* line_buffer, 
                                   uint16_t line_number,
                                   int32_t field_number) = 0;
    
    /**
     * @brief Generate multiburst frequency response test signal
     * @param line_buffer Output buffer for one line
     * @param frequencies Vector of test frequencies in MHz
     * @param line_number Line number for phase calculation
     * @param field_number Field number for V-switch
     */
    virtual void generateMultiburst(uint16_t* line_buffer,
                                   const std::vector<float>& frequencies,
                                   uint16_t line_number,
                                   int32_t field_number) = 0;
    
    /**
     * @brief Generate staircase luminance test signal
     * @param line_buffer Output buffer for one line
     * @param numSteps Number of steps (typically 6-8)
     * @param line_number Line number for phase calculation
     * @param field_number Field number for V-switch
     */
    virtual void generateStaircase(uint16_t* line_buffer,
                                  uint8_t numSteps,
                                  uint16_t line_number,
                                  int32_t field_number) = 0;
    
    /**
     * @brief Get field width in samples
     */
    virtual uint16_t getFieldWidth() const = 0;
    
    /**
     * @brief Get active video start sample
     */
    virtual uint16_t getActiveVideoStart() const = 0;
    
    /**
     * @brief Get active video end sample
     */
    virtual uint16_t getActiveVideoEnd() const = 0;
};

/**
 * @brief PAL-specific VITS signal generator
 * 
 * Implements VITS signal generation for PAL format, including
 * LaserDisc-specific test signals (IEC 60856-1986).
 */
class PALVITSSignalGenerator : public VITSSignalGeneratorBase {
public:
    /**
     * @brief Construct PAL VITS generator
     * @param params PAL video parameters
     */
    explicit PALVITSSignalGenerator(const VideoParameters& params);
    
    // Implement base interface for PAL
    void generateWhiteReference(uint16_t* line_buffer,
                               uint16_t line_number,
                               int32_t field_number) override;
    void generate75GrayReference(uint16_t* line_buffer,
                                 uint16_t line_number,
                                 int32_t field_number) override;
    void generate50GrayReference(uint16_t* line_buffer,
                                 uint16_t line_number,
                                 int32_t field_number) override;
    void generateBlackReference(uint16_t* line_buffer,
                               uint16_t line_number,
                               int32_t field_number) override;
    
    void generateColorBurst(uint16_t* line_buffer, 
                          uint16_t line_number,
                          int32_t field_number) override;
    
    void generateMultiburst(uint16_t* line_buffer,
                          const std::vector<float>& frequencies,
                          uint16_t line_number,
                          int32_t field_number) override;
    
    void generateStaircase(uint16_t* line_buffer,
                         uint8_t numSteps,
                         uint16_t line_number,
                         int32_t field_number) override;
    
    uint16_t getFieldWidth() const override { return static_cast<uint16_t>(params_.field_width); }
    uint16_t getActiveVideoStart() const override { return static_cast<uint16_t>(params_.active_video_start); }
    uint16_t getActiveVideoEnd() const override { return static_cast<uint16_t>(params_.active_video_end); }
    
    // PAL LaserDisc-specific signals (IEC 60856-1986)
    
    /**
     * @brief Generate Insertion Test Signal (ITS) for LaserDisc
     * @param line_buffer Output buffer for one line
     * @param line_number Line number for phase calculation
     * @param field_number Field number for V-switch
     * 
     * ITS is a timing reference for LaserDisc playback head stability.
     * Exact specification from IEC 60856-1986 pages 20-21.
     */
    void generateInsertionTestSignal(uint16_t* line_buffer,
                                    uint16_t line_number,
                                    int32_t field_number);
    
    /**
     * @brief Generate differential gain and phase test signal
     * @param line_buffer Output buffer for one line
     * @param chromaAmplitude Chroma signal amplitude (typical: 0.3 for 300mV)
     * @param backgroundLuma Background luminance level (0.0-1.0)
     * @param line_number Line number for phase calculation
     * @param field_number Field number for V-switch
     * 
     * Tests chroma gain/phase variation across different luminance levels.
     * Used to measure differential gain (should be ≤ ±20%) and
     * differential phase (should be ≤ ±5°).
     */
    void generateDifferentialGainPhase(uint16_t* line_buffer,
                                      float chromaAmplitude,
                                      float backgroundLuma,
                                      uint16_t line_number,
                                      int32_t field_number);
    
    /**
     * @brief Generate cross-color distortion reference
     * @param line_buffer Output buffer for one line
     * @param line_number Line number for phase calculation
     * @param field_number Field number for V-switch
     * 
     * Chroma overlay on luminance transitions to detect cross-color artifacts.
     */
    void generateCrossColorReference(uint16_t* line_buffer,
                                    uint16_t line_number,
                                    int32_t field_number);
    
    // IEC 60856-1986 specific VITS signals
    
    /**
     * @brief Generate Line 19 signal: Luminance tests (B2, B1, F, D1)
     * @param line_buffer Output buffer for one line
     * @param line_number Line number for phase calculation
     * @param field_number Field number for V-switch
     * 
     * Contains:
     * - B2: White reference bar (0.70V ± 0.5%)
     * - B1: 2T sine-squared pulse (200ns width)
     * - F: 20T carrier-borne sine-squared pulse (2000ns width)
     * - D1: 6-level staircase
     */
    void generateIEC60856Line19(uint16_t* line_buffer,
                                uint16_t line_number,
                                int32_t field_number);
    
    /**
     * @brief Generate Line 20 signal: Frequency response (C1, C2, C3)
     * @param line_buffer Output buffer for one line
     * @param line_number Line number for phase calculation
     * @param field_number Field number for V-switch
     * 
     * Contains:
     * - C1: White reference (80% of 0.70V)
     * - C2: Black reference (20% of 0.70V)
     * - C3: Multiburst at 0.5, 1.3, 2.3, 4.2, 4.8, 5.8 MHz
     */
    void generateIEC60856Line20(uint16_t* line_buffer,
                                uint16_t line_number,
                                int32_t field_number);
    
    /**
     * @brief Generate Line 332 signal: Differential gain/phase (B2, B1, D2)
     * @param line_buffer Output buffer for one line
     * @param line_number Line number for phase calculation
     * @param field_number Field number for V-switch
     * 
     * Contains:
     * - B2: White reference bar
     * - B1: 2T sine-squared pulse
     * - D2: Composite staircase with superimposed chroma (0.28V subcarrier)
     */
    void generateIEC60856Line332(uint16_t* line_buffer,
                                 uint16_t line_number,
                                 int32_t field_number);
    
    /**
     * @brief Generate Line 333 signal: Chrominance tests (G1, E)
     * @param line_buffer Output buffer for one line
     * @param line_number Line number for phase calculation
     * @param field_number Field number for V-switch
     * 
     * Contains:
     * - G1: 3-level chrominance bars (20%, 60%, 100% of 0.70V)
     * - E: Chrominance reference (60% of 0.70V)
     */
    void generateIEC60856Line333(uint16_t* line_buffer,
                                 uint16_t line_number,
                                 int32_t field_number);

private:
    VideoParameters params_;
    
    // PAL-specific constants
    static constexpr double PI = 3.141592653589793238463;
    
    // Signal levels
    int32_t sync_level_;
    int32_t blanking_level_;
    int32_t black_level_;
    int32_t white_level_;
    
    // Subcarrier and timing
    double subcarrier_freq_;
    double sample_rate_;
    double samples_per_cycle_;
    
    /**
     * @brief Generate basic line structure (sync + blanking)
     * @param line_buffer Output buffer
     */
    void generateBaseLine(uint16_t* line_buffer);
    
    /**
     * @brief Generate basic line structure with color burst
     * @param line_buffer Output buffer
     * @param line_number Line number for phase calculation
     * @param field_number Field number for V-switch
     */
    void generateBaseLineWithBurst(uint16_t* line_buffer, 
                                   uint16_t line_number,
                                   int32_t field_number);
    
    /**
     * @brief Fill active video region with solid luminance level
     * @param line_buffer Output buffer
     * @param level Luminance level (0.0-1.0)
     */
    void fillActiveRegion(uint16_t* line_buffer, float level);
    
    /**
     * @brief Generate horizontal sync pulse
     * @param line_buffer Output buffer
     */
    void generateSyncPulse(uint16_t* line_buffer);
    
    /**
     * @brief Generate color burst (internal helper)
     * @param line_buffer Output buffer
     * @param line_number Line number for phase
     * @param field_number Field number for V-switch
     */
    void generateColorBurstInternal(uint16_t* line_buffer,
                                   uint16_t line_number,
                                   int32_t field_number);
    
    /**
     * @brief Calculate PAL V-switch sign for a given field
     * @param field_number Field number (0-indexed)
     * @return +1 or -1 for PAL V-switch
     */
    int32_t getVSwitch(int32_t field_number) const;
    
    /**
     * @brief Clamp value to 16-bit unsigned range
     */
    static uint16_t clampTo16bit(int32_t value);
};

} // namespace encode_orc

#endif // ENCODE_ORC_VITS_SIGNAL_GENERATOR_H
