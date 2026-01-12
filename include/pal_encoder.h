/*
 * File:        pal_encoder.h
 * Module:      encode-orc
 * Purpose:     PAL video signal encoder
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: 2026 Simon Inns
 */

#ifndef ENCODE_ORC_PAL_ENCODER_H
#define ENCODE_ORC_PAL_ENCODER_H

#include "field.h"
#include "frame_buffer.h"
#include "video_parameters.h"
#include <cstdint>
#include <cmath>

namespace encode_orc {

/**
 * @brief PAL video signal encoder
 * 
 * This class implements PAL composite video encoding including:
 * - Sync pulse generation
 * - YUV color encoding and subcarrier modulation
 * - PAL V-switch handling
 * - VBI (Vertical Blanking Interval) and VITS line generation
 */
class PALEncoder {
public:
    /**
     * @brief Construct a PAL encoder
     * @param params Video parameters for PAL
     */
    explicit PALEncoder(const VideoParameters& params);
    
    /**
     * @brief Encode a progressive frame to two interlaced PAL fields
     * @param frame_buffer Input frame in YUV444P16 format
     * @param field_number Starting field number (for V-switch calculation)
     * @return Frame containing two encoded PAL composite fields
     */
    Frame encode_frame(const FrameBuffer& frame_buffer, int32_t field_number);
    
    /**
     * @brief Encode a single field from half of a progressive frame
     * @param frame_buffer Input frame in YUV444P16 format
     * @param field_number Field number (for V-switch and line selection)
     * @param is_first_field true for first field (even lines), false for second (odd lines)
     * @return Encoded PAL composite field
     */
    Field encode_field(const FrameBuffer& frame_buffer, int32_t field_number, bool is_first_field);

private:
    VideoParameters params_;
    
    // PAL-specific constants
    static constexpr double PI = 3.141592653589793238463;
    static constexpr int32_t LINES_PER_FIELD = 313;      // PAL has 625 lines total (313 per field)
    static constexpr int32_t ACTIVE_LINES_START = 23;     // First active video line
    static constexpr int32_t ACTIVE_LINES_END = 310;      // Last active video line
    static constexpr int32_t VSYNC_LINES = 5;             // Number of vertical sync lines
    
    // Sync levels (in 16-bit samples)
    int32_t sync_level_;
    int32_t blanking_level_;
    int32_t black_level_;
    int32_t white_level_;
    
    // Subcarrier frequency and phase
    double subcarrier_freq_;
    double sample_rate_;
    double samples_per_cycle_;
    
    /**
     * @brief Generate horizontal sync pulse for a line
     * @param line_buffer Pointer to line data
     * @param line_number Line number within field (0-indexed)
     */
    void generate_sync_pulse(uint16_t* line_buffer, int32_t line_number);
    
    /**
     * @brief Generate color burst for a line
     * @param line_buffer Pointer to line data
     * @param line_number Line number in field (for phase calculation)
     * @param field_number Field number in sequence (for absolute line calculation)
     */
    void generate_color_burst(uint16_t* line_buffer, int32_t line_number, int32_t field_number);
    
    /**
     * @brief Encode active video line with PAL color subcarrier
     * @param line_buffer Pointer to line data
     * @param y_line Pointer to Y (luma) line data from frame buffer
     * @param u_line Pointer to U (chroma) line data from frame buffer
     * @param v_line Pointer to V (chroma) line data from frame buffer
     * @param line_number Line number in field (for absolute line calculation)
     * @param field_number Field number (for absolute line calculation)
     * @param width Width of active video in pixels
     */
    void encode_active_line(uint16_t* line_buffer, 
                           const uint16_t* y_line,
                           const uint16_t* u_line, 
                           const uint16_t* v_line,
                           int32_t line_number,
                           int32_t field_number,
                           int32_t width);
    
    /**
     * @brief Generate vertical sync line
     * @param line_buffer Pointer to line data
     * @param line_number Line number within field (0-indexed)
     */
    void generate_vsync_line(uint16_t* line_buffer, int32_t line_number);
    
    /**
     * @brief Generate blanking line (for VBI)
     * @param line_buffer Pointer to line data
     */
    void generate_blanking_line(uint16_t* line_buffer);
    
    /**
     * @brief Calculate PAL V-switch sign for a given field
     * @param field_number Field number (0-indexed)
     * @return +1 or -1 for PAL V-switch
     */
    int32_t get_v_switch(int32_t field_number) const {
        // PAL V-switch alternates every field (8 field sequence)
        // For simplicity, we use a 4-field sequence: +1, +1, -1, -1
        int32_t phase = field_number % 4;
        return (phase < 2) ? 1 : -1;
    }
    
    /**
     * @brief Clamp value to 16-bit unsigned range
     */
    static uint16_t clamp_to_16bit(int32_t value) {
        if (value < 0) return 0;
        if (value > 65535) return 65535;
        return static_cast<uint16_t>(value);
    }
    
    /**
     * @brief Convert YUV to composite PAL signal sample
     * @param y Luma value (16-bit)
     * @param u U chroma value (16-bit)
     * @param v V chroma value (16-bit)
     * @param phase Subcarrier phase (radians)
     * @param v_switch PAL V-switch (+1 or -1)
     * @return Composite video sample (16-bit)
     */
    uint16_t yuv_to_composite(uint16_t y, uint16_t u, uint16_t v, 
                             double phase, int32_t v_switch);
};

} // namespace encode_orc

#endif // ENCODE_ORC_PAL_ENCODER_H
