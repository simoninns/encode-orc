/*
 * File:        blanking_encoder.h
 * Module:      encode-orc
 * Purpose:     Blanking-level video encoder for validation
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: 2026 Simon Inns
 */

#ifndef ENCODE_ORC_BLANKING_ENCODER_H
#define ENCODE_ORC_BLANKING_ENCODER_H

#include "field.h"
#include "video_parameters.h"
#include "metadata.h"
#include "tbc_writer.h"
#include "metadata_writer.h"
#include <string>

namespace encode_orc {

/**
 * @brief Encoder that generates blanking-level TBC output for validation
 * 
 * This encoder creates TBC files where all samples are set to the blanking
 * IRE level. This is useful for validating file formats, metadata schema,
 * and field structure before implementing complex encoding.
 */
class BlankingEncoder {
public:
    /**
     * @brief Construct a blanking encoder
     */
    BlankingEncoder() = default;
    
    /**
     * @brief Encode blanking-level video
     * @param output_filename Base filename (without extension)
     * @param system Video system (PAL or NTSC)
     * @param num_frames Number of frames to generate
     * @param verbose Enable verbose output
     * @return true on success, false on failure
     */
    bool encode(const std::string& output_filename, 
                VideoSystem system,
                int32_t num_frames,
                bool verbose = false);
    
    /**
     * @brief Get last error message
     */
    const std::string& get_error() const {
        return error_message_;
    }

private:
    /**
     * @brief Generate a field filled with blanking level
     */
    Field generate_blanking_field(const VideoParameters& params);
    
    std::string error_message_;
};

} // namespace encode_orc

#endif // ENCODE_ORC_BLANKING_ENCODER_H
