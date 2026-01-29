/*
 * File:        vbi_metadata_generator.h
 * Module:      encode-orc
 * Purpose:     Generate VBI metadata (vbi0, vbi1, vbi2 bytes) for LaserDisc formats
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: 2026 Simon Inns
 */

#ifndef ENCODE_ORC_VBI_METADATA_GENERATOR_H
#define ENCODE_ORC_VBI_METADATA_GENERATOR_H

#include "metadata.h"
#include "video_parameters.h"
#include "source_video_standard.h"
#include <cstdint>
#include <string>

namespace encode_orc {

/**
 * @brief Generates VBI metadata bytes for LaserDisc formats
 * 
 * Converts picture numbers, timecodes, and chapter numbers into the 6-byte
 * VBI format (vbi0, vbi1, vbi2 for field 1 and field 2).
 */
class VBIMetadataGenerator {
public:
    enum class Mode {
        CAV,              // Picture numbers
        CLVTimecode,      // Timecode
        CLVChapter,       // Chapter numbers
        None
    };
    
    /**
     * @brief Generate VBI data for a frame
     * @param frame_num Frame number in sequence
     * @param mode VBI mode (CAV, CLV timecode, CLV chapter)
     * @param picture_start Starting picture number for CAV
     * @param timecode_start_frame Starting timecode in frames for CLV
     * @param chapter Chapter number for CLV
     * @param fps Frame rate (25 for PAL, 30 for NTSC)
     * @param disc_area Disc area: "lead-in", "programme-area", or "lead-out"
     * @param field1 Output VBI data for field 1
     * @param field2 Output VBI data for field 2
     */
    static void generate_frame_vbi(
        int32_t frame_num,
        Mode mode,
        int32_t picture_start,
        int32_t timecode_start_frame,
        int32_t chapter,
        int32_t fps,
        const std::string& disc_area,
        VBIData& field1,
        VBIData& field2
    );
    
    /**
     * @brief Generate lead-in VBI data (convenience wrapper)
     */
    static void generate_leadin_vbi(VBIData& field1, VBIData& field2);
    
    /**
     * @brief Generate lead-out VBI data (convenience wrapper)
     */
    static void generate_leadout_vbi(VBIData& field1, VBIData& field2);
};

} // namespace encode_orc

#endif // ENCODE_ORC_VBI_METADATA_GENERATOR_H
