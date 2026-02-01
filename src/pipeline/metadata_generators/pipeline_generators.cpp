/*
 * File:        pipeline_generators.cpp
 * Module:      encode-orc
 * Purpose:     Pipeline-compatible metadata generators
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: 2026 Simon Inns
 */

#include "pipeline_generators.h"
#include "vits_pipeline_generator.h"
#include "logging.h"
#include "field.h"
#include <algorithm>

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
    
    // For Y/C output on C field, use 32768 as center level (mid-range chroma)
    // For composite, use blanking level
    int32_t center_level = context.is_c_field_for_yc ? 32768 : params_.blanking_16b_ire;
    
    for (int32_t line = first_burst_line; line <= last_line; line++) {
        uint16_t* line_buffer = field.line_data(line);
        
        if (system_ == VideoSystem::PAL) {
            generator_->generate_pal_burst(line_buffer, line, context.field_number,
                                          center_level, burst_amplitude);
        } else {
            generator_->generate_ntsc_burst(line_buffer, line, context.field_number,
                                           center_level, burst_amplitude);
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
// PALVITSMetadataGenerator
// ============================================================================

PALVITSMetadataGenerator::PALVITSMetadataGenerator(const VideoParameters& params,
                                                   const std::vector<VITSSignalConfig>& signals)
    : params_(params), generator_(std::make_unique<PALVITSGenerator>(params)), signals_(signals) {
}

void PALVITSMetadataGenerator::apply(Field& field, const MetadataContext& context) {
    // Determine which field we're in (1 or 2)
    int32_t field_in_frame = context.is_first_field ? 1 : 2;
    
    // Generate each configured signal
    for (const auto& signal_config : signals_) {
        // Check if this signal applies to the current field
        if (signal_config.field != field_in_frame) {
            continue;  // Skip signals for other field
        }
        
        // Validate line number
        if (signal_config.line < 0 || signal_config.line >= field.height()) {
            ENCODE_ORC_LOG_WARN("PAL VITS signal configured for invalid line: {}", signal_config.line);
            continue;
        }
        
        // Generate the appropriate signal
        uint16_t* line_buffer = field.line_data(signal_config.line);
        
        switch (signal_config.signal) {
            case VITSSignalType::ITU_COMPOSITE:
                generator_->generate_itu_composite(line_buffer, context.field_number);
                break;
            case VITSSignalType::UK_NATIONAL:
                generator_->generate_uk_national(line_buffer, context.field_number);
                break;
            case VITSSignalType::ITU_ITS:
                generator_->generate_itu_its(line_buffer, context.field_number);
                break;
            case VITSSignalType::MULTIBURST:
                generator_->generate_multiburst(line_buffer, context.field_number);
                break;
            case VITSSignalType::VIR:
            case VITSSignalType::NTC7_COMPOSITE:
            case VITSSignalType::NTC7_COMBINATION:
                ENCODE_ORC_LOG_WARN("NTSC-specific VITS signal type used in PAL project - this should have been caught during validation");
                break;
        }
    }
}

std::vector<int32_t> PALVITSMetadataGenerator::affected_lines() const {
    std::vector<int32_t> lines;
    for (const auto& signal : signals_) {
        lines.push_back(signal.line);
    }
    return lines;
}

// ============================================================================
// NTSCVITSMetadataGenerator
// ============================================================================

NTSCVITSMetadataGenerator::NTSCVITSMetadataGenerator(const VideoParameters& params,
                                                     const std::vector<VITSSignalConfig>& signals)
    : params_(params), generator_(std::make_unique<NTSCVITSGenerator>(params)), signals_(signals) {
}

void NTSCVITSMetadataGenerator::apply(Field& field, const MetadataContext& context) {
    // Determine which field we're in (1 or 2)
    int32_t field_in_frame = context.is_first_field ? 1 : 2;
    
    // Generate each configured signal
    for (const auto& signal_config : signals_) {
        // Check if this signal applies to the current field
        if (signal_config.field != field_in_frame) {
            continue;  // Skip signals for other field
        }
        
        // Validate line number
        if (signal_config.line < 0 || signal_config.line >= field.height()) {
            ENCODE_ORC_LOG_WARN("NTSC VITS signal configured for invalid line: {}", signal_config.line);
            continue;
        }
        
        // Generate the appropriate signal
        uint16_t* line_buffer = field.line_data(signal_config.line);
        
        switch (signal_config.signal) {
            case VITSSignalType::VIR:
                generator_->generate_vir(line_buffer, context.field_number);
                break;
            case VITSSignalType::NTC7_COMPOSITE:
                generator_->generate_ntc7_composite(line_buffer, context.field_number);
                break;
            case VITSSignalType::NTC7_COMBINATION:
                generator_->generate_ntc7_combination(line_buffer, context.field_number);
                break;
            case VITSSignalType::ITU_COMPOSITE:
            case VITSSignalType::UK_NATIONAL:
            case VITSSignalType::ITU_ITS:
            case VITSSignalType::MULTIBURST:
                ENCODE_ORC_LOG_WARN("PAL-specific VITS signal type used in NTSC project - this should have been caught during validation");
                break;
        }
    }
}

std::vector<int32_t> NTSCVITSMetadataGenerator::affected_lines() const {
    std::vector<int32_t> lines;
    for (const auto& signal : signals_) {
        lines.push_back(signal.line);
    }
    return lines;
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
    
    // Get field height for the current field being processed
    int32_t field_height = field.height();
    
    // Use actual field1 height from parameters (in absolute frame line numbering)
    int32_t field1_absolute_height = params_.field1_height;
    
    // Process configured lines and encode VBI data
    // We map lines based on their position within the current field
    
    for (size_t i = 0; i < lines_.size(); i++) {
        int32_t absolute_line = lines_[i];
        
        // Determine which field this line belongs to (using absolute line numbering)
        bool is_field1_line = (absolute_line < field1_absolute_height);
        
        // Skip if this line belongs to the other field
        if (is_field1_line != context.is_first_field) {
            continue;
        }
        
        // Convert absolute line to field-relative line
        int32_t field_relative_line = is_field1_line ? 
                                     absolute_line : 
                                     (absolute_line - field1_absolute_height);
        
        // Validate line is within current field
        if (field_relative_line < 0 || field_relative_line >= field_height) {
            continue;
        }
        
        // Determine which VBI byte this line maps to (vbi0, vbi1, vbi2)
        // We need to count how many lines in the current field we've already processed
        int32_t byte_index = 0;
        for (size_t j = 0; j < i; j++) {
            int32_t prev_absolute_line = lines_[j];
            bool prev_is_field1 = (prev_absolute_line < field1_absolute_height);
            if (prev_is_field1 == is_field1_line) {
                byte_index++;
            }
        }
        
        // Sanity check
        if (byte_index > 2) {
            ENCODE_ORC_LOG_WARN("Biphase VBI generator: more than 3 lines per field configured, skipping line {}", absolute_line);
            continue;
        }
        
        // Get the appropriate VBI byte (with explicit cast to avoid C4244 on MSVC)
        uint8_t vbi_byte = (byte_index == 0) ? static_cast<uint8_t>(context.vbi_data->vbi0) : 
                          (byte_index == 1) ? static_cast<uint8_t>(context.vbi_data->vbi1) : 
                                              static_cast<uint8_t>(context.vbi_data->vbi2);
        
        uint16_t* line_buffer = field.line_data(field_relative_line);
        
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
