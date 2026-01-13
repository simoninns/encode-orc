/*
 * File:        vits_line_allocator.h
 * Module:      encode-orc
 * Purpose:     VITS line allocation for different standards
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: 2026 Simon Inns
 */

#ifndef ENCODE_ORC_VITS_LINE_ALLOCATOR_H
#define ENCODE_ORC_VITS_LINE_ALLOCATOR_H

#include "video_parameters.h"
#include <cstdint>
#include <string>
#include <vector>

namespace encode_orc {

/**
 * @brief VITS signal types
 */
enum class VITSSignalType {
    NONE,                    ///< No VITS signal (blanking only)
    COLOR_BURST,             ///< Color burst reference
    WHITE_REFERENCE,         ///< White level reference (100 IRE)
    GRAY_75_REFERENCE,       ///< 75% gray reference
    GRAY_50_REFERENCE,       ///< 50% gray reference
    BLACK_REFERENCE,         ///< Black level reference (0 IRE)
    MULTIBURST,              ///< Multiburst frequency response test
    STAIRCASE,               ///< Staircase luminance test
    INSERTION_TEST_SIGNAL,   ///< LaserDisc insertion test signal (ITS)
    DIFFERENTIAL_GAIN_PHASE, ///< Differential gain and phase test
    CROSS_COLOR,             ///< Cross-color distortion test
    VSYNC,                   ///< Vertical sync (timing reference)
    RESERVED,                ///< Reserved for future use
    IEC60856_LINE19,         ///< IEC 60856 Line 19: Luminance tests (B2, B1, F, D1)
    IEC60856_LINE20,         ///< IEC 60856 Line 20: Frequency response (C1, C2, C3)
    IEC60856_LINE332,        ///< IEC 60856 Line 332: Differential gain/phase (B2, B1, D2)
    IEC60856_LINE333         ///< IEC 60856 Line 333: Chrominance (G1, E)
};

/**
 * @brief Convert signal type to string
 */
inline std::string vits_signal_type_to_string(VITSSignalType type) {
    switch (type) {
        case VITSSignalType::NONE: return "None";
        case VITSSignalType::COLOR_BURST: return "ColorBurst";
        case VITSSignalType::WHITE_REFERENCE: return "WhiteReference";
        case VITSSignalType::GRAY_75_REFERENCE: return "75% Gray";
        case VITSSignalType::GRAY_50_REFERENCE: return "50% Gray";
        case VITSSignalType::BLACK_REFERENCE: return "BlackReference";
        case VITSSignalType::MULTIBURST: return "Multiburst";
        case VITSSignalType::STAIRCASE: return "Staircase";
        case VITSSignalType::INSERTION_TEST_SIGNAL: return "ITS";
        case VITSSignalType::DIFFERENTIAL_GAIN_PHASE: return "DiffGainPhase";
        case VITSSignalType::CROSS_COLOR: return "CrossColor";
        case VITSSignalType::VSYNC: return "VSync";
        case VITSSignalType::RESERVED: return "Reserved";
        case VITSSignalType::IEC60856_LINE19: return "IEC60856-Line19";
        case VITSSignalType::IEC60856_LINE20: return "IEC60856-Line20";
        case VITSSignalType::IEC60856_LINE332: return "IEC60856-Line332";
        case VITSSignalType::IEC60856_LINE333: return "IEC60856-Line333";
        default: return "Unknown";
    }
}

/**
 * @brief Line allocation entry
 */
struct VITSLineAllocation {
    int32_t lineNumber;         ///< Frame line number (1-based, PAL 1-625)
    VITSSignalType signalType;  ///< Type of signal on this line
    bool includeInField1;       ///< Include in first field (frame lines 1-313)
    bool includeInField2;       ///< Include in second field (frame lines 314-625)
    std::string description;    ///< Human-readable description
};

/**
 * @brief Abstract base class for VITS line allocation
 */
class VITSLineAllocatorBase {
public:
    virtual ~VITSLineAllocatorBase() = default;
    
    /**
     * @brief Check if a line number contains VITS
     * @param lineNumber Line number (0-indexed within field)
     * @return true if line contains VITS signal
     */
    virtual bool isVITSLine(int32_t lineNumber) const = 0;
    
    /**
     * @brief Get signal type for a specific line
     * @param lineNumber Frame line number (1-indexed, PAL 1-625)
     * @param fieldNumber Field number (0-based; field 0 = first field)
     * @return VITS signal type for this line
     */
    virtual VITSSignalType getSignalForLine(int32_t lineNumber, uint8_t fieldNumber) const = 0;
    
    /**
     * @brief Get first VITS line number
     * @return Starting line number for VITS region
     */
    virtual int32_t getVITSStartLine() const = 0;
    
    /**
     * @brief Get last VITS line number (inclusive)
     * @return Ending line number for VITS region
     */
    virtual int32_t getVITSEndLine() const = 0;
    
    /**
     * @brief Get complete line allocation
     * @return Vector of all VITS line allocations
     */
    virtual std::vector<VITSLineAllocation> getAllocations() const = 0;
};

/**
 * @brief PAL LaserDisc line allocator (IEC 60856-1986)
 * 
 * Implements VITS line allocation for PAL LaserDisc according to
 * IEC 60856-1986 specification (pages 20-21).
 * 
 * PAL VITS lines: 6-22 (within VBI)
 */
class PALLaserDiscLineAllocator : public VITSLineAllocatorBase {
public:
    PALLaserDiscLineAllocator();
    
    bool isVITSLine(int32_t lineNumber) const override;
    VITSSignalType getSignalForLine(int32_t lineNumber, uint8_t fieldNumber) const override;
    
    int32_t getVITSStartLine() const override { return 19; }   // IEC 60856 VITS on frame lines 19-20
    int32_t getVITSEndLine() const override { return 20; }     // IEC 60856 VITS on frame lines 19-20
    
    std::vector<VITSLineAllocation> getAllocations() const override;

private:
    std::vector<VITSLineAllocation> allocations_;
    
    /**
     * @brief Initialize PAL LaserDisc VITS allocation
     */
    void initializeAllocations();
};

} // namespace encode_orc

#endif // ENCODE_ORC_VITS_LINE_ALLOCATOR_H
