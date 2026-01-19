/*
 * File:        vitc_generator.h
 * Module:      encode-orc
 * Purpose:     VITC (Vertical Interval Time Code) line generator for tape formats
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: 2026 Simon Inns
 */

#ifndef ENCODE_ORC_VITC_GENERATOR_H
#define ENCODE_ORC_VITC_GENERATOR_H

#include "video_parameters.h"
#include <cstdint>
#include <vector>

namespace encode_orc {

/**
 * @brief Generate VITC timecode waveform for NTSC and PAL tape formats.
 *
 * Key characteristics:
 * - 90 bits total with sync pairs (0,1) per byte, bits sent LSB-first
 * - BCD time fields packed per SMPTE: frames, seconds, minutes, hours, user bytes; field-mark in hours byte bit7; CRC over bits 0–81 (poly x^8+1)
 * - Biphase-mark waveform: mandatory mid-bit transition every bit; additional boundary transition only for logical 1
 * - Bit period 0.5517 µs; rise/fall ~200 ns; levels blanking to blanking+550 mV
 * - Start after colour burst and ≥11.2 µs from sync; end ≤1.9 µs before next sync
 */
class VITCGenerator {
public:
    explicit VITCGenerator(const VideoParameters& params);

    /**
     * @brief Render a VITC line into an existing line buffer.
     * @param system Video system (PAL/NTSC) for frame-rate and phase flags.
     * @param total_frame Frame index (0-based) to encode into the timecode.
     * @param line_buffer Output buffer for one line (already contains sync/burst/blanking).
     * @param line_number Line number within the field (0-indexed) used for any phase flags.
     */
    void generate_line(VideoSystem system,
                       int32_t total_frame,
                       uint16_t* line_buffer,
                       int32_t line_number,
                       bool is_second_field) const;

    // Get the 90 raw VITC bits without waveform rendering (for testing/debugging)
    void get_vitc_bits(VideoSystem system,
                       int32_t total_frame,
                       bool is_second_field,
                       std::vector<uint8_t>& bits) const;

private:
    VideoParameters params_;

    int32_t black_level_;
    int32_t blanking_level_;
    int32_t white_level_;

    int32_t samples_per_bit_;        // Samples per VITC bit
    int32_t total_bit_span_samples_; // Total span of all 90 bits
    int32_t start_sample_;           // Start position for bit 0 (after burst, within margins)
    int32_t rise_fall_samples_;      // Edge shaping samples
    uint16_t low_level_;             // Blanking level
    uint16_t high_level_;            // Blanking +550 mV level

    void build_vitc_bits(VideoSystem system,
                         int32_t total_frame,
                         bool is_second_field,
                         std::vector<uint8_t>& bits) const;
    static uint8_t compute_crc(const std::vector<uint8_t>& bits);
    void render_nrz(const std::vector<uint8_t>& bits, uint16_t* line_buffer) const;
};

} // namespace encode_orc

#endif // ENCODE_ORC_VITC_GENERATOR_H
