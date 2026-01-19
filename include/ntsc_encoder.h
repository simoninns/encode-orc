/*
 * File:        ntsc_encoder.h
 * Module:      encode-orc
 * Purpose:     NTSC video signal encoder
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: 2026 Simon Inns
 */

#ifndef ENCODE_ORC_NTSC_ENCODER_H
#define ENCODE_ORC_NTSC_ENCODER_H

#include "field.h"
#include "frame_buffer.h"
#include "video_parameters.h"
#include "source_video_standard.h"
#include "ntsc_vits_generator.h"
#include "vitc_generator.h"
#include "fir_filter.h"
#include <cstdint>
#include <cmath>
#include <memory>
#include <optional>

namespace encode_orc {

/**
 * @brief NTSC video signal encoder
 * 
 * This class implements NTSC composite video encoding including:
 * - Sync pulse generation
 * - YIQ color encoding and subcarrier modulation
 * - NTSC field structure handling
 * - VBI (Vertical Blanking Interval) and VITS line generation
 */
class NTSCEncoder {
public:
    /**
     * @brief Construct an NTSC encoder
     * @param params Video parameters for NTSC
     * @param enable_chroma_filter Enable 1.3 MHz low-pass filter on I/Q (default: true)
     * @param enable_luma_filter Enable low-pass filter on Y (default: false)
     */
    explicit NTSCEncoder(const VideoParameters& params,
                        bool enable_chroma_filter = true,
                        bool enable_luma_filter = false);
    
    /**
     * @brief Encode a progressive frame to two interlaced NTSC fields
     * @param frame_buffer Input frame in YUV444P16 format (actually YIQ for NTSC)
     * @param field_number Starting field number
     * @param frame_number_for_vbi Frame number to encode in VBI (optional, -1 to disable)
     * @return Frame containing two encoded NTSC composite fields
     */
    Frame encode_frame(const FrameBuffer& frame_buffer, int32_t field_number, int32_t frame_number_for_vbi = -1);
    
    /**
     * @brief Encode a single field from half of a progressive frame
     * @param frame_buffer Input frame in YUV444P16 format (actually YIQ for NTSC)
     * @param field_number Field number
     * @param is_first_field true for first field (even lines), false for second (odd lines)
     * @param frame_number_for_vbi Frame number to encode in VBI (optional, -1 to disable)
     * @return Encoded NTSC composite field
     */
    Field encode_field(const FrameBuffer& frame_buffer, int32_t field_number, bool is_first_field, int32_t frame_number_for_vbi = -1);

    /**
     * @brief Enable VITS (Vertical Interval Test Signals)
     */
    void enable_vits();
    
    /**
     * @brief Disable VITS
     */
    void disable_vits();
    
    /**
     * @brief Check if VITS is enabled
     */
    bool is_vits_enabled() const;

    /**
     * @brief Set the LaserDisc standard (determines VBI/VITS/VITC behavior)
     * @param standard The LaserDisc standard to use
     * 
     * This method enables/disables VITS and VITC automatically based on the standard:
     * - LaserDisc standards (IEC60856/IEC60857) enable VITS and VBI
     * - Consumer tape enables VITC (no LaserDisc VBI)
     * - None disables both VITS and VITC
     */
    void set_source_video_standard(SourceVideoStandard standard);

    /**
     * @brief Enable VITC generation (tape formats)
     * @param start_frame_offset Frame offset to add to encoded timecode
     */
    void enable_vitc(int32_t start_frame_offset = 0);

    /**
     * @brief Disable VITC
     */
    void disable_vitc();
    
    /**
     * @brief Check if VITC is enabled
     */
    bool is_vitc_enabled() const;
    
