/*
 * File:        biphase_encoder.h
 * Module:      encode-orc
 * Purpose:     24-bit biphase encoder for VBI frame numbers
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: 2026 Simon Inns
 */

#ifndef ENCODE_ORC_BIPHASE_ENCODER_H
#define ENCODE_ORC_BIPHASE_ENCODER_H

#include <cstdint>
#include <vector>

namespace encode_orc {

/**
 * @brief 24-bit biphase encoder for VBI frame numbers and timecode
 * 
 * Implements biphase (Manchester) encoding for VBI lines 16, 17, 18
 * according to the specification:
 * - Logical 1: positive (low-to-high) transition at center of bit cell
 * - Logical 0: negative (high-to-low) transition at center of bit cell
 * - Bit length: 2.0 µs per bit
 * - Total frame length: 48 µs (24 bits)
 * - MSB first transmission
 */
class BiphaseEncoder {
public:
    /**
     * @brief Encode three 8-bit values into biphase signal
     * @param byte0 First byte (MSB of 24-bit value)
     * @param byte1 Second byte (middle byte)
     * @param byte2 Third byte (LSB of 24-bit value)
     * @param sample_rate Sample rate in Hz
     * @param high_level High voltage level (in 16-bit scale)
     * @param low_level Low voltage level (in 16-bit scale)
     * @return Vector of samples representing the biphase-encoded signal
     */
    static std::vector<uint16_t> encode(uint8_t byte0,
                                        uint8_t byte1,
                                        uint8_t byte2,
                                        double sample_rate,
                                        uint16_t high_level,
                                        uint16_t low_level);
    
    /**
     * @brief Get the duration of the biphase signal in samples
     * @param sample_rate Sample rate in Hz
     * @return Number of samples for a complete 24-bit biphase signal
     */
    static int32_t get_signal_duration_samples(double sample_rate);
    
    /**
     * @brief Encode frame number as LaserDisc CAV picture number
     * @param frame_number Frame number to encode (0-79999)
     * @param byte0 Output: First byte (0xF + top digit)
     * @param byte1 Output: Second byte (middle 2 BCD digits)
     * @param byte2 Output: Third byte (lower 2 BCD digits)
     */
    static void encode_cav_picture_number(uint32_t frame_number,
                                          uint8_t& byte0,
                                          uint8_t& byte1,
                                          uint8_t& byte2);
    
    /**
     * @brief Get the start position for biphase signal on a line
     * @param sample_rate Sample rate in Hz
     * @param line_period_h Line period (H) in seconds
     * @return Sample position where biphase signal should start
     * 
     * According to spec: T = 0.188 H ± 0.003 H (normal case)
     */
    static int32_t get_signal_start_position(double sample_rate, double line_period_h);

private:
    /**
     * @brief Generate a single biphase bit
     * @param bit_value Logical bit value (0 or 1)
     * @param sample_rate Sample rate in Hz
     * @param high_level High voltage level
     * @param low_level Low voltage level
     * @return Vector of samples for this bit
     */
    static std::vector<uint16_t> encode_bit(bool bit_value,
                                            double sample_rate,
                                            uint16_t high_level,
                                            uint16_t low_level);
};

} // namespace encode_orc

#endif // ENCODE_ORC_BIPHASE_ENCODER_H
