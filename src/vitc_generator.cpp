/*
 * File:        vitc_generator.cpp
 * Module:      encode-orc
 * Purpose:     VITC (Vertical Interval Time Code) line generator implementation
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: 2026 Simon Inns
 */

#include "vitc_generator.h"
#include "manchester_encoder.h"
#include "logging.h"
#include <algorithm>
#include <cmath>
#include <cstdio>

namespace encode_orc {

VITCGenerator::VITCGenerator(const VideoParameters& params)
    : params_(params) {
    black_level_ = params_.black_16b_ire;
    blanking_level_ = params_.blanking_16b_ire;
    white_level_ = params_.white_16b_ire;

    // 115 bit cells across a line (per ITU/SMPTE guidance and ld-decode decoder expectations)
    samples_per_bit_ = static_cast<double>(params_.field_width) / 115.0;

    // Start a little after colour burst to avoid porch/burst interference
    start_sample_ = params_.colour_burst_end + static_cast<int32_t>(2.0 * params_.sample_rate / 1.0e6);
    int32_t bits_span = static_cast<int32_t>(std::ceil(90.0 * samples_per_bit_));
    if (start_sample_ + bits_span >= params_.field_width) {
        start_sample_ = std::max(0, params_.field_width - bits_span - 1);
    }
}

int32_t VITCGenerator::ire_to_sample(double ire) const {
    if (ire < -43.0) ire = -43.0;
    if (ire > 100.0) ire = 100.0;

    // Blanking is 0 IRE; below is sync region.
    if (ire < 0.0) {
        double sync_range = static_cast<double>(blanking_level_); // sync tip is assumed near 0
        return static_cast<int32_t>(blanking_level_ - ((-ire / 43.0) * sync_range));
    }
    double luma_range = static_cast<double>(white_level_ - blanking_level_);
    return static_cast<int32_t>(blanking_level_ + ((ire / 100.0) * luma_range));
}

uint8_t VITCGenerator::bcd(int value) {
    return static_cast<uint8_t>(((value / 10) << 4) | (value % 10));
}

void VITCGenerator::build_timecode_bytes(VideoSystem system, int32_t total_frame, uint8_t (&tc)[8]) const {
    const int fps = (system == VideoSystem::PAL) ? 25 : 30;

    int32_t frames = total_frame % fps;
    int32_t total_seconds = total_frame / fps;
    int32_t seconds = total_seconds % 60;
    int32_t total_minutes = total_seconds / 60;
    int32_t minutes = total_minutes % 60;
    int32_t hours = (total_minutes / 60) % 24;

    // Byte 0: frames units/tens, drop-frame flag (bit6) = 0, color-frame flag (bit7) = 0
    tc[0] = bcd(frames);

    // Byte 1: seconds units/tens, binary group flag 1 (bit7) = 0
    tc[1] = bcd(seconds);

    // Byte 2: minutes units/tens, binary group flag 2 (bit7) = 0
    tc[2] = bcd(minutes);

    // Byte 3: hours units/tens, flag bit7 = 0
    tc[3] = bcd(hours);

    // User bits (bytes 4-7) set to zero for now
    tc[4] = tc[5] = tc[6] = tc[7] = 0;
}

void VITCGenerator::build_bitstream(const uint8_t tc[8], std::vector<uint8_t>& bits) const {
    bits.clear();
    bits.reserve(90);

    auto append_byte = [&bits](uint8_t data) {
        // Ten bits per byte: sync bits 01 (LSB first) then 8 data bits (LSB first)
        uint16_t raw = static_cast<uint16_t>(data) << 2;
        raw |= 0x1; // LSB sync pattern '01'
        for (int i = 0; i < 10; ++i) {
            bits.push_back(static_cast<uint8_t>((raw >> i) & 0x1)); // LSB first
        }
    };

    // Append 8 data bytes
    for (int i = 0; i < 8; ++i) {
        append_byte(tc[i]);
    }

    // Compute CRC as XOR of serialized raw bits (first 80 bits)
    std::vector<uint8_t> crc_bytes;
    crc_bytes.reserve(12);
    uint8_t acc = 0;
    int bit_count = 0;
    for (int i = 0; i < 80; ++i) {
        acc |= (bits[i] & 0x1) << (bit_count % 8);
        ++bit_count;
        if ((bit_count % 8) == 0) {
            crc_bytes.push_back(acc);
            acc = 0;
        }
    }
    if ((bit_count % 8) != 0) {
        crc_bytes.push_back(acc);
    }

    uint8_t crc = 0;
    for (uint8_t b : crc_bytes) crc ^= b;

    append_byte(crc);
}

void VITCGenerator::render_manchester(const std::vector<uint8_t>& bits, uint16_t* line_buffer) const {
    // VITC amplitude: 0 to 100 IRE (blanking to white)
    const uint16_t low_level = static_cast<uint16_t>(std::clamp(blanking_level_, 0, 65535));
    const uint16_t high_level = static_cast<uint16_t>(std::clamp(white_level_, 0, 65535));

    // Determine rise/fall samples for ~50 ns (10–90%) using sine^2 edge shaping.
    // For sine^2, 10–90 width is ~0.590812 of the total ramp duration.
    const double ten_to_ninety_s = 50.0e-9; // nominal 50 ns
    const double ramp_time_s = ten_to_ninety_s / 0.590812; // total ramp duration
    int32_t rise_fall_samples = static_cast<int32_t>(std::round(params_.sample_rate * ramp_time_s));
    if (rise_fall_samples < 1) rise_fall_samples = 1;

    int32_t samples_per_bit = static_cast<int32_t>(std::round(samples_per_bit_));
    // Ensure ramp fits comfortably within half a bit cell
    int32_t max_ramp = std::max(1, samples_per_bit / 2);
    if (rise_fall_samples > max_ramp) rise_fall_samples = max_ramp;

    // Render with sine-squared edges
    ManchesterEncoder::render_bits(bits, start_sample_, samples_per_bit, low_level, high_level,
                                   rise_fall_samples, line_buffer, params_.field_width);
}

void VITCGenerator::generate_line(VideoSystem system,
                                  int32_t total_frame,
                                  uint16_t* line_buffer,
                                  int32_t line_number) const {
    (void)line_number; // Reserved for future flags

    uint8_t tc[8];
    build_timecode_bytes(system, total_frame, tc);

    std::vector<uint8_t> bits;
    build_bitstream(tc, bits);

    // Debug log the timecode
    const int fps = (system == VideoSystem::PAL) ? 25 : 30;
    int32_t frames = total_frame % fps;
    int32_t total_seconds = total_frame / fps;
    int32_t seconds = total_seconds % 60;
    int32_t total_minutes = total_seconds / 60;
    int32_t minutes = total_minutes % 60;
    int32_t hours = (total_minutes / 60) % 24;
    
    ENCODE_ORC_LOG_DEBUG("VITC frame {} line {}: timecode {}:{}:{}.{} (bytes: 0x{:02X} 0x{:02X} 0x{:02X} 0x{:02X} 0x{:02X} 0x{:02X} 0x{:02X} 0x{:02X})",
                        total_frame, line_number, hours, minutes, seconds, frames,
                        tc[0], tc[1], tc[2], tc[3], tc[4], tc[5], tc[6], tc[7]);

    render_manchester(bits, line_buffer);
}

} // namespace encode_orc