    /**
     * @brief Encode frame to separate Y and C fields (for separate Y/C TBC output)
     * @param frame_buffer Input frame in YUV444P16 format (actually YIQ for NTSC)
     * @param field_number Starting field number
     * @param frame_number_for_vbi Frame number to encode in VBI (optional, -1 to disable)
     * @param y_field1 Output Y field 1
     * @param c_field1 Output C field 1
     * @param y_field2 Output Y field 2
     * @param c_field2 Output C field 2
     */
    void encode_frame_yc(const FrameBuffer& frame_buffer, int32_t field_number, 
                         int32_t frame_number_for_vbi,
                         Field& y_field1, Field& c_field1,
                         Field& y_field2, Field& c_field2);

private:
    VideoParameters params_;
    
    // VITS generator (optional)
    std::unique_ptr<NTSCVITSGenerator> vits_generator_;
    bool vits_enabled_;

    // VITC generator (optional)
    std::unique_ptr<VITCGenerator> vitc_generator_;
    bool vitc_enabled_ = false;
    int32_t vitc_start_frame_offset_ = 0;
    
    // Filters (optional)
    std::optional<FIRFilter> chroma_filter_;  // 1.3 MHz low-pass for I/Q
    std::optional<FIRFilter> luma_filter_;    // Optional low-pass for Y
    
    // NTSC-specific constants
    static constexpr double PI = 3.141592653589793238463;
    static constexpr int32_t LINES_PER_FIELD = 263;      // NTSC has 525 lines total (263 per field)
    static constexpr int32_t ACTIVE_LINES_START = 21;    // First active video line
    static constexpr int32_t ACTIVE_LINES_END = 261;     // Last active video line + 1 (240 active lines: 21-260)
    static constexpr int32_t VSYNC_LINES = 3;            // Number of vertical sync lines
    
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
     * @brief Generate color burst on chroma channel (centered at 32768)
     * @param line_buffer Pointer to line data
     * @param line_number Line number in field
     * @param field_number Field number in sequence
     */
    void generate_color_burst_chroma(uint16_t* line_buffer, int32_t line_number, int32_t field_number);
    
    /**
     * @brief Generate color burst on chroma channel for part of a line
     * @param line_buffer Pointer to line data
     * @param line_number Line number in field
     * @param field_number Field number in sequence
     * @param burst_end End position for burst (active video starts here)
     */
    void generate_color_burst_chroma_line(uint16_t* line_buffer, int32_t line_number, 
                                          int32_t field_number, int32_t burst_end);
    
    /**
     * @brief Encode active video line with NTSC color subcarrier
     * @param line_buffer Pointer to line data
     * @param y_line Pointer to Y (luma) line data from frame buffer
     * @param i_line Pointer to I (chroma) line data from frame buffer
     * @param q_line Pointer to Q (chroma) line data from frame buffer
     * @param line_number Line number in field (for absolute line calculation)
     * @param field_number Field number (for absolute line calculation)
     * @param width Width of active video in pixels
     * @param studio_range_input true if input is studio range (0-1023), false if full-range (0-65535)
     */
    void encode_active_line(uint16_t* line_buffer, 
                           const uint16_t* y_line,
                           const uint16_t* i_line, 
                           const uint16_t* q_line,
                           int32_t line_number,
                           int32_t field_number,
                           int32_t width,
                           bool studio_range_input = false);
    
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
     * @brief Generate VBI line with biphase frame number
     * @param line_buffer Pointer to line data
     * @param line_number Line number within field (0-indexed)
     * @param field_number Field number in sequence
     * @param frame_number Frame number to encode
     */
    void generate_biphase_vbi_line(uint16_t* line_buffer, int32_t line_number, 
                                   int32_t field_number, int32_t frame_number);
    
    /**
     * @brief Clamp a value to the valid signal range
     * @param value Input value
     * @return Clamped value
     */
    uint16_t clamp_signal(double value) const;
    
    /**
     * @brief Clamp value to 16-bit unsigned range
     */
    static uint16_t clamp_to_16bit(int32_t value) {
        if (value < 0) return 0;
        if (value > 65535) return 65535;
        return static_cast<uint16_t>(value);
    }
};

} // namespace encode_orc

#endif // ENCODE_ORC_NTSC_ENCODER_H
