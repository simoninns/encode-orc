/*
 * File:        active_video_encoder.h
 * Module:      encode-orc
 * Purpose:     Abstract base class for active video encoding (YUV to composite)
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: 2026 Simon Inns
 */

#ifndef ENCODE_ORC_ACTIVE_VIDEO_ENCODER_H
#define ENCODE_ORC_ACTIVE_VIDEO_ENCODER_H

#include "video_parameters.h"
#include <cstdint>
#include <vector>
#include <memory>

namespace encode_orc {

/**
 * @brief Abstract base class for active video encoding (YUV to composite)
 * 
 * This class handles the encoding of active video (color) portion of each line,
 * converting YUV to composite PAL/NTSC signals with appropriate subcarrier modulation.
 * 
 * This is Phase 5 of the architecture refactoring - extraction of the smallest,
 * most testable component from the monolithic encoders.
 */
class ActiveVideoEncoder {
public:
    virtual ~ActiveVideoEncoder() = default;
    
    /**
     * @brief Encode a single line of active video
     * 
     * @param line_buffer Output buffer (will be written at samples [active_start, active_end))
     * @param y_line Luma component (one sample per pixel)
     * @param u_line Chroma U component (one sample per pixel)
     * @param v_line Chroma V component (one sample per pixel)
     * @param line_number Field line number (0-312 for field1, 0-313 for field2 in PAL)
     * @param field_number Absolute field number (for V-switch and phase calculation)
     * @param is_first_field true if this is field1, false for field2
     * @param width Width of the video (pixels)
     * @param studio_range_input true if input is studio range (0-1023), false for full-range (0-65535)
     */
    virtual void encode_active_line(uint16_t* line_buffer,
                                   const uint16_t* y_line,
                                   const uint16_t* u_line,
                                   const uint16_t* v_line,
                                   int32_t line_number,
                                   int32_t field_number,
                                   bool is_first_field,
                                   int32_t width,
                                   bool studio_range_input) = 0;
    
    /**
     * @brief Convert YUV sample to composite signal at given phase
     * 
     * @param y Luma component
     * @param u Chroma U component  
     * @param v Chroma V component
     * @param phase Subcarrier phase in radians
     * @param studio_range_input true if input is studio range (0-1023), false for full-range (0-65535)
     * @return Composite sample as 16-bit value
     */
    virtual uint16_t yuv_to_composite(uint16_t y, uint16_t u, uint16_t v,
                                      double phase, bool studio_range_input) = 0;
    
    /**
     * @brief Get the video parameters this encoder uses
     */
    virtual const VideoParameters& get_params() const = 0;
    
    /**
     * @brief Get the video system (PAL or NTSC)
     */
    virtual VideoSystem get_video_system() const = 0;
};

}  // namespace encode_orc

#endif  // ENCODE_ORC_ACTIVE_VIDEO_ENCODER_H
