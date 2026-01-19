/*
 * File:        manchester_encoder.cpp
 * Module:      encode-orc
 * Purpose:     Shared Manchester (biphase) encoder implementation
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: 2026 Simon Inns
 */

#include "manchester_encoder.h"
#include <algorithm>
#include <cmath>

namespace encode_orc {

void ManchesterEncoder::add_transition(uint16_t* line_buffer,
                                       int32_t buffer_size,
                                       int32_t start_pos,
                                       int32_t ramp_samples,
                                       uint16_t start_level,
                                       uint16_t end_level) {
    if (ramp_samples <= 0 || start_pos >= buffer_size) return;

    // Sine-squared edge: y = sin^2(pi/2 * x), x in [0,1]
    // This produces a more rounded, band-limited-looking transition.
    // Center x calculation to ensure symmetry around the middle of the ramp.
    for (int32_t i = 0; i < ramp_samples && (start_pos + i) < buffer_size; ++i) {
        // Map i to range [0, 1] centered at 0.5 for perfect symmetry
        double x = (ramp_samples > 1) ? (static_cast<double>(i)) / static_cast<double>(ramp_samples - 1)
                                       : (i == 0 ? 0.0 : 1.0);
        double s = std::sin(0.5 * M_PI * x);
        double y = s * s;
        double level = static_cast<double>(start_level) + y * (static_cast<double>(end_level) - static_cast<double>(start_level));
        line_buffer[start_pos + i] = static_cast<uint16_t>(std::clamp(level, 0.0, 65535.0));
    }
}

void ManchesterEncoder::fill_level(uint16_t* line_buffer,
                                   int32_t buffer_size,
                                   int32_t start_pos,
                                   int32_t end_pos,
                                   uint16_t level) {
    if (start_pos >= buffer_size) return;
    int32_t actual_end = std::min(end_pos, buffer_size);
    for (int32_t i = start_pos; i < actual_end; ++i) {
        line_buffer[i] = level;
    }
}

void ManchesterEncoder::render_bit(bool bit_value,
                                   int32_t bit_pos,
                                   int32_t samples_per_bit,
                                   uint16_t low_level,
                                   uint16_t high_level,
                                   int32_t rise_fall_samples,
                                   uint16_t* line_buffer,
                                   int32_t buffer_size) {
    if (bit_pos >= buffer_size || samples_per_bit <= 0) return;

    int32_t bit_center = bit_pos + samples_per_bit / 2;
    int32_t bit_end = bit_pos + samples_per_bit;

    if (bit_value) {
        // Bit 1: low->high transition at center
        // First half: low, second half: high
        if (rise_fall_samples > 0) {
            int32_t transition_center = bit_center;
            // Center the ramp symmetrically around bit_center using even/odd split
            int32_t ramp_before = rise_fall_samples / 2;           // samples before center
            int32_t ramp_after = rise_fall_samples - ramp_before;  // samples after center (accounts for odd ramp_samples)
            int32_t ramp_start = transition_center - ramp_before;
            int32_t ramp_end = transition_center + ramp_after;
            
            // Fill before ramp, ramp, fill after ramp
            fill_level(line_buffer, buffer_size, bit_pos, ramp_start, low_level);
            add_transition(line_buffer, buffer_size, ramp_start, rise_fall_samples, low_level, high_level);
            fill_level(line_buffer, buffer_size, ramp_end, bit_end, high_level);
        } else {
            fill_level(line_buffer, buffer_size, bit_pos, bit_center, low_level);
            fill_level(line_buffer, buffer_size, bit_center, bit_end, high_level);
        }
    } else {
        // Bit 0: high->low transition at center
        // First half: high, second half: low
        if (rise_fall_samples > 0) {
            int32_t transition_center = bit_center;
            // Center the ramp symmetrically around bit_center using even/odd split
            int32_t ramp_before = rise_fall_samples / 2;           // samples before center
            int32_t ramp_after = rise_fall_samples - ramp_before;  // samples after center (accounts for odd ramp_samples)
            int32_t ramp_start = transition_center - ramp_before;
            int32_t ramp_end = transition_center + ramp_after;
            
            // Fill before ramp, ramp, fill after ramp
            fill_level(line_buffer, buffer_size, bit_pos, ramp_start, high_level);
            add_transition(line_buffer, buffer_size, ramp_start, rise_fall_samples, high_level, low_level);
            fill_level(line_buffer, buffer_size, ramp_end, bit_end, low_level);
        } else {
            fill_level(line_buffer, buffer_size, bit_pos, bit_center, high_level);
            fill_level(line_buffer, buffer_size, bit_center, bit_end, low_level);
        }
    }
}

void ManchesterEncoder::render_bits(const std::vector<uint8_t>& bits,
                                    int32_t bit_start_pos,
                                    int32_t samples_per_bit,
                                    uint16_t low_level,
                                    uint16_t high_level,
                                    int32_t rise_fall_samples,
                                    uint16_t* line_buffer,
                                    int32_t buffer_size) {
    for (size_t i = 0; i < bits.size(); ++i) {
        int32_t bit_pos = bit_start_pos + static_cast<int32_t>(i) * samples_per_bit;
        if (bit_pos >= buffer_size) break;

        bool bit_value = (bits[i] != 0);
        render_bit(bit_value, bit_pos, samples_per_bit, low_level, high_level,
                   rise_fall_samples, line_buffer, buffer_size);
    }
}

} // namespace encode_orc
