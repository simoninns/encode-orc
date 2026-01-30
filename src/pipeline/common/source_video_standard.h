/*
 * File:        source_video_standard.h
 * Module:      encode-orc
 * Purpose:     Source video standard helpers (VBI/VITS/VITC capabilities)
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: 2026 Simon Inns
 */

#ifndef ENCODE_ORC_SOURCE_VIDEO_STANDARD_H
#define ENCODE_ORC_SOURCE_VIDEO_STANDARD_H

#include "video_parameters.h"
#include <algorithm>
#include <string>

namespace encode_orc {

/**
 * @brief Source video standards (LaserDisc IEC standards, consumer tape, or none)
 * 
 * This enum represents the source video format standard, which determines what
 * vertical interval signals are present:
 * - LaserDisc formats (IEC60856-1986 for NTSC, IEC60857-1986 for PAL) include
 *   LaserDisc VBI data and VITS test signals
 * - Consumer tape formats include VITC timecode but not LaserDisc VBI/VITS
 * - None disables all vertical interval data
 */
enum class SourceVideoStandard {
    None,
    IEC60856_1986,  // NTSC LaserDisc
    IEC60857_1986,  // PAL LaserDisc
    ConsumerTape    // Consumer tape (VHS, SVHS, Betamax, etc.)
};

/**
 * @brief Convert SourceVideoStandard to string (lowercase)
 */
inline std::string source_video_standard_to_string(SourceVideoStandard standard) {
    switch (standard) {
        case SourceVideoStandard::IEC60856_1986: return "iec60856-1986";
        case SourceVideoStandard::IEC60857_1986: return "iec60857-1986";
        case SourceVideoStandard::ConsumerTape:  return "consumer-tape";
        case SourceVideoStandard::None:
        default: return "none";
    }
}

/**
 * @brief Parse source video standard string (case-insensitive)
 * @return true if parsed successfully, false otherwise
 */
inline bool parse_source_video_standard(const std::string& value, SourceVideoStandard& out) {
    std::string lower = value;
    std::transform(lower.begin(), lower.end(), lower.begin(), [](unsigned char c){ return static_cast<char>(std::tolower(c)); });
    if (lower == "none" || lower.empty()) {
        out = SourceVideoStandard::None;
        return true;
    }
    if (lower == "iec60856-1986") {
        out = SourceVideoStandard::IEC60856_1986;
        return true;
    }
    if (lower == "iec60857-1986") {
        out = SourceVideoStandard::IEC60857_1986;
        return true;
    }
    if (lower == "consumer-tape") {
        out = SourceVideoStandard::ConsumerTape;
        return true;
    }
    return false;
}

/**
 * @brief Whether the standard allows LaserDisc VBI data for the given system
 * 
 * LaserDisc VBI is only present on LaserDisc-format sources (IEC standards).
 * Consumer tape and other formats do not include LaserDisc VBI.
 */
inline bool standard_supports_vbi(SourceVideoStandard standard, VideoSystem system) {
    switch (standard) {
        case SourceVideoStandard::IEC60856_1986: return system == VideoSystem::NTSC;
        case SourceVideoStandard::IEC60857_1986: return system == VideoSystem::PAL;
        case SourceVideoStandard::ConsumerTape:  return false;  // Consumer tape does not have LaserDisc VBI
        case SourceVideoStandard::None:
        default: return false;
    }
}

/**
 * @brief Whether the standard allows VITS (Video Insertion Test Signals) for the given system
 * 
 * VITS are only present on LaserDisc-format sources (IEC standards).
 * Consumer tape uses VITC instead of VITS.
 */
inline bool standard_supports_vits(SourceVideoStandard standard, VideoSystem system) {
    // VITS is system-independent for LaserDisc
    (void)system;
    switch (standard) {
        case SourceVideoStandard::IEC60856_1986: return true;  // LaserDisc NTSC includes VITS
        case SourceVideoStandard::IEC60857_1986: return true;  // LaserDisc PAL includes VITS
        case SourceVideoStandard::ConsumerTape:  return false; // Consumer tape does not include VITS
        case SourceVideoStandard::None:
        default: return false;
    }
}

/**
 * @brief Whether the standard allows VITC (Vertical Interval Time Code) insertion
 * 
 * VITC is present on consumer tape formats but not on LaserDisc formats.
 * LaserDisc uses biphase VBI timecode instead of VITC.
 */
inline bool standard_supports_vitc(SourceVideoStandard standard, VideoSystem system) {
    (void)system;  // VITC is system-independent
    switch (standard) {
        case SourceVideoStandard::ConsumerTape:  return true;   // Consumer tape uses VITC timecode
        case SourceVideoStandard::IEC60856_1986: return false;  // LaserDisc NTSC uses VBI, not VITC
        case SourceVideoStandard::IEC60857_1986: return false;  // LaserDisc PAL uses VBI, not VITC
        case SourceVideoStandard::None:
        default: return false;
    }
}

} // namespace encode_orc

#endif // ENCODE_ORC_SOURCE_VIDEO_STANDARD_H
