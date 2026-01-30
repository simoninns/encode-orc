/*
 * File:        vitc_pipeline_generator.cpp
 * Module:      encode-orc
 * Purpose:     Pipeline generator wrapper for VITC (consumer tape timecode)
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: 2026 Simon Inns
 */

#include "vitc_pipeline_generator.h"
#include "logging.h"

namespace encode_orc {

VITCPipelineGenerator::VITCPipelineGenerator(const VideoParameters& params, const Config& config)
    : vitc_gen_(std::make_unique<VITCGenerator>(params)), config_(config) {
}

void VITCPipelineGenerator::apply(StructuredField& field, const MetadataContext& context) {
    // Generate VITC on each configured line
    for (int32_t line : config_.lines) {
        if (line < 0 || line >= field.field_data.height()) {
            continue;  // Skip invalid lines
        }
        
        // Calculate frame number with offset
        int32_t frame_with_offset = context.total_frame + config_.start_frame_offset;
        
        // Generate VITC line
        vitc_gen_->generate_line(
            context.system,
            frame_with_offset,
            field.field_data.line_data(line),
            line,
            !context.is_first_field  // VITCGenerator expects is_second_field
        );
    }
}

std::vector<int32_t> VITCPipelineGenerator::affected_lines() const {
    return config_.lines;
}

std::string VITCPipelineGenerator::name() const {
    return "VITCPipelineGenerator";
}

} // namespace encode_orc
