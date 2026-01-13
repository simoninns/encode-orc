/*
 * File:        vits_composer.cpp
 * Module:      encode-orc
 * Purpose:     VITS signal composition implementation
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: 2026 Simon Inns
 */

#include "vits_composer.h"
#include <cstring>

namespace encode_orc {

VITSComposer::VITSComposer(std::unique_ptr<VITSSignalGeneratorBase> generator,
                           std::unique_ptr<VITSLineAllocatorBase> allocator,
                           VideoSystem format)
    : generator_(std::move(generator))
    , allocator_(std::move(allocator))
    , format_(format)
    , enabled_(true)
{
    initializeDefaults();
}

void VITSComposer::initializeDefaults() {
    // Default multiburst frequencies for PAL (in MHz)
    // These are standard test frequencies for video frequency response
    multiburst_frequencies_ = {0.5f, 1.0f, 2.0f, 3.0f, 4.2f};
}

std::pair<int32_t, int32_t> VITSComposer::getVITSLineRange() const {
    return {allocator_->getVITSStartLine(), allocator_->getVITSEndLine()};
}

void VITSComposer::composeVITSField(Field& field, int32_t fieldNumber) {
    if (!enabled_ || !generator_ || !allocator_) {
        return;
    }
    
    // IEC 60856-1986 specifies only 4 VITS lines: 19, 20, 332, 333 (frame line numbers, 1-indexed)
    // Insert VITS signals according to allocation
    std::vector<VITSLineAllocation> allocations = allocator_->getAllocations();
    for (const auto& alloc : allocations) {
        int32_t frame_line = alloc.lineNumber;  // 1-indexed from IEC spec
        
        // Check if this line should be included in this field
        // PAL field order: ODD field first (frame lines 1-313), EVEN field second (frame lines 314-625)
        // Field numbering: odd field numbers (1,3,5...) = first field, even (0,2,4...) = second field
        bool is_odd_field = (fieldNumber % 2) != 0;
        bool include = is_odd_field ? alloc.includeInField1 : alloc.includeInField2;
        if (!include) {
            continue;
        }
        
        // Convert frame line number to field line using helper function
        int32_t field_line = pal_frame_line_to_field_line(frame_line, fieldNumber);
        if (field_line < 0 || field_line >= field.height()) {
            continue;
        }
        
        // Get signal type for this line
        VITSSignalType signal_type = allocator_->getSignalForLine(frame_line, static_cast<uint8_t>(fieldNumber));
        
        if (signal_type != VITSSignalType::NONE) {
            insertVITSLine(field, field_line, fieldNumber, signal_type);
        }
    }
}

int32_t VITSComposer::pal_frame_line_to_field_line(int32_t frame_line, int32_t field_number) const {
    // PAL uses 625 frame lines (1-indexed). 
    // PAL field order: ODD field is first (frame lines 1-313), EVEN field is second (frame lines 314-625)
    // Field numbering: odd field numbers (1,3,5...) = first/odd field, even (0,2,4...) = second/even field
    //
    // First/odd field covers frame lines 1-313, second/even field covers 314-625. We
    // convert the 1-based frame line to a 0-based field-line index so it can
    // be used directly with Field::line_data().
    //
    // Examples (frame → field index):
    //   frame 19 (first/odd field)   → 18
    //   frame 20 (first/odd field)   → 19
    //   frame 332 (second/even field) → 18
    //   frame 333 (second/even field) → 19

    if (frame_line < 1 || frame_line > 625) {
        return -1;
    }

    const bool is_odd_field = (field_number % 2) != 0;

    if (frame_line <= 313) {
        // Frame lines 1-313 belong to the first/odd field only
        if (!is_odd_field) {
            return -1;
        }
        return frame_line - 1;  // convert 1-based to 0-based
    }

    // Frame lines 314-625 belong to the second/even field only
    if (is_odd_field) {
        return -1;
    }
    return frame_line - 314;  // 314 → index 0, 625 → index 311
}

void VITSComposer::insertVITSLine(Field& field,
                                 int32_t lineNumber,
                                 int32_t fieldNumber,
                                 VITSSignalType signalType) {
    if (!generator_) {
        return;
    }
    
    // Validate line number
    if (lineNumber >= field.height()) {
        return;
    }
    
    // Get pointer to line buffer in field
    uint16_t* line_buffer = field.line_data(lineNumber);
    
    // Generate appropriate VITS signal based on type
    switch (signalType) {
        case VITSSignalType::NONE:
            // No VITS - keep existing content (blanking)
            break;
            
        case VITSSignalType::COLOR_BURST:
            generator_->generateColorBurst(line_buffer, lineNumber, fieldNumber);
            break;
            
        case VITSSignalType::WHITE_REFERENCE:
            generator_->generateWhiteReference(line_buffer, lineNumber, fieldNumber);
            break;
            
        case VITSSignalType::GRAY_75_REFERENCE:
            generator_->generate75GrayReference(line_buffer, lineNumber, fieldNumber);
            break;
            
        case VITSSignalType::GRAY_50_REFERENCE:
            generator_->generate50GrayReference(line_buffer, lineNumber, fieldNumber);
            break;
            
        case VITSSignalType::BLACK_REFERENCE:
            generator_->generateBlackReference(line_buffer, lineNumber, fieldNumber);
            break;
            
        case VITSSignalType::MULTIBURST:
            generator_->generateMultiburst(line_buffer, multiburst_frequencies_, 
                                          lineNumber, fieldNumber);
            break;
            
        case VITSSignalType::STAIRCASE:
            generator_->generateStaircase(line_buffer, 8, lineNumber, fieldNumber);  // 8 steps
            break;
            
        case VITSSignalType::INSERTION_TEST_SIGNAL:
            // PAL-specific - cast to PAL generator
            if (auto* pal_gen = dynamic_cast<PALVITSSignalGenerator*>(generator_.get())) {
                pal_gen->generateInsertionTestSignal(line_buffer, lineNumber, fieldNumber);
            }
            break;
            
        case VITSSignalType::DIFFERENTIAL_GAIN_PHASE:
            // PAL-specific - cast to PAL generator
            if (auto* pal_gen = dynamic_cast<PALVITSSignalGenerator*>(generator_.get())) {
                // Test at 50% luminance with 30% chroma (300mV standard)
                pal_gen->generateDifferentialGainPhase(line_buffer, 0.3f, 0.5f,
                                                      lineNumber, fieldNumber);
            }
            break;
            
        case VITSSignalType::CROSS_COLOR:
            // PAL-specific - cast to PAL generator
            if (auto* pal_gen = dynamic_cast<PALVITSSignalGenerator*>(generator_.get())) {
                pal_gen->generateCrossColorReference(line_buffer, lineNumber, fieldNumber);
            }
            break;
            
        case VITSSignalType::VSYNC:
            // VSYNC lines typically handled by encoder, but we can ensure blanking
            generator_->generateBlackReference(line_buffer, lineNumber, fieldNumber);
            break;
            
        case VITSSignalType::RESERVED:
            // Reserved lines - use blanking
            generator_->generateBlackReference(line_buffer, lineNumber, fieldNumber);
            break;
            
        case VITSSignalType::IEC60856_LINE19:
            // IEC 60856 Line 19: Luminance tests (B2, B1, F, D1)
            if (auto* pal_gen = dynamic_cast<PALVITSSignalGenerator*>(generator_.get())) {
                pal_gen->generateIEC60856Line19(line_buffer, lineNumber, fieldNumber);
            }
            break;
            
        case VITSSignalType::IEC60856_LINE20:
            // IEC 60856 Line 20: Frequency response (C1, C2, C3)
            if (auto* pal_gen = dynamic_cast<PALVITSSignalGenerator*>(generator_.get())) {
                pal_gen->generateIEC60856Line20(line_buffer, lineNumber, fieldNumber);
            } else {
                // Fallback: just generate blanking if dynamic_cast fails
                generator_->generateBlackReference(line_buffer, lineNumber, fieldNumber);
            }
            break;
            
        case VITSSignalType::IEC60856_LINE332:
            // IEC 60856 Line 332: Differential gain/phase (B2, B1, D2)
            if (auto* pal_gen = dynamic_cast<PALVITSSignalGenerator*>(generator_.get())) {
                pal_gen->generateIEC60856Line332(line_buffer, lineNumber, fieldNumber);
            }
            break;
            
        case VITSSignalType::IEC60856_LINE333:
            // IEC 60856 Line 333: Chrominance (G1, E)
            if (auto* pal_gen = dynamic_cast<PALVITSSignalGenerator*>(generator_.get())) {
                pal_gen->generateIEC60856Line333(line_buffer, lineNumber, fieldNumber);
            }
            break;
    }
}

// Factory function to create VITS composer
std::unique_ptr<VITSComposer> createVITSComposer(const VideoParameters& params,
                                                  VITSStandard standard) {
    switch (standard) {
        case VITSStandard::NONE:
            // Return nullptr for no VITS
            return nullptr;
            
        case VITSStandard::IEC60856_PAL:
            // Create PAL LaserDisc VITS composer
            if (params.system == VideoSystem::PAL) {
                auto generator = std::make_unique<PALVITSSignalGenerator>(params);
                auto allocator = std::make_unique<PALLaserDiscLineAllocator>();
                return std::make_unique<VITSComposer>(std::move(generator),
                                                      std::move(allocator),
                                                      VideoSystem::PAL);
            }
            break;
            
        case VITSStandard::IEC60856_NTSC:
            // Future: NTSC LaserDisc support
            // TODO: Implement when NTSC VITS generator is available
            break;
            
        case VITSStandard::ITU_J63_PAL:
            // Future: ITU-T J.63 generic PAL support
            // TODO: Implement when J.63 allocator is available
            break;
    }
    
    return nullptr;
}

} // namespace encode_orc
