/*
 * File:        vitc_generator.cpp
 * Module:      encode-orc
 * Purpose:     VITC (Vertical Interval Time Code) line generator implementation
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: 2026 Simon Inns
 */

#include "vitc_generator.h"
#include "logging.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <vector>

namespace encode_orc {

namespace {
// VITC physical layer constants
constexpr double BIT_PERIOD_S = 0.5517e-6;      // 0.5517 µs per bit
constexpr int32_t TOTAL_BITS = 90;              // 90 bits per word
constexpr double EDGE_TIME_S = 200.0e-9;        // 200 ns nominal rise/fall
constexpr double LEAD_MARGIN_S = 11.2e-6;       // ≥11.2 µs after sync
constexpr double TRAIL_MARGIN_S = 1.9e-6;       // ≤1.9 µs before next sync
constexpr double POST_BURST_S = 1.0e-6;         // small guard after burst
constexpr double HIGH_SCALE = 550.0 / 700.0;    // +550 mV relative to 700 mV @100 IRE
}

VITCGenerator::VITCGenerator(const VideoParameters& params)
    : params_(params) {
    black_level_ = params_.black_16b_ire;
    blanking_level_ = params_.blanking_16b_ire;
    white_level_ = params_.white_16b_ire;

    samples_per_bit_ = std::max(2, static_cast<int32_t>(std::lround(params_.sample_rate * BIT_PERIOD_S)));
    total_bit_span_samples_ = samples_per_bit_ * TOTAL_BITS;

    rise_fall_samples_ = std::max(1, static_cast<int32_t>(std::lround(params_.sample_rate * EDGE_TIME_S)));
    if (rise_fall_samples_ > samples_per_bit_ / 2) {
        rise_fall_samples_ = std::max(1, samples_per_bit_ / 2);
    }

    // Choose start after burst and ≥11.2 µs from sync, while keeping tail within 1.9 µs of line end.
    int32_t after_burst = params_.colour_burst_end + static_cast<int32_t>(std::ceil(params_.sample_rate * POST_BURST_S));
    int32_t lead_margin = static_cast<int32_t>(std::ceil(params_.sample_rate * LEAD_MARGIN_S));
    int32_t trailing_margin = static_cast<int32_t>(std::ceil(params_.sample_rate * TRAIL_MARGIN_S));
    start_sample_ = std::max(after_burst, lead_margin);
    int32_t latest_start = std::max(0, params_.field_width - trailing_margin - total_bit_span_samples_);
    if (start_sample_ > latest_start) {
        start_sample_ = latest_start;
    }

    low_level_ = static_cast<uint16_t>(std::clamp(blanking_level_, 0, 65535));
    double luma_span = static_cast<double>(white_level_ - blanking_level_);
    int32_t high = static_cast<int32_t>(std::lround(static_cast<double>(blanking_level_) + (luma_span * HIGH_SCALE)));
    high_level_ = static_cast<uint16_t>(std::clamp(high, 0, 65535));
}

void VITCGenerator::build_vitc_bits(VideoSystem system,
                                     int32_t total_frame,
                                     bool is_second_field,
                                     std::vector<uint8_t>& bits) const {
    bits.assign(TOTAL_BITS, 0);

    const int fps = (system == VideoSystem::PAL) ? 25 : 30;
    const int frames = total_frame % fps;
    const int total_seconds = total_frame / fps;
    const int seconds = total_seconds % 60;
    const int total_minutes = total_seconds / 60;
    const int minutes = total_minutes % 60;
    const int hours = (total_minutes / 60) % 24;

    // Sync pairs: bit0=1, bit1=0 at every 10-bit boundary
    for (int i = 0; i < TOTAL_BITS; i += 10) {
        bits[i] = 1;
        bits[i + 1] = 0;
    }

    auto set_bcd_bits = [&bits](int value, std::initializer_list<int> positions) {
        int weight = 1;
        for (int pos : positions) {
            if (value & weight) bits[pos] = 1;
            weight <<= 1;
        }
    };

    // Frame number (00-24)
    set_bcd_bits(frames % 10, {2, 3, 4, 5});      // frame units weights 1,2,4,8
    set_bcd_bits(frames / 10, {12, 13});          // frame tens weights 1,2

    // Seconds (00-59)
    set_bcd_bits(seconds % 10, {22, 23, 24, 25}); // seconds units
    set_bcd_bits(seconds / 10, {32, 33, 34});     // seconds tens weights 1,2,4

    // Minutes (00-59)
    set_bcd_bits(minutes % 10, {42, 43, 44, 45}); // minutes units
    set_bcd_bits(minutes / 10, {52, 53, 54});     // minutes tens

    // Hours (00-23)
    set_bcd_bits(hours % 10, {62, 63, 64, 65});   // hours units
    set_bcd_bits(hours / 10, {72, 73});           // hours tens

    // Field mark bit 75: 0 for first field, 1 for second field (PAL)
    bits[75] = is_second_field ? 1 : 0;

    // Binary group flags (35,55) and user bits left at 0; reserved bits 14,74 and colour-lock 15 left at 0.

    // Compute CRC over bits 0-81, then place bits 82-89
    uint8_t crc = compute_crc(bits);
    for (int i = 0; i < 8; ++i) {
        bits[82 + i] = static_cast<uint8_t>((crc >> i) & 0x1);
    }
}

uint8_t VITCGenerator::compute_crc(const std::vector<uint8_t>& bits) {
    // Compute 8-bit CRC as per EBU Tech 3097 / SMPTE 12M spec.
    // Each CRC bit is the XOR of specific data bits (Hamming code parity).
    // Reference: Unai.VITC VITCLine.cs SetChecksum()
    
    uint8_t crc = 0;
    
    // Bit 82 (CRC bit 0): XOR of data bits 2, 10, 18, 26, 34, 42, 50, 58, 66, 74
    crc |= (bits[2] ^ bits[10] ^ bits[18] ^ bits[26] ^ bits[34] ^ bits[42] ^ bits[50] ^ bits[58] ^ bits[66] ^ bits[74]) << 0;
    
    // Bit 83 (CRC bit 1): XOR of data bits 3, 11, 19, 27, 35, 43, 51, 59, 67, 75
    crc |= (bits[3] ^ bits[11] ^ bits[19] ^ bits[27] ^ bits[35] ^ bits[43] ^ bits[51] ^ bits[59] ^ bits[67] ^ bits[75]) << 1;
    
    // Bit 84 (CRC bit 2): XOR of data bits 4, 12, 20, 28, 36, 44, 52, 60, 68, 76
    crc |= (bits[4] ^ bits[12] ^ bits[20] ^ bits[28] ^ bits[36] ^ bits[44] ^ bits[52] ^ bits[60] ^ bits[68] ^ bits[76]) << 2;
    
    // Bit 85 (CRC bit 3): XOR of data bits 5, 13, 21, 29, 37, 45, 53, 61, 69, 77
    crc |= (bits[5] ^ bits[13] ^ bits[21] ^ bits[29] ^ bits[37] ^ bits[45] ^ bits[53] ^ bits[61] ^ bits[69] ^ bits[77]) << 3;
    
    // Bit 86 (CRC bit 4): XOR of data bits 6, 14, 22, 30, 38, 46, 54, 62, 70, 78
    crc |= (bits[6] ^ bits[14] ^ bits[22] ^ bits[30] ^ bits[38] ^ bits[46] ^ bits[54] ^ bits[62] ^ bits[70] ^ bits[78]) << 4;
    
    // Bit 87 (CRC bit 5): XOR of data bits 7, 15, 23, 31, 39, 47, 55, 63, 71, 79
    crc |= (bits[7] ^ bits[15] ^ bits[23] ^ bits[31] ^ bits[39] ^ bits[47] ^ bits[55] ^ bits[63] ^ bits[71] ^ bits[79]) << 5;
    
    // Bit 88 (CRC bit 6): XOR of data bits 0, 8, 16, 24, 32, 40, 48, 56, 64, 72, 80
    crc |= (bits[0] ^ bits[8] ^ bits[16] ^ bits[24] ^ bits[32] ^ bits[40] ^ bits[48] ^ bits[56] ^ bits[64] ^ bits[72] ^ bits[80]) << 6;
    
    // Bit 89 (CRC bit 7): XOR of data bits 1, 9, 17, 25, 33, 41, 49, 57, 65, 73, 81
    crc |= (bits[1] ^ bits[9] ^ bits[17] ^ bits[25] ^ bits[33] ^ bits[41] ^ bits[49] ^ bits[57] ^ bits[65] ^ bits[73] ^ bits[81]) << 7;
    
    return crc;
}

void VITCGenerator::render_nrz(const std::vector<uint8_t>& bits, uint16_t* line_buffer) const {
    const int32_t buffer_size = params_.field_width;

    // NRZ (Non-Return to Zero) encoding per EBU Tech 3097 §2.1:
    // Each bit is rendered as a constant level for the entire bit period.
    // Transitions occur only when the data changes between adjacent bits.
    // Bit 0 → low_level (blanking)
    // Bit 1 → high_level (+550mV above blanking)
    //
    // Per §4: Transitions must have:
    // - Rise/fall time: 200ns ± 50ns
    // - Shape: sine-squared pulse edge
    // - Max overshoot: 5%

    // Lambda to write a transition with sine-squared shaping
    auto write_transition = [&](int32_t start, int32_t end, uint16_t from_level, uint16_t to_level) {
        if (start >= buffer_size || end <= start) return;
        
        int32_t ramp_start = std::max(0, start);
        int32_t ramp_end = std::min(end, buffer_size);
        int32_t ramp_len = ramp_end - ramp_start;
        
        if (ramp_len <= 0) return;
        
        const double start_level = static_cast<double>(from_level);
        const double end_level = static_cast<double>(to_level);
        
        for (int32_t i = 0; i < ramp_len; ++i) {
            // Normalized position [0, 1]
            double x = (ramp_len > 1) ? static_cast<double>(i) / static_cast<double>(ramp_len - 1) : 0.0;
            // Sine-squared shaping
            double s = std::sin(0.5 * M_PI * x);
            double y = s * s;
            double level = start_level + y * (end_level - start_level);
            line_buffer[ramp_start + i] = static_cast<uint16_t>(std::clamp(level, 0.0, 65535.0));
        }
    };

    // Render each bit with proper transitions
    uint16_t current_level = low_level_;  // Start at blanking (bit 0 level)
    
    // Fill initial region before VITC with blanking
    for (int32_t i = 0; i < start_sample_ && i < buffer_size; ++i) {
        line_buffer[i] = low_level_;
    }

    for (size_t i = 0; i < bits.size(); ++i) {
        int32_t bit_start = start_sample_ + static_cast<int32_t>(i) * samples_per_bit_;
        if (bit_start >= buffer_size) break;

        int32_t bit_end = std::min(bit_start + samples_per_bit_, buffer_size);
        uint16_t target_level = bits[i] ? high_level_ : low_level_;
        
        // If level changes, create transition at start of bit
        if (target_level != current_level) {
            int32_t transition_end = std::min(bit_start + rise_fall_samples_, bit_end);
            write_transition(bit_start, transition_end, current_level, target_level);
            
            // Fill rest of bit with target level
            for (int32_t sample = transition_end; sample < bit_end; ++sample) {
                line_buffer[sample] = target_level;
            }
            current_level = target_level;
        } else {
            // No transition needed, fill with current level
            for (int32_t sample = bit_start; sample < bit_end; ++sample) {
                line_buffer[sample] = current_level;
            }
        }
    }
}

void VITCGenerator::generate_line(VideoSystem system,
                                  int32_t total_frame,
                                  uint16_t* line_buffer,
                                  int32_t line_number,
                                  bool is_second_field) const {
    (void)line_number; // reserved for future use (e.g., line-dependent flags)

    std::vector<uint8_t> bits;
    build_vitc_bits(system, total_frame, is_second_field, bits);

    // Debug log the timecode
    const int fps = (system == VideoSystem::PAL) ? 25 : 30;
    int32_t frames = total_frame % fps;
    int32_t total_seconds = total_frame / fps;
    int32_t seconds = total_seconds % 60;
    int32_t total_minutes = total_seconds / 60;
    int32_t minutes = total_minutes % 60;
    int32_t hours = (total_minutes / 60) % 24;

    ENCODE_ORC_LOG_DEBUG("VITC frame {} line {}: timecode {}:{}:{}.{} (seq field {}), start {} samples",
                         total_frame, line_number, hours, minutes, seconds, frames,
                         is_second_field ? 2 : 1, start_sample_);

    render_nrz(bits, line_buffer);
}

void VITCGenerator::get_vitc_bits(VideoSystem system,
                                  int32_t total_frame,
                                  bool is_second_field,
                                  std::vector<uint8_t>& bits) const {
    build_vitc_bits(system, total_frame, is_second_field, bits);
}

} // namespace encode_orc
