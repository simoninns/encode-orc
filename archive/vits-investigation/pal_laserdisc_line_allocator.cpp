/*
 * File:        pal_laserdisc_line_allocator.cpp
 * Module:      encode-orc
 * Purpose:     PAL LaserDisc VITS line allocation (IEC 60856-1986)
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: 2026 Simon Inns
 */

#include "vits_line_allocator.h"

namespace encode_orc {

PALLaserDiscLineAllocator::PALLaserDiscLineAllocator() {
    initializeAllocations();
}

void PALLaserDiscLineAllocator::initializeAllocations() {
    // PAL LaserDisc VITS allocation based on IEC 60856-1986 (pages 20-21)
    // PAL uses 625-line interlaced format:
    //   First field: frame lines 1-313
    //   Second field: frame lines 314-625
    //
    // IEC 60856 specifies VITS on frame lines:
    //   19  → first field
    //   20  → first field
    //   332 → second field (332 - 313 = field line 19)
    //   333 → second field (333 - 313 = field line 20)
    
    allocations_ = {
        // Line 19 - First field (frame lines 1-313)
        // Luminance amplitude, transient response, K-factor
        // Contains: White reference (B2), 2T pulse (B1), 20T pulse (F), Staircase (D1)
        {19, VITSSignalType::IEC60856_LINE19, true, false,
         "Luminance transient & amplitude (B2, B1, F, D1)"},
        
        // Line 20 - First field (frame lines 1-313)
        // Frequency response (multiburst)
        // Contains: White ref (C1), Black ref (C2), Multiburst (0.5, 1.3, 2.3, 4.2, 4.8, 5.8 MHz)
        {20, VITSSignalType::IEC60856_LINE20, true, false,
         "Frequency response multiburst (C1, C2, C3)"},
        
        // Line 332 - Second field (frame lines 314-625; field line 19 = 332-313)
        // Differential gain and differential phase
        // Contains: White (B2), 2T pulse (B1), Composite staircase with chroma (D2)
        {332, VITSSignalType::IEC60856_LINE332, false, true,
         "Differential gain & phase (B2, B1, D2)"},
        
        // Line 333 - Second field (frame lines 314-625; field line 20 = 333-313)
        // Chrominance amplitude and linearity
        // Contains: 3-level chroma bars (G1), Chroma reference (E)
        {333, VITSSignalType::IEC60856_LINE333, false, true,
         "Chrominance amplitude & linearity (G1, E)"}
    };
}

bool PALLaserDiscLineAllocator::isVITSLine(int32_t lineNumber) const {
    // VITS on lines 19, 20, 332, 333
    return (lineNumber == 19 || lineNumber == 20 || lineNumber == 332 || lineNumber == 333);
}

VITSSignalType PALLaserDiscLineAllocator::getSignalForLine(int32_t lineNumber, 
                                                           uint8_t fieldNumber) const {
    // Find allocation for this line
    for (const auto& alloc : allocations_) {
        if (alloc.lineNumber == lineNumber) {
            // Check if signal is included in this field
            // PAL field order: ODD field first (1,3,5...), EVEN field second (0,2,4...)
            bool is_odd_field = (fieldNumber % 2) != 0;
            bool include = is_odd_field ? alloc.includeInField1 : alloc.includeInField2;
            if (include) {
                return alloc.signalType;
            }
        }
    }
    
    return VITSSignalType::NONE;
}

std::vector<VITSLineAllocation> PALLaserDiscLineAllocator::getAllocations() const {
    return allocations_;
}

} // namespace encode_orc
