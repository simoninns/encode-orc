/*
 * File:        biphase_encoder.cpp
 * Module:      encode-orc
 * Purpose:     24-bit biphase encoder implementation
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: 2026 Simon Inns
 */

#include "biphase_encoder.h"
#include <cmath>

namespace encode_orc {

// Biphase encoding constants
static constexpr double BIT_DURATION_US = 2.0;  // 2.0 µs per bit
static constexpr int32_t TOTAL_BITS = 24;       // 24 bits total
static constexpr double RISE_FALL_TIME_NS = 225.0;  // 225 ns rise/fall time

// Helper function to add a transition (ramp) to the signal
static void add_transition(std::vector<uint16_t>& signal, int32_t start_pos, int32_t rise_fall_samples,
                          uint16_t start_level, uint16_t end_level) {
    for (int32_t i = 0; i < rise_fall_samples && (start_pos + i) < static_cast<int32_t>(signal.size()); ++i) {
        double position = static_cast<double>(i) / rise_fall_samples;
        double level = start_level + position * (end_level - start_level);
        signal[start_pos + i] = static_cast<uint16_t>(level);
    }
}

// Helper function to fill a range with a constant level
static void fill_level(std::vector<uint16_t>& signal, int32_t start_pos, int32_t end_pos, uint16_t level) {
    for (int32_t i = start_pos; i < end_pos && i < static_cast<int32_t>(signal.size()); ++i) {
        signal[i] = level;
    }
}

std::vector<uint16_t> BiphaseEncoder::encode(uint8_t byte0,
                                             uint8_t byte1,
                                             uint8_t byte2,
                                             double sample_rate,
                                             uint16_t high_level,
                                             uint16_t low_level) {
    // Combine three bytes into 24-bit value
    uint32_t value = (static_cast<uint32_t>(byte0) << 16) |
                     (static_cast<uint32_t>(byte1) << 8) |
                     static_cast<uint32_t>(byte2);
    
    // Total signal duration: 24 bits × 2.0 µs = 48 µs
    int32_t samples_per_bit = static_cast<int32_t>(sample_rate * BIT_DURATION_US * 1e-6);
    int32_t total_samples = samples_per_bit * TOTAL_BITS;
    int32_t rise_fall_samples = static_cast<int32_t>(sample_rate * RISE_FALL_TIME_NS * 1e-9);
    if (rise_fall_samples < 1) rise_fall_samples = 1;
    
    std::vector<uint16_t> signal(total_samples);
    
    // Manchester encoding: each bit has a transition at its center
    // Bit 1: low->high at center (starts low, ends high)
    // Bit 0: high->low at center (starts high, ends low)
    
    // Track current state (level at end of previous bit)
    bool current_state_high = false;  // Start low
    
    // Encode each bit, MSB first
    for (int32_t bit_index = TOTAL_BITS - 1; bit_index >= 0; --bit_index) {
        bool bit_value = (value >> bit_index) & 1;
        int32_t bit_start = (TOTAL_BITS - 1 - bit_index) * samples_per_bit;
        int32_t bit_center = bit_start + samples_per_bit / 2;
        
        if (bit_value) {
            // Bit 1: must have low->high transition at center
            // First half: low, second half: high
            
            // If we're starting high, transition to low at bit boundary
            if (current_state_high) {
                add_transition(signal, bit_start, rise_fall_samples, high_level, low_level);
                fill_level(signal, bit_start + rise_fall_samples, bit_center - rise_fall_samples / 2, low_level);
            } else {
                // Already low, stay low
                fill_level(signal, bit_start, bit_center - rise_fall_samples / 2, low_level);
            }
            
            // Transition to high at center
            add_transition(signal, bit_center - rise_fall_samples / 2, rise_fall_samples, low_level, high_level);
            fill_level(signal, bit_center + rise_fall_samples / 2, bit_start + samples_per_bit, high_level);
            
            current_state_high = true;  // End high
        } else {
            // Bit 0: must have high->low transition at center
            // First half: high, second half: low
            
            // If we're starting low, transition to high at bit boundary
            if (!current_state_high) {
                add_transition(signal, bit_start, rise_fall_samples, low_level, high_level);
                fill_level(signal, bit_start + rise_fall_samples, bit_center - rise_fall_samples / 2, high_level);
            } else {
                // Already high, stay high
                fill_level(signal, bit_start, bit_center - rise_fall_samples / 2, high_level);
            }
            
            // Transition to low at center
            add_transition(signal, bit_center - rise_fall_samples / 2, rise_fall_samples, high_level, low_level);
            fill_level(signal, bit_center + rise_fall_samples / 2, bit_start + samples_per_bit, low_level);
            
            current_state_high = false;  // End low
        }
    }
    
    return signal;
}

int32_t BiphaseEncoder::get_signal_duration_samples(double sample_rate) {
    // Total duration: 24 bits × 2.0 µs/bit = 48 µs
    double total_duration_s = TOTAL_BITS * BIT_DURATION_US * 1e-6;
    return static_cast<int32_t>(sample_rate * total_duration_s);
}

int32_t BiphaseEncoder::get_signal_start_position(double sample_rate, double line_period_h) {
    // According to spec: T = 0.188 H ± 0.003 H (normal case)
    // This is the time from start of line to start of biphase signal
    double start_time_s = 0.188 * line_period_h;
    return static_cast<int32_t>(sample_rate * start_time_s);
}

void BiphaseEncoder::encode_cav_picture_number(uint32_t frame_number,
                                               uint8_t& byte0,
                                               uint8_t& byte1,
                                               uint8_t& byte2) {
    // CAV picture numbers are encoded as:
    // 0xFxxxxx where xxxxx is BCD-encoded frame number (0-79999)
    // The first digit is masked to 0-7 range
    
    // Clamp frame number to valid range
    if (frame_number > 79999) {
        frame_number = 79999;
    }
    
    // Convert frame number to BCD (5 digits)
    uint32_t bcd = 0;
    uint32_t temp = frame_number;
    uint32_t shift = 0;
    
    // Encode each decimal digit as a 4-bit nibble
    while (temp > 0 || shift == 0) {
        uint32_t digit = temp % 10;
        bcd |= (digit << shift);
        temp /= 10;
        shift += 4;
        
        // Stop after 5 digits (20 bits)
        if (shift >= 20) break;
    }
    
    // Mask the first digit to 0-7 range and prepend 0xF
    uint32_t result = 0xF00000 | (bcd & 0x07FFFF);
    
    // Split into three bytes
    byte0 = (result >> 16) & 0xFF;  // MSB: 0xF + top digit
    byte1 = (result >> 8) & 0xFF;   // Middle 2 BCD digits
    byte2 = result & 0xFF;          // Lower 2 BCD digits
}

} // namespace encode_orc
