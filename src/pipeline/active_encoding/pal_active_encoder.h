/*
 * File:        pal_active_encoder.h
 * Module:      encode-orc
 * Purpose:     PAL active video encoder (YUV to composite with V-switch)
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: 2026 Simon Inns
 */

#ifndef ENCODE_ORC_PAL_ACTIVE_ENCODER_H
#define ENCODE_ORC_PAL_ACTIVE_ENCODER_H

#include "active_video_encoder.h"
#include "video_parameters.h"
#include "fir_filter.h"
#include <cstdint>
#include <memory>
#include <vector>
#include <optional>

namespace encode_orc {

/**
 * @brief PAL active video encoder
 * 
 * Implements Phase 5 active video encoding for PAL:
 * - YUV to composite conversion with subcarrier modulation
 * - PAL V-switch handling (alternates V component sign each line)
 * - Optional chroma (1.3 MHz) and luma low-pass filtering
 * 
 * The phase calculation follows ld-chroma-encoder's approach:
 * - 8-field sequence determines absolute phase position
 * - V-switch alternates based on frame line number
 */
class PALActiveEncoder : public ActiveVideoEncoder {
public:
    /**
     * @brief Construct a PAL active video encoder
     * @param params Video parameters for PAL
     * @param enable_chroma_filter Enable 1.3 MHz low-pass filter on U/V (default: true)
     * @param enable_luma_filter Enable low-pass filter on Y (default: false)
     */
    explicit PALActiveEncoder(const VideoParameters& params,
                             bool enable_chroma_filter = true,
                             bool enable_luma_filter = false);
    
    /**
     * @brief Encode a single line of active video (PAL)
     */
    void encode_active_line(uint16_t* line_buffer,
                           const uint16_t* y_line,
                           const uint16_t* u_line,
                           const uint16_t* v_line,
                           int32_t line_number,
                           int32_t field_number,
                           bool is_first_field,
                           int32_t width,
                           bool studio_range_input,
                           uint16_t* y_buffer = nullptr,
                           uint16_t* c_buffer = nullptr) override;
    
    /**
     * @brief Convert YUV sample to PAL composite at given phase with V-switch
     */
    uint16_t yuv_to_composite(uint16_t y, uint16_t u, uint16_t v,
                              double phase, bool studio_range_input) override;
    
    /**
     * @brief Get video parameters
     */
    const VideoParameters& get_params() const override { return params_; }
    
    /**
     * @brief Get video system
     */
    VideoSystem get_video_system() const override { return VideoSystem::PAL; }

private:
    VideoParameters params_;
    std::optional<FIRFilter> chroma_filter_;
    std::optional<FIRFilter> luma_filter_;
    
    // Signal levels
    int32_t sync_level_;
    int32_t blanking_level_;
    int32_t black_level_;
    int32_t white_level_;
    
    // Subcarrier parameters
    double subcarrier_freq_;
    double sample_rate_;
    double samples_per_cycle_;
    
    /**
     * @brief Calculate V-switch value for PAL line
     * @param line_number Field line number
     * @param field_number Absolute field number
     * @param is_first_field true for field1, false for field2
     * @return 1 or -1 (V-switch alternation)
     */
    int32_t calculate_v_switch(int32_t line_number, int32_t field_number, bool is_first_field) const;
    
    /**
     * @brief Calculate subcarrier phase at start of active video
     * @param line_number Field line number
     * @param field_number Absolute field number
     * @param is_first_field true for field1, false for field2
     * @return Phase in radians
     */
    double calculate_phase(int32_t line_number, int32_t field_number, bool is_first_field) const;
    
    /**
     * @brief Clamp composite value to valid 16-bit range
     */
    uint16_t clamp_to_16bit(int32_t value) const;
};

}  // namespace encode_orc

#endif  // ENCODE_ORC_PAL_ACTIVE_ENCODER_H
