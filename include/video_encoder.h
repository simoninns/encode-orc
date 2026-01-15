/*
 * File:        video_encoder.h
 * Module:      encode-orc
 * Purpose:     Main video encoder coordinating test card generation and PAL/NTSC encoding
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: 2026 Simon Inns
 */

#ifndef ENCODE_ORC_VIDEO_ENCODER_H
#define ENCODE_ORC_VIDEO_ENCODER_H

#include "video_parameters.h"
#include "test_card_generator.h"
#include "pal_encoder.h"
#include "ntsc_encoder.h"
#include "tbc_writer.h"
#include "metadata_writer.h"
#include <string>
#include <cstdint>

namespace encode_orc {

/**
 * @brief Main video encoder class
 * 
 * Coordinates test card generation, PAL/NTSC encoding, and file output
 */
class VideoEncoder {
public:
    /**
     * @brief Encode video with test card
     * @param output_filename Output .tbc filename
     * @param system Video system (PAL or NTSC)
     * @param test_card_type Type of test card to generate
     * @param num_frames Number of frames to generate
     * @param verbose Enable verbose output
     * @param picture_start Starting CAV picture number (0 = not used)
     * @param chapter CLV chapter number (0 = not used)
     * @param timecode_start CLV timecode HH:MM:SS:FF (empty = not used)
     * @return true on success, false on error
     */
    bool encode_test_card(const std::string& output_filename,
                         VideoSystem system,
                         TestCardGenerator::Type test_card_type,
                         int32_t num_frames,
                         bool verbose = false,
                         int32_t picture_start = 0,
                         int32_t chapter = 0,
                         const std::string& timecode_start = "");
    
    /**
     * @brief Get error message from last operation
     */
    const std::string& get_error() const { return error_message_; }
    
private:
    std::string error_message_;
};

} // namespace encode_orc

#endif // ENCODE_ORC_VIDEO_ENCODER_H
