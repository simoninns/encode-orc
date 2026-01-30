/*
 * File:        color_burst_pipeline_generator.cpp
 * Module:      encode-orc
 * Purpose:     Pipeline generator for color burst reference signal
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: 2026 Simon Inns
 */

#include "color_burst_pipeline_generator.h"
#include "logging.h"

namespace encode_orc {

ColorBurstPipelineGenerator::ColorBurstPipelineGenerator(const VideoParameters& params, const Config& config)
    : burst_gen_(std::make_unique<ColorBurstGenerator>(params)),
      params_(params),
      config_(config) {
}

void ColorBurstPipelineGenerator::apply(StructuredField& field, const MetadataContext& context) {
    // Add color burst to all active video and VBI lines
    // (Color burst is needed on all lines except vsync lines)
    
    // Calculate burst amplitude (3/14 of luma range per PAL/NTSC spec)
    int32_t luma_range = params_.white_16b_ire - params_.black_16b_ire;
    int32_t burst_amplitude = static_cast<int32_t>((3.0 / 14.0) * luma_range);
    
    for (int32_t line = 0; line < field.field_data.height(); ++line) {
        LineType line_type = field.get_line_type(line);
        
        // Skip vsync lines (they don't have color burst)
        if (line_type == LineType::VSYNC) {
            continue;
        }
        
        // Generate color burst on this line
        uint16_t* line_buffer = field.field_data.line_data(line);
        
        if (context.system == VideoSystem::PAL) {
            burst_gen_->generate_pal_burst(
                line_buffer,
                line,
                context.field_number,
                params_.blanking_16b_ire,
                burst_amplitude
            );
        } else {
            burst_gen_->generate_ntsc_burst(
                line_buffer,
                line,
                context.field_number,
                params_.blanking_16b_ire,
                burst_amplitude
            );
        }
    }
}

std::vector<int32_t> ColorBurstPipelineGenerator::affected_lines() const {
    // Color burst affects all lines (empty vector means all lines)
    return std::vector<int32_t>();
}

std::string ColorBurstPipelineGenerator::name() const {
    return "ColorBurstPipelineGenerator";
}

} // namespace encode_orc
