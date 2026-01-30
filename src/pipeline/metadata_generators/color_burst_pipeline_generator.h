/*
 * File:        color_burst_pipeline_generator.h
 * Module:      encode-orc
 * Purpose:     Pipeline generator for color burst reference signal
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: 2026 Simon Inns
 */

#ifndef ENCODE_ORC_COLOR_BURST_PIPELINE_GENERATOR_H
#define ENCODE_ORC_COLOR_BURST_PIPELINE_GENERATOR_H

#include "pipeline_metadata_generator.h"
#include "color_burst_generator.h"
#include <memory>

namespace encode_orc {

/**
 * @brief Pipeline generator for color burst reference signal
 * 
 * Wraps ColorBurstGenerator to work as a pipeline component.
 * Adds color burst to all active video and VBI lines.
 */
class ColorBurstPipelineGenerator : public PipelineMetadataGenerator {
public:
    /**
     * @brief Configuration for color burst generator
     */
    struct Config {
        // Future: could add custom amplitude, phase override, etc.
    };
    
    /**
     * @brief Construct a color burst pipeline generator
     * @param params Video parameters
     * @param config Optional configuration
     */
    explicit ColorBurstPipelineGenerator(const VideoParameters& params, const Config& config = Config());
    
    void apply(StructuredField& field, const MetadataContext& context) override;
    std::vector<int32_t> affected_lines() const override;
    std::string name() const override;
    
private:
    std::unique_ptr<ColorBurstGenerator> burst_gen_;
    VideoParameters params_;
    Config config_;
};

} // namespace encode_orc

#endif // ENCODE_ORC_COLOR_BURST_PIPELINE_GENERATOR_H
