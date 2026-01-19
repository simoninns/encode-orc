/*
 * File:        laserdisc_standard.h
 * Module:      encode-orc
 * Purpose:     LaserDisc standards helpers (VBI/VITS capabilities)
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: 2026 Simon Inns
 */

#ifndef ENCODE_ORC_LASERDISC_STANDARD_H
#define ENCODE_ORC_LASERDISC_STANDARD_H

#include "video_parameters.h"
#include <algorithm>
#include <string>

namespace encode_orc {

/**
 * @brief Supported LaserDisc standards
 */
enum class LaserDiscStandard {
    None,
    IEC60856_1986,  // NTSC
    IEC60857_1986,  // PAL
    ConsumerTape    // Consumer tape (non-LaserDisc) formats
};

/**
 * @brief Convert LaserDiscStandard to string (lowercase)
 */
inline std::string laserdisc_standard_to_string(LaserDiscStandard standard) {
    switch (standard) {
        case LaserDiscStandard::IEC60856_1986: return "iec60856-1986";
        case LaserDiscStandard::IEC60857_1986: return "iec60857-1986";
        case LaserDiscStandard::ConsumerTape:  return "consumer-tape";
        case LaserDiscStandard::None:
        default: return "none";
    }
}

/**
 * @brief Parse LaserDisc standard string (case-insensitive)
 * @return true if parsed successfully, false otherwise
 */
inline bool parse_laserdisc_standard(const std::string& value, LaserDiscStandard& out) {
    std::string lower = value;
    std::transform(lower.begin(), lower.end(), lower.begin(), [](unsigned char c){ return static_cast<char>(std::tolower(c)); });
    if (lower == "none" || lower.empty()) {
        out = LaserDiscStandard::None;
        return true;
    }
    if (lower == "iec60856-1986") {
        out = LaserDiscStandard::IEC60856_1986;
        return true;
    }
    if (lower == "iec60857-1986") {
        out = LaserDiscStandard::IEC60857_1986;
        return true;
    }
    if (lower == "consumer-tape") {
        out = LaserDiscStandard::ConsumerTape;
        return true;
    }
    return false;
}

/**
 * @brief Whether the standard allows VBI data for the given system
 */
inline bool standard_supports_vbi(LaserDiscStandard standard, VideoSystem system) {
    switch (standard) {
        case LaserDiscStandard::IEC60856_1986: return system == VideoSystem::NTSC;
        case LaserDiscStandard::IEC60857_1986: return system == VideoSystem::PAL;
        case LaserDiscStandard::ConsumerTape:  return false;  // Do not emit LaserDisc-style VBI
        case LaserDiscStandard::None:
        default: return false;
    }
}

/**
 * @brief Whether the standard allows VITS insertion for the given system
 */
inline bool standard_supports_vits(LaserDiscStandard standard, VideoSystem system) {
    // LaserDisc VITS mirrors VBI support
    (void)system;  // VITS is system-independent for LaserDisc
    switch (standard) {
        case LaserDiscStandard::IEC60856_1986: return true;
        case LaserDiscStandard::IEC60857_1986: return true;
        case LaserDiscStandard::ConsumerTape:  return false;  // Consumer tape uses VITC, not VITS
        case LaserDiscStandard::None:
        default: return false;
    }
}

/**
 * @brief Whether the standard allows VITC (timecode) insertion for the given system
 */
inline bool standard_supports_vitc(LaserDiscStandard standard, VideoSystem system) {
    (void)system;  // VITC is system-independent
    switch (standard) {
        case LaserDiscStandard::ConsumerTape:  return true;   // Consumer tape uses VITC timecode
        case LaserDiscStandard::IEC60856_1986:
        case LaserDiscStandard::IEC60857_1986:
        case LaserDiscStandard::None:
        default: return false;
    }
}

} // namespace encode_orc

#endif // ENCODE_ORC_LASERDISC_STANDARD_H
