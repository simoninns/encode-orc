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
 * The generator follows ITU/SMPTE framing:
 * - 8 data bytes (timecode + user bits) sent LSB-first with "01" sync prefix
 * - One CRC byte (XOR of the serialized 80 raw bits) also with sync prefix
 * - Total of 90 bits encoded using Manchester coding.
 *
 * Waveform timing:
 * - Bit cell count: 115 per line (matches ld-decode VITC decoder expectation)
 * - Bits start shortly after colour burst; automatically clamped to the line.
 * - Levels: low at blanking (0 IRE); high at white (100 IRE).
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
                       int32_t line_number) const;

private:
    VideoParameters params_;

    int32_t black_level_;
    int32_t blanking_level_;
    int32_t white_level_;

    double samples_per_bit_;   // Derived from 115 bits across a line
    int32_t start_sample_;     // Start position for first bit (after burst)

    int32_t ire_to_sample(double ire) const;
    static uint8_t bcd(int value);
    void build_timecode_bytes(VideoSystem system, int32_t total_frame, uint8_t (&tc)[8]) const;
    void build_bitstream(const uint8_t tc[8], std::vector<uint8_t>& bits) const;
    void render_manchester(const std::vector<uint8_t>& bits, uint16_t* line_buffer) const;
};

} // namespace encode_orc

#endif // ENCODE_ORC_VITC_GENERATOR_H
