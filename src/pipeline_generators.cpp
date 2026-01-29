/*
 * File:        pipeline_generators.cpp
 * Module:      encode-orc
 * Purpose:     Pipeline-compatible metadata generators
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: 2026 Simon Inns
 */

#include "pipeline_generators.h"
#include "logging.h"
#include "field.h"

namespace encode_orc {

// ============================================================================
// ColorBurstMetadataGenerator
// ============================================================================

ColorBurstMetadataGenerator::ColorBurstMetadataGenerator(const VideoParameters& params)
    : params_(params), system_(params.system) {
    generator_ = std::make_unique<ColorBurstGenerator>(params);
}

void ColorBurstMetadataGenerator::apply(Field& field, const MetadataContext& context) {
    // Add color burst to all lines except pure sync lines
    // For PAL: lines 1-22 get burst (VBI region), lines 23+ get burst (active video)
    // For NTSC: similar pattern
    
    int32_t first_burst_line = (system_ == VideoSystem::PAL) ? 6 : 10;
    int32_t last_line = field.height() - 1;
    
    // Calculate burst amplitude based on video system and luma range
    // PAL: 300mV peak-to-peak = 150mV amplitude = 3/14 of luma range
    // NTSC: 20% of luma range per standard
    int32_t luma_range = params_.white_16b_ire - params_.black_16b_ire;
    int32_t burst_amplitude = (system_ == VideoSystem::PAL) ? 
        static_cast<int32_t>((3.0 / 14.0) * luma_range) :
        static_cast<int32_t>((20.0 / 100.0) * luma_range);
    
    for (int32_t line = first_burst_line; line <= last_line; line++) {
        uint16_t* line_buffer = field.line_data(line);
        
        if (system_ == VideoSystem::PAL) {
            generator_->generate_pal_burst(line_buffer, line, context.field_number,
                                          params_.blanking_16b_ire, burst_amplitude);
        } else {
            generator_->generate_ntsc_burst(line_buffer, line, context.field_number,
                                           params_.blanking_16b_ire, burst_amplitude);
        }
    }
}

std::vector<int32_t> ColorBurstMetadataGenerator::affected_lines() const {
    // Returns all lines that get color burst
    std::vector<int32_t> lines;
    int32_t first_burst_line = (system_ == VideoSystem::PAL) ? 6 : 10;
    int32_t height = (system_ == VideoSystem::PAL) ? 313 : 263;
    
    for (int32_t line = first_burst_line; line < height; line++) {
        lines.push_back(line);
    }
    return lines;
}

// ============================================================================
// VITCMetadataGenerator
// ============================================================================

VITCMetadataGenerator::VITCMetadataGenerator(const VideoParameters& params, 
                                             const std::vector<int32_t>& lines)
    : params_(params), system_(params.system) {
    generator_ = std::make_unique<VITCGenerator>(params);
    
    // Use provided lines or default based on system
    if (!lines.empty()) {
        lines_ = lines;
    } else {
        if (system_ == VideoSystem::PAL) {
            // PAL: lines 19 and 21 (0-indexed: 18, 20)
            lines_ = {18, 20};
        } else {
            // NTSC: lines 14 and 16 (0-indexed: 13, 15)
            lines_ = {13, 15};
        }
    }
}

void VITCMetadataGenerator::apply(Field& field, const MetadataContext& context) {
    // VITC doesn't use VBI data - it generates timecode from frame number
    // No check needed
    
    // VITC uses frame number, not field number
    int32_t frame_number = context.total_frame;
    bool is_second_field = !context.is_first_field;
    
    for (int32_t line : lines_) {
        if (line >= 0 && line < field.height()) {
            uint16_t* line_buffer = field.line_data(line);
            generator_->generate_line(system_, frame_number, line_buffer, line, is_second_field);
        }
    }
}

std::vector<int32_t> VITCMetadataGenerator::affected_lines() const {
    return lines_;
}

// ============================================================================
// VITSMetadataGenerator
// ============================================================================

VITSMetadataGenerator::VITSMetadataGenerator(const VideoParameters& params)
    : params_(params), system_(params.system) {
    
    if (system_ == VideoSystem::PAL) {
        pal_generator_ = std::make_unique<PALVITSGenerator>(params);
    } else {
        ntsc_generator_ = std::make_unique<NTSCVITSGenerator>(params);
    }
}

void VITSMetadataGenerator::apply(Field& field, const MetadataContext& context) {
    bool is_first_field = context.is_first_field;
    int32_t field_number = context.field_number;
    
    if (system_ == VideoSystem::PAL) {
        // PAL VITS on lines 19, 20 for field 1; lines 332, 333 for field 2
        if (is_first_field) {
            // Field 1: lines 19 and 20 (0-indexed: 18, 19)
            uint16_t* line19 = field.line_data(18);
            uint16_t* line20 = field.line_data(19);
            // Generate standard IEC 60857 VITS: multiburst on line 19, UK national on line 20
            pal_generator_->generate_multiburst(line19, field_number);
            pal_generator_->generate_uk_national(line20, field_number);
        } else {
            // Field 2: lines 332 and 333 (field line 0-indexed: 18, 19)
            uint16_t* line332 = field.line_data(18);
            uint16_t* line333 = field.line_data(19);
            // Generate ITU composite on line 332, ITU ITS on line 333
            pal_generator_->generate_itu_composite(line332, field_number);
            pal_generator_->generate_itu_its(line333, field_number);
        }
    } else {
        // NTSC VITS on lines 19, 20 for field 1; lines 282, 283 for field 2
        if (is_first_field) {
            // Field 1: lines 19 and 20 (0-indexed: 18, 19)
            uint16_t* line19 = field.line_data(18);
            uint16_t* line20 = field.line_data(19);
            // Generate standard IEC 60856 VITS - Use composite and combination signals
            ntsc_generator_->generate_ntc7_composite(line19, field_number);
            ntsc_generator_->generate_ntc7_combination(line20, field_number);
        } else {
            // Field 2: lines 282 and 283 (field line 0-indexed: 18, 19)
            uint16_t* line282 = field.line_data(18);
            uint16_t* line283 = field.line_data(19);
            ntsc_generator_->generate_ntc7_combination(line282, field_number);
            ntsc_generator_->generate_ntc7_composite(line283, field_number);
        }
    }
}

std::vector<int32_t> VITSMetadataGenerator::affected_lines() const {
    // Both PAL and NTSC use field-relative lines 18 and 19
    return {18, 19};
}

// ============================================================================
// BiphaseVBIMetadataGenerator
// ============================================================================

BiphaseVBIMetadataGenerator::BiphaseVBIMetadataGenerator(const VideoParameters& params,
                                                         const std::vector<int32_t>& lines)
    : params_(params), lines_(lines) {
    // BiphaseEncoder is all static methods, no need to instantiate
}

void BiphaseVBIMetadataGenerator::apply(Field& field, const MetadataContext& context) {
    if (!context.vbi_data) {
        ENCODE_ORC_LOG_WARN("Biphase VBI generator called but no VBI data provided");
        return;
    }
    
    // Encode each VBI byte onto its line using BiphaseEncoder static methods
    for (size_t i = 0; i < lines_.size() && i < 3; i++) {
        int32_t line_idx = lines_[i];
        if (line_idx < 0 || line_idx >= field.height()) continue;
        
        uint16_t* line_buffer = field.line_data(line_idx);
        uint8_t vbi_byte = (i == 0) ? context.vbi_data->vbi0 : 
                          (i == 1) ? context.vbi_data->vbi1 : 
                                     context.vbi_data->vbi2;
        
        // Encode single byte as 3-byte biphase (byte, 0xFF, 0xFF for single-byte mode)
        auto samples = BiphaseEncoder::encode(vbi_byte, 0xFF, 0xFF,
                                             params_.sample_rate,
                                             params_.white_16b_ire,
                                             params_.blanking_16b_ire);
        
        // Get start position for biphase signal
        double line_period_h = 1.0 / (params_.system == VideoSystem::PAL ? 15625.0 : 15734.0);
        int32_t start_pos = BiphaseEncoder::get_signal_start_position(params_.sample_rate, line_period_h);
        
        // Copy biphase samples into line buffer
        for (size_t j = 0; j < samples.size() && (start_pos + j) < static_cast<size_t>(field.width()); j++) {
            line_buffer[start_pos + j] = samples[j];
        }
    }
}

std::vector<int32_t> BiphaseVBIMetadataGenerator::affected_lines() const {
    return lines_;
}

} // namespace encode_orc
