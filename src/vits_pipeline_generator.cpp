/*
 * File:        vits_pipeline_generator.cpp
 * Module:      encode-orc
 * Purpose:     Pipeline generators for PAL and NTSC VITS test signals
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: 2026 Simon Inns
 */

#include "vits_pipeline_generator.h"
#include "logging.h"
#include <algorithm>

namespace encode_orc {

// =============================================================================
// Helper Functions
// =============================================================================

bool parse_vits_signal_type(const std::string& str, VITSSignalType& type) {
    if (str == "itu-composite") {
        type = VITSSignalType::ITU_COMPOSITE;
        return true;
    } else if (str == "uk-national") {
        type = VITSSignalType::UK_NATIONAL;
        return true;
    } else if (str == "itu-its") {
        type = VITSSignalType::ITU_ITS;
        return true;
    } else if (str == "multiburst") {
        type = VITSSignalType::MULTIBURST;
        return true;
    }
    return false;
}

std::string vits_signal_type_to_string(VITSSignalType type) {
    switch (type) {
        case VITSSignalType::ITU_COMPOSITE: return "itu-composite";
        case VITSSignalType::UK_NATIONAL: return "uk-national";
        case VITSSignalType::ITU_ITS: return "itu-its";
        case VITSSignalType::MULTIBURST: return "multiburst";
    }
    return "unknown";
}

// =============================================================================
// PALVITSPipelineGenerator
// =============================================================================

PALVITSPipelineGenerator::PALVITSPipelineGenerator(const VideoParameters& params, const Config& config)
    : vits_gen_(std::make_unique<PALVITSGenerator>(params)), config_(config) {
}

void PALVITSPipelineGenerator::apply(StructuredField& field, const MetadataContext& context) {
    // Determine which field we're in (1 or 2)
    int32_t field_in_frame = context.is_first_field ? 1 : 2;
    
    // Generate each configured signal
    for (const auto& signal_config : config_.signals) {
        // Check if this signal applies to the current field
        if (signal_config.field != field_in_frame) {
            continue;  // Skip signals for other field
        }
        
        // Validate line number
        if (signal_config.line < 0 || signal_config.line >= field.field_data.height()) {
            ENCODE_ORC_LOG_WARN("VITS signal configured for invalid line: {}", signal_config.line);
            continue;
        }
        
        // Generate the appropriate signal
        uint16_t* line_buffer = field.field_data.line_data(signal_config.line);
        
        switch (signal_config.signal) {
            case VITSSignalType::ITU_COMPOSITE:
                vits_gen_->generate_itu_composite(line_buffer, context.field_number);
                break;
            case VITSSignalType::UK_NATIONAL:
                vits_gen_->generate_uk_national(line_buffer, context.field_number);
                break;
            case VITSSignalType::ITU_ITS:
                vits_gen_->generate_itu_its(line_buffer, context.field_number);
                break;
            case VITSSignalType::MULTIBURST:
                vits_gen_->generate_multiburst(line_buffer, context.field_number);
                break;
        }
    }
}

std::vector<int32_t> PALVITSPipelineGenerator::affected_lines() const {
    std::vector<int32_t> lines;
    for (const auto& signal : config_.signals) {
        lines.push_back(signal.line);
    }
    // Sort and remove duplicates
    std::sort(lines.begin(), lines.end());
    lines.erase(std::unique(lines.begin(), lines.end()), lines.end());
    return lines;
}

std::string PALVITSPipelineGenerator::name() const {
    return "PALVITSPipelineGenerator";
}

bool PALVITSPipelineGenerator::is_applicable(const MetadataContext& context) const {
    // Only applicable for PAL systems
    return context.system == VideoSystem::PAL;
}

// =============================================================================
// NTSCVITSPipelineGenerator
// =============================================================================

NTSCVITSPipelineGenerator::NTSCVITSPipelineGenerator(const VideoParameters& params, const Config& config)
    : vits_gen_(std::make_unique<NTSCVITSGenerator>(params)), config_(config) {
}

void NTSCVITSPipelineGenerator::apply(StructuredField& field, const MetadataContext& context) {
    // Determine which field we're in (1 or 2)
    int32_t field_in_frame = context.is_first_field ? 1 : 2;
    
    // Generate each configured signal
    for (const auto& signal_config : config_.signals) {
        // Check if this signal applies to the current field
        if (signal_config.field != field_in_frame) {
            continue;  // Skip signals for other field
        }
        
        // Validate line number
        if (signal_config.line < 0 || signal_config.line >= field.field_data.height()) {
            ENCODE_ORC_LOG_WARN("VITS signal configured for invalid line: {}", signal_config.line);
            continue;
        }
        
        // Generate the appropriate signal
        uint16_t* line_buffer = field.field_data.line_data(signal_config.line);
        
        switch (signal_config.signal) {
            case VITSSignalType::ITU_COMPOSITE:
                vits_gen_->generate_ntc7_composite(line_buffer, context.field_number);
                break;
            case VITSSignalType::UK_NATIONAL:
                // UK National is PAL-specific, not available for NTSC
                ENCODE_ORC_LOG_WARN("UK National VITS signal is PAL-specific, skipping for NTSC");
                break;
            case VITSSignalType::ITU_ITS:
                vits_gen_->generate_ntc7_combination(line_buffer, context.field_number);
                break;
            case VITSSignalType::MULTIBURST:
                // NTSC doesn't have a standalone multiburst, use NTC7 composite which includes multiburst
                vits_gen_->generate_ntc7_composite(line_buffer, context.field_number);
                break;
        }
    }
}

std::vector<int32_t> NTSCVITSPipelineGenerator::affected_lines() const {
    std::vector<int32_t> lines;
    for (const auto& signal : config_.signals) {
        lines.push_back(signal.line);
    }
    // Sort and remove duplicates
    std::sort(lines.begin(), lines.end());
    lines.erase(std::unique(lines.begin(), lines.end()), lines.end());
    return lines;
}

std::string NTSCVITSPipelineGenerator::name() const {
    return "NTSCVITSPipelineGenerator";
}

bool NTSCVITSPipelineGenerator::is_applicable(const MetadataContext& context) const {
    // Only applicable for NTSC systems
    return context.system == VideoSystem::NTSC;
}

} // namespace encode_orc
