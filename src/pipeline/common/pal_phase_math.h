/*
 * File:        pal_phase_math.h
 * Module:      encode-orc
 * Purpose:     Shared PAL subcarrier phase calculation helper
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: 2026 Simon Inns
 */

#ifndef ENCODE_ORC_PAL_PHASE_MATH_H
#define ENCODE_ORC_PAL_PHASE_MATH_H

#include <cstdint>
#include <cmath>

namespace encode_orc {

static constexpr double PAL_LINES_PER_FIELD = 312.5;
static constexpr double PAL_LINE_RATE_HZ    = 625.0 * 25.0;

/**
 * @brief Calculate the raw subcarrier phase angle at a given sample position
 *        within the PAL 8-field colour framing sequence.
 *
 * PAL has 312.5 lines per field. Using a floating-point field count preserves
 * the mandatory half-line offset between the two fields of each frame, which
 * is what drives the 8-field phase sequence. An integer field count (313)
 * would accumulate a half-line phase error on every other field.
 *
 * @param field_number  0-based absolute field index (not clamped to modulo 8)
 * @param line_number   0-based line index within the field
 * @param sample        0-based sample index within the line
 * @param fsc           Configured subcarrier frequency in Hz
 * @param sample_rate   Configured sample rate in Hz
 * @return Phase angle in radians (not wrapped)
 */
inline double pal_subcarrier_phase(int32_t field_number,
                                   int32_t line_number,
                                   int32_t sample,
                                   double  fsc,
                                   double  sample_rate)
{
    static constexpr double TWO_PI = 2.0 * M_PI;

    const double cycles_per_line = fsc / PAL_LINE_RATE_HZ;

    // Absolute line count — the 0.5 fractional part of lines_per_field
    // is what generates the correct half-line inter-field phase shift.
    const double prev_lines  = static_cast<double>(field_number) * PAL_LINES_PER_FIELD
                             + static_cast<double>(line_number);
    const double prev_cycles = prev_lines * cycles_per_line;

    const double time_phase  = TWO_PI * fsc * static_cast<double>(sample) / sample_rate;
    return TWO_PI * prev_cycles + time_phase;
}

} // namespace encode_orc

#endif // ENCODE_ORC_PAL_PHASE_MATH_H
