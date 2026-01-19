/*
 * File:        biphase_encoder.cpp
 * Module:      encode-orc
 * Purpose:     24-bit biphase encoder implementation
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: 2026 Simon Inns
 */

#include "biphase_encoder.h"
#include "manchester_encoder.h"
#include <cmath>

namespace encode_orc {

// Biphase encoding constants
static constexpr double BIT_DURATION_US = 2.0;  // 2.0 µs per bit
static constexpr int32_t TOTAL_BITS = 24;       // 24 bits total
static constexpr double RISE_FALL_TIME_NS = 225.0;  // 225 ns rise/fall time

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
    
    // Convert 24-bit value to bit vector (MSB first)
    std::vector<uint8_t> bits;
    for (int32_t bit_index = TOTAL_BITS - 1; bit_index >= 0; --bit_index) {
        bits.push_back((value >> bit_index) & 1);
    }
    
    // Use shared Manchester encoder to render the bits
    ManchesterEncoder::render_bits(bits, 0, samples_per_bit, low_level, high_level,
                                   rise_fall_samples, signal.data(), total_samples);
    
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
