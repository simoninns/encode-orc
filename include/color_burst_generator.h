/*
 * File:        color_burst_generator.h
 * Module:      encode-orc
 * Purpose:     Shared color burst generation utility for NTSC and PAL
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: 2026 Simon Inns
 */

#ifndef ENCODE_ORC_COLOR_BURST_GENERATOR_H
#define ENCODE_ORC_COLOR_BURST_GENERATOR_H

#include "video_parameters.h"
#include <cstdint>
#include <cmath>

namespace encode_orc {

/**
 * @brief Shared color burst generator for NTSC and PAL systems
 * 
 * Generates properly phase-locked color burst signals with system-specific
 * characteristics (fixed phase for NTSC, swinging burst for PAL).
 * 
 * This utility is used by both the encoders and VITS generators to ensure
 * consistent, correct color burst implementation across the system.
 */
class ColorBurstGenerator {
public:
    /**
     * @brief Construct a color burst generator
     * @param params Video parameters for the system (NTSC or PAL)
     */
    explicit ColorBurstGenerator(const VideoParameters& params);
    
    /**
     * @brief Generate color burst for NTSC
     * @param line_buffer Output buffer for one line
     * @param line_number Line number within field
     * @param field_number Field number in sequence
     */
    void generate_ntsc_burst(uint16_t* line_buffer, int32_t line_number, int32_t field_number);
    
    /**
     * @brief Generate color burst for PAL
     * @param line_buffer Output buffer for one line
     * @param line_number Line number within field
     * @param field_number Field number in sequence
     */
    void generate_pal_burst(uint16_t* line_buffer, int32_t line_number, int32_t field_number);

private:
    const VideoParameters& params_;
    
    // Signal levels (16-bit samples)
    int32_t sync_level_;
    int32_t blanking_level_;
    int32_t white_level_;
    
    // Subcarrier parameters
    double subcarrier_freq_;
    double sample_rate_;
    
    // Constants
    static constexpr double PI = 3.141592653589793238463;
    
    /**
     * @brief Calculate phase for NTSC
     * @param field_number Field number in sequence
     * @param line_number Line number within field
     * @param sample Sample position within line
     * @return Phase in radians
     */
    double calculate_ntsc_phase(int32_t field_number, int32_t line_number, int32_t sample) const;
    
    /**
     * @brief Calculate phase for PAL
     * @param field_number Field number in sequence
     * @param line_number Line number within field
     * @param sample Sample position within line
     * @return Phase in radians
     */
    double calculate_pal_phase(int32_t field_number, int32_t line_number, int32_t sample) const;
    
    /**
     * @brief Calculate PAL V-switch for a given field and line
     * @param field_number Field number
     * @param line_number Line number within field
     * @return +1 or -1 for V-switch
     */
    int32_t get_pal_v_switch(int32_t field_number, int32_t line_number) const;
    
    /**
     * @brief Calculate envelope shaping factor for burst amplitude
     * @param sample Current sample position within burst
     * @param burst_start Start of burst region
     * @param burst_end End of burst region
     * @param rise_time_ns Rise time in nanoseconds (10-90%)
     * @param fall_time_ns Fall time in nanoseconds (90-10%)
     * @return Envelope factor [0.0, 1.0]
     */
    double calculate_envelope(int32_t sample, int32_t burst_start, int32_t burst_end,
                             double rise_time_ns, double fall_time_ns) const;
    
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

#endif // ENCODE_ORC_COLOR_BURST_GENERATOR_H
