/*
 * File:        biphase_vbi_generator.cpp
 * Module:      encode-orc
 * Purpose:     Pipeline generator for LaserDisc VBI frame numbers (lines 16-18)
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: 2026 Simon Inns
 */

#include "biphase_vbi_generator.h"
#include "logging.h"
#include <algorithm>

namespace encode_orc {

BiphaseVBIGenerator::BiphaseVBIGenerator(const VideoParameters& params, const Config& config)
    : params_(params), config_(config) {
}

void BiphaseVBIGenerator::apply(StructuredField& field, const MetadataContext& context) {
    // Only apply if we have VBI data
    if (!context.vbi_data) {
        return;
    }
    
    // Encode VBI data on the configured lines
    // Note: config_.lines are 0-indexed, VBI data fields map as:
    // vbi0 -> line 16 (1-indexed) = 15 (0-indexed)
    // vbi1 -> line 17 (1-indexed) = 16 (0-indexed)
    // vbi2 -> line 18 (1-indexed) = 17 (0-indexed)
    
    if (config_.lines.size() >= 1 && config_.lines[0] < field.field_data.height()) {
        encode_biphase_on_line(field.field_data.line_data(config_.lines[0]), 
                              context.vbi_data->vbi0);
    }
    
    if (config_.lines.size() >= 2 && config_.lines[1] < field.field_data.height()) {
        encode_biphase_on_line(field.field_data.line_data(config_.lines[1]), 
                              context.vbi_data->vbi1);
    }
    
    if (config_.lines.size() >= 3 && config_.lines[2] < field.field_data.height()) {
        encode_biphase_on_line(field.field_data.line_data(config_.lines[2]), 
                              context.vbi_data->vbi2);
    }
}

std::vector<int32_t> BiphaseVBIGenerator::affected_lines() const {
    return config_.lines;
}

std::string BiphaseVBIGenerator::name() const {
    return "BiphaseVBIGenerator";
}

void BiphaseVBIGenerator::encode_biphase_on_line(uint16_t* line_buffer, int32_t vbi_value) const {
    // Extract bytes from 24-bit value
    uint8_t byte0 = (vbi_value >> 16) & 0xFF;
    uint8_t byte1 = (vbi_value >> 8) & 0xFF;
    uint8_t byte2 = vbi_value & 0xFF;
    
    // Generate biphase signal
    auto biphase_samples = BiphaseEncoder::encode(
        byte0, byte1, byte2,
        params_.sample_rate,
        static_cast<uint16_t>(params_.white_16b_ire),
        static_cast<uint16_t>(params_.blanking_16b_ire)
    );
    
    // Calculate start position for biphase signal
    // According to spec: T = 0.188 H ± 0.003 H
    // H (line period) can be calculated from sample_rate and field_width
    double line_period_h = static_cast<double>(params_.field_width) / params_.sample_rate;
    int32_t start_pos = BiphaseEncoder::get_signal_start_position(
        params_.sample_rate, line_period_h);
    
    // Copy biphase signal into line buffer
    int32_t samples_to_copy = std::min(
        static_cast<int32_t>(biphase_samples.size()),
        params_.field_width - start_pos
    );
    
    if (samples_to_copy > 0) {
        std::copy(biphase_samples.begin(),
                 biphase_samples.begin() + samples_to_copy,
                 line_buffer + start_pos);
    }
}

} // namespace encode_orc
