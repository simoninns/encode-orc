/*
 * File:        biphase_vbi_generator.h
 * Module:      encode-orc
 * Purpose:     Pipeline generator for LaserDisc VBI frame numbers (lines 16-18)
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: 2026 Simon Inns
 */

#ifndef ENCODE_ORC_BIPHASE_VBI_GENERATOR_H
#define ENCODE_ORC_BIPHASE_VBI_GENERATOR_H

#include "pipeline_metadata_generator.h"
#include "biphase_encoder.h"
#include "video_parameters.h"
#include <vector>
#include <cstdint>

namespace encode_orc {

/**
 * @brief Pipeline generator for LaserDisc VBI frame numbers
 * 
 * Encodes 24-bit VBI data onto specified lines using biphase (Manchester)
 * encoding for LaserDisc formats.
 * 
 * Lines are absolute 0-indexed across the full frame.
 * NTSC: lines 16, 17, 18 (field 1) and 278, 279, 280 (field 2)
 * PAL: lines 17, 18, 19 (field 1) and 330, 331, 332 (field 2)
 */
class BiphaseVBIGenerator : public PipelineMetadataGenerator {
public:
    /**
     * @brief VBI encoding format
     */
    enum class VBIFormat {
        PictureNumber,  ///< Encode frame/picture numbers (CAV LaserDisc)
        Timecode        ///< Encode timecode (CLV LaserDisc)
    };
    
    /**
     * @brief Configuration for BiPhase VBI generator
     */
    struct Config {
        std::vector<int32_t> lines;  // Lines to encode VBI on (0-indexed)
        VBIFormat format;            // Picture numbers or timecode
        
        Config() : format(VBIFormat::PictureNumber) {}  // Default: lines must be specified in YAML config
    };
    
    /**
     * @brief Construct a biphase VBI generator
     * @param params Video parameters (for sample rate, timing, levels)
     * @param config Optional configuration (line placement)
     */
    explicit BiphaseVBIGenerator(const VideoParameters& params, const Config& config = Config());
    
    void apply(StructuredField& field, const MetadataContext& context) override;
    std::vector<int32_t> affected_lines() const override;
    std::string name() const override;
    
private:
    VideoParameters params_;
    Config config_;
    
    /**
     * @brief Encode biphase data onto a single line
     * @param line_buffer Line buffer to modify
     * @param vbi_value 24-bit VBI value to encode
     */
    void encode_biphase_on_line(uint16_t* line_buffer, int32_t vbi_value) const;
};

} // namespace encode_orc

#endif // ENCODE_ORC_BIPHASE_VBI_GENERATOR_H
