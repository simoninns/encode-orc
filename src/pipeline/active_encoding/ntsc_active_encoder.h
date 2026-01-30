/*
 * File:        ntsc_active_encoder.h
 * Module:      encode-orc
 * Purpose:     NTSC active video encoder (YIQ to composite)
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: 2026 Simon Inns
 */

#ifndef ENCODE_ORC_NTSC_ACTIVE_ENCODER_H
#define ENCODE_ORC_NTSC_ACTIVE_ENCODER_H

#include "active_video_encoder.h"
#include "video_parameters.h"
#include "fir_filter.h"
#include <cstdint>
#include <memory>
#include <vector>
#include <optional>

namespace encode_orc {

/**
 * @brief NTSC active video encoder
 * 
 * Implements Phase 5 active video encoding for NTSC:
 * - YIQ to composite conversion with subcarrier modulation
 * - No V-switch (unlike PAL) - pure 4-field color framing sequence
 * - Optional chroma (1.3 MHz) and luma low-pass filtering
 * 
 * The phase calculation follows ld-chroma-encoder's approach:
 * - 4-field sequence determines absolute phase position
 * - Uses 262.5 lines per field to preserve half-line offset between fields
 * - Cycles per line: 227.5 (for proper color framing)
 */
class NTSCActiveEncoder : public ActiveVideoEncoder {
public:
    /**
     * @brief Construct an NTSC active video encoder
     * @param params Video parameters for NTSC
     * @param enable_chroma_filter Enable 1.3 MHz low-pass filter on I/Q (default: true)
     * @param enable_luma_filter Enable low-pass filter on Y (default: false)
     */
    explicit NTSCActiveEncoder(const VideoParameters& params,
                              bool enable_chroma_filter = true,
                              bool enable_luma_filter = false);
    
    /**
     * @brief Encode a single line of active video (NTSC)
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
     * @brief Convert YIQ sample to NTSC composite at given phase
     * 
     * Note: For NTSC, u_line/v_line parameters are actually I/Q components
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
    VideoSystem get_video_system() const override { return VideoSystem::NTSC; }

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
     * @brief Calculate subcarrier phase at start of active video
     * 
     * NTSC uses 262.5 lines per field and 227.5 cycles per line
     * to achieve proper 4-field color framing.
     * 
     * @param line_number Field line number
     * @param field_number Absolute field number
     * @return Phase in radians
     */
    double calculate_phase(int32_t line_number, int32_t field_number) const;
    
    /**
     * @brief Clamp composite value to valid 16-bit range
     */
    uint16_t clamp_to_16bit(int32_t value) const;
};

}  // namespace encode_orc

#endif  // ENCODE_ORC_NTSC_ACTIVE_ENCODER_H
