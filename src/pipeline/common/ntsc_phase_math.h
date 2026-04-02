/*
 * File:        ntsc_phase_math.h
 * Module:      encode-orc
 * Purpose:     Shared NTSC/PAL-M subcarrier phase calculation helper
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: 2026 Simon Inns
 */

#ifndef ENCODE_ORC_NTSC_PHASE_MATH_H
#define ENCODE_ORC_NTSC_PHASE_MATH_H

#include <cstdint>
#include <cmath>

namespace encode_orc {

// Both standard NTSC and PAL-M use 525-line 60 Hz geometry.
static constexpr double NTSC_LINES_PER_FIELD = 262.5;
static constexpr double NTSC_LINE_RATE_HZ    = 525.0 * (30000.0 / 1001.0);

/**
 * @brief Calculate the raw subcarrier phase angle at a given sample position
 *        within the NTSC (or PAL-M) colour framing sequence.
 *
 * NTSC has 262.5 lines per field. Using a floating-point field count preserves
 * the mandatory half-line offset between the two fields of each frame, which
 * produces the 4-field colour framing sequence (±90° per field). An integer
 * count (262 or 263) would accumulate a half-line phase error on every other
 * field.
 *
 * PAL-M shares the same 525-line geometry as NTSC; pass the PAL-M subcarrier
 * frequency as @p fsc to obtain the correct PAL-M phase.
 *
 * @param field_number  0-based absolute field index (not clamped to modulo 4)
 * @param line_number   0-based line index within the field
 * @param sample        0-based sample index within the line
 * @param fsc           Configured subcarrier frequency in Hz
 * @param sample_rate   Configured sample rate in Hz
 * @return Phase angle in radians (not wrapped)
 */
inline double ntsc_subcarrier_phase(int32_t field_number,
                                    int32_t line_number,
                                    int32_t sample,
                                    double  fsc,
                                    double  sample_rate)
{
    static constexpr double TWO_PI = 2.0 * M_PI;

    // Derive cycles/line from the configured fSC so that the formula is equally
    // correct for standard NTSC (≈227.5 cycles/line) and PAL-M (different fSC).
    const double cycles_per_line = fsc / NTSC_LINE_RATE_HZ;

    // Absolute line count — the 0.5 fractional part of lines_per_field is what
    // generates the correct half-line inter-field phase shift.
    const double prev_lines  = static_cast<double>(field_number) * NTSC_LINES_PER_FIELD
                             + static_cast<double>(line_number);
    const double prev_cycles = prev_lines * cycles_per_line;

    const double time_phase  = TWO_PI * fsc * static_cast<double>(sample) / sample_rate;
    return TWO_PI * prev_cycles + time_phase;
}

} // namespace encode_orc

#endif // ENCODE_ORC_NTSC_PHASE_MATH_H
