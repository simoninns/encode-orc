/*
 * File:        vitc_pipeline_generator.h
 * Module:      encode-orc
 * Purpose:     Pipeline generator wrapper for VITC (consumer tape timecode)
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: 2026 Simon Inns
 */

#ifndef ENCODE_ORC_VITC_PIPELINE_GENERATOR_H
#define ENCODE_ORC_VITC_PIPELINE_GENERATOR_H

#include "pipeline_metadata_generator.h"
#include "vitc_generator.h"
#include <vector>
#include <memory>

namespace encode_orc {

/**
 * @brief Pipeline generator for VITC timecode
 * 
 * Wraps the existing VITCGenerator to make it work as a pipeline component.
 * Encodes VITC timecode on specified lines (typically 19 and 21 for consumer tape).
 */
class VITCPipelineGenerator : public PipelineMetadataGenerator {
public:
    /**
     * @brief Configuration for VITC generator
     */
    struct Config {
        std::vector<int32_t> lines;  // Lines to encode VITC on (0-indexed)
        int32_t start_frame_offset;  // Frame number offset for timecode
        
        Config() : lines({18, 20}), start_frame_offset(0) {}
    };
    
    /**
     * @brief Construct a VITC pipeline generator
     * @param params Video parameters
     * @param config Optional configuration (line placement, offset)
     */
    explicit VITCPipelineGenerator(const VideoParameters& params, const Config& config = Config());
    
    void apply(StructuredField& field, const MetadataContext& context) override;
    std::vector<int32_t> affected_lines() const override;
    std::string name() const override;
    bool is_applicable(const MetadataContext& context) const override;
    
private:
    std::unique_ptr<VITCGenerator> vitc_gen_;
    Config config_;
};

} // namespace encode_orc

#endif // ENCODE_ORC_VITC_PIPELINE_GENERATOR_H
