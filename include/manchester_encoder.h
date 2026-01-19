/*
 * File:        manchester_encoder.h
 * Module:      encode-orc
 * Purpose:     Shared Manchester (biphase) encoder utilities for VBI and VITC
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: 2026 Simon Inns
 */

#ifndef ENCODE_ORC_MANCHESTER_ENCODER_H
#define ENCODE_ORC_MANCHESTER_ENCODER_H

#include <cstdint>
#include <vector>

namespace encode_orc {

/**
 * @brief Shared Manchester encoder utilities for video signal encoding
 * 
 * Provides low-level Manchester bit rendering used by both VBI (biphase)
 * and VITC encoders. Supports configurable bit durations, levels, and transitions.
 */
class ManchesterEncoder {
public:
    /**
     * @brief Render a sequence of bits into a line buffer using Manchester encoding
     * 
     * Manchester encoding represents bits as transitions at the center of each bit cell:
     * - Logical 1: low->high transition (starts low, ends high)
     * - Logical 0: high->low transition (starts high, ends low)
     * 
     * @param bits Vector of bit values (0 or 1)
     * @param bit_start_pos Sample position where first bit begins
     * @param samples_per_bit Samples allocated per bit cell
     * @param low_level Voltage level for "low" state (16-bit value)
     * @param high_level Voltage level for "high" state (16-bit value)
     * @param rise_fall_samples Number of samples for transition ramps (0 = instantaneous)
     * @param line_buffer Output buffer to render into
     * @param buffer_size Total size of line_buffer
     */
    static void render_bits(const std::vector<uint8_t>& bits,
                           int32_t bit_start_pos,
                           int32_t samples_per_bit,
                           uint16_t low_level,
                           uint16_t high_level,
                           int32_t rise_fall_samples,
                           uint16_t* line_buffer,
                           int32_t buffer_size);

    /**
     * @brief Render a single bit at a specific position
     * 
     * @param bit_value Logical bit value (0 or 1)
     * @param bit_pos Sample position where bit begins
     * @param samples_per_bit Samples allocated for this bit
     * @param low_level Low voltage level
     * @param high_level High voltage level
     * @param rise_fall_samples Samples for transition ramps
     * @param line_buffer Output buffer
     * @param buffer_size Buffer size
     */
    static void render_bit(bool bit_value,
                          int32_t bit_pos,
                          int32_t samples_per_bit,
                          uint16_t low_level,
                          uint16_t high_level,
                          int32_t rise_fall_samples,
                          uint16_t* line_buffer,
                          int32_t buffer_size);

private:
    /**
     * @brief Add a sine-squared shaped transition to a buffer
     * 
     * Uses y = sin^2(π/2 · x) easing from start_level → end_level
     * across ramp_samples, approximating a "sine squared" edge.
     * 
     * @param line_buffer Output buffer
     * @param buffer_size Buffer size
     * @param start_pos Starting sample position
     * @param ramp_samples Number of samples over which to ramp
     * @param start_level Starting voltage
     * @param end_level Ending voltage
     */
    static void add_transition(uint16_t* line_buffer,
                              int32_t buffer_size,
                              int32_t start_pos,
                              int32_t ramp_samples,
                              uint16_t start_level,
                              uint16_t end_level);

    /**
     * @brief Fill a range with a constant voltage level
     * 
     * @param line_buffer Output buffer
     * @param buffer_size Buffer size
     * @param start_pos Starting sample position
     * @param end_pos Ending sample position (exclusive)
     * @param level Voltage level to fill
     */
    static void fill_level(uint16_t* line_buffer,
                          int32_t buffer_size,
                          int32_t start_pos,
                          int32_t end_pos,
                          uint16_t level);
};

} // namespace encode_orc

#endif // ENCODE_ORC_MANCHESTER_ENCODER_H
