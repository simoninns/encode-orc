/*
 * File:        vits_composer.h
 * Module:      encode-orc
 * Purpose:     VITS signal composition and field integration
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: 2026 Simon Inns
 */

#ifndef ENCODE_ORC_VITS_COMPOSER_H
#define ENCODE_ORC_VITS_COMPOSER_H

#include "vits_signal_generator.h"
#include "vits_line_allocator.h"
#include "field.h"
#include <memory>
#include <string>

namespace encode_orc {

/**
 * @brief VITS standard identifiers
 */
enum class VITSStandard {
    NONE,              ///< No VITS signals
    IEC60856_PAL,      ///< IEC 60856-1986 PAL LaserDisc
    IEC60856_NTSC,     ///< IEC 60856-1986 NTSC LaserDisc (future)
    ITU_J63_PAL        ///< ITU-T J.63 Generic PAL (future)
};

/**
 * @brief Convert standard enum to string
 */
inline std::string vits_standard_to_string(VITSStandard standard) {
    switch (standard) {
        case VITSStandard::NONE: return "none";
        case VITSStandard::IEC60856_PAL: return "iec60856-pal";
        case VITSStandard::IEC60856_NTSC: return "iec60856-ntsc";
        case VITSStandard::ITU_J63_PAL: return "itu-j63-pal";
        default: return "unknown";
    }
}

/**
 * @brief Parse string to VITS standard
 */
inline VITSStandard string_to_vits_standard(const std::string& str) {
    if (str == "none") return VITSStandard::NONE;
    if (str == "iec60856-pal") return VITSStandard::IEC60856_PAL;
    if (str == "iec60856-ntsc") return VITSStandard::IEC60856_NTSC;
    if (str == "itu-j63-pal") return VITSStandard::ITU_J63_PAL;
    return VITSStandard::NONE;
}

/**
 * @brief VITS composer - coordinates signal generation and line allocation
 * 
 * This class combines VITS signal generators and line allocators to
 * insert test signals into video fields. It is format-agnostic and works
 * with both PAL and NTSC (via polymorphism).
 */
class VITSComposer {
public:
    /**
     * @brief Construct VITS composer with specific generator and allocator
     * @param generator VITS signal generator (PAL or NTSC specific)
     * @param allocator Line allocator for specific standard
     * @param format Video format (PAL or NTSC)
     */
    VITSComposer(std::unique_ptr<VITSSignalGeneratorBase> generator,
                 std::unique_ptr<VITSLineAllocatorBase> allocator,
                 VideoSystem format);
    
    /**
     * @brief Compose VITS signals into a field
     * @param field Field to insert VITS into
     * @param fieldNumber Field number (for V-switch calculation)
     * 
     * This method inserts VITS signals according to the line allocator's
     * mapping. It replaces the content of VITS lines while preserving
     * sync and blanking structure.
     */
    void composeVITSField(Field& field, int32_t fieldNumber);
    
    /**
     * @brief Insert VITS signal on a specific line
     * @param field Field to modify
     * @param lineNumber Line number within field (0-indexed)
     * @param fieldNumber Field number (for signal generation)
     * @param signalType Type of VITS signal to insert
     */
    void insertVITSLine(Field& field, 
                       int32_t lineNumber,
                       int32_t fieldNumber,
                       VITSSignalType signalType);
    
    /**
     * @brief Convert PAL frame line number to field line position
     * @param frame_line Frame line number (1-indexed PAL numbering: 1-625)
     * @param field_number Field number (0 = field 1, 1 = field 2)
     * @return Field line position (0-indexed, 0-312), or -1 if line not in field
     * 
     * PAL interlaced video uses 625 lines numbered 1-625 with field interleaving:
     *   Odd lines (1, 3, 5, ..., 625): Field 2
     *   Even lines (2, 4, 6, ..., 624): Field 1
     * 
     * Each field contains 313 lines indexed 0-312.
     * 
     * IEC 60856-1986 VITS are mirrored in both frame halves:
     *   Lines 19, 20: first half VBI area
     *   Lines 332, 333: second half VBI area (313 + 19, 313 + 20)
     *   Use ORIGINAL frame_line parity for field assignment, but normalize for position:
     *   - Line 332 (even) uses normalized_line=19 for position, but goes to Field 1 (even lines)
     *   - Line 333 (odd) uses normalized_line=20 for position, but goes to Field 2 (odd lines)
     * 
     * Examples:
     *   Line 19 (odd) in Field 2 → position 9
     *   Line 20 (even) in Field 1 → position 9
     *   Line 332 (even) in Field 1 → position 9 (normalized from 19, same field as line 20)
     *   Line 333 (odd) in Field 2 → position 9 (normalized from 20, same field as line 19)
     */
    int32_t pal_frame_line_to_field_line(int32_t frame_line, int32_t field_number) const;
    
    /**
     * @brief Get VITS line range
     * @return Pair of (start_line, end_line)
     */
    std::pair<int32_t, int32_t> getVITSLineRange() const;
    
    /**
     * @brief Check if composer is enabled
     */
    bool isEnabled() const { return enabled_; }
    
    /**
     * @brief Enable/disable VITS composition
     */
    void setEnabled(bool enabled) { enabled_ = enabled; }

private:
    std::unique_ptr<VITSSignalGeneratorBase> generator_;
    std::unique_ptr<VITSLineAllocatorBase> allocator_;
    VideoSystem format_;
    bool enabled_;
    
    // Default frequencies for multiburst (MHz)
    std::vector<float> multiburst_frequencies_;
    
    /**
     * @brief Initialize default parameters
     */
    void initializeDefaults();
};

/**
 * @brief Factory function to create VITS composer for specific standard
 * @param params Video parameters
 * @param standard VITS standard to use
 * @return Unique pointer to VITSComposer, or nullptr if standard not supported
 */
std::unique_ptr<VITSComposer> createVITSComposer(const VideoParameters& params,
                                                  VITSStandard standard);

} // namespace encode_orc

#endif // ENCODE_ORC_VITS_COMPOSER_H
