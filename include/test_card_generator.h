/*
 * File:        test_card_generator.h
 * Module:      encode-orc
 * Purpose:     Test card and color bars generator
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: 2026 Simon Inns
 */

#ifndef ENCODE_ORC_TEST_CARD_GENERATOR_H
#define ENCODE_ORC_TEST_CARD_GENERATOR_H

#include "frame_buffer.h"
#include "video_parameters.h"
#include <cstdint>
#include <string>

namespace encode_orc {

/**
 * @brief Test card generator for various standard test patterns
 */
class TestCardGenerator {
public:
    /**
     * @brief Type of test card to generate
     */
    enum class Type {
        COLOR_BARS,      // EBU color bars for PAL, EIA color bars for NTSC
        PM5544,          // Philips PM5544 test card
        TESTCARD_F       // BBC Test Card F
    };
    
    /**
     * @brief Generate a test card
     * @param type Type of test card
     * @param params Video parameters (determines PAL/NTSC and dimensions)
     * @return Frame buffer containing the test card in YUV444P16 format
     */
    static FrameBuffer generate(Type type, const VideoParameters& params);
    
private:
    /**
     * @brief Generate EBU color bars (100/0/75/0)
     * Standard PAL color bars pattern
     */
    static FrameBuffer generate_ebu_bars(const VideoParameters& params);
    
    /**
     * @brief Generate EIA color bars
     * Standard NTSC color bars pattern
     */
    static FrameBuffer generate_eia_bars(const VideoParameters& params);
    
    /**
     * @brief Helper to convert RGB to YUV for 16-bit values
     */
    static void rgb_to_yuv16(uint16_t r, uint16_t g, uint16_t b,
                            uint16_t& y, uint16_t& u, uint16_t& v,
                            const VideoParameters& params);
};

} // namespace encode_orc

#endif // ENCODE_ORC_TEST_CARD_GENERATOR_H
