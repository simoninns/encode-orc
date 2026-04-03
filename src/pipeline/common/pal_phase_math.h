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

/**
 * @brief LUT-based PAL V-switch polarity at any TBC row.
 *
 * Replaces the accumulated-line-count formula that used an incorrect 313
 * constant (instead of the correct 312.5) for second-field offsets.  Using
 * 313 (odd) instead of 312 (even) flipped the parity bit for every second
 * field, rotating the colour burst by 180° and mis-detecting phase IDs 2, 3,
 * 6, and 7.
 *
 * The table gives the V-switch sign at the first ld-decode vote row for each
 * phase: TBC row 9 for first fields, TBC row 10 for second fields.  Lines
 * above and below that reference alternate with normal ±1 parity.
 *
 * Values derived analytically: for each field_number 0..7, compute
 * pal_subcarrier_phase(fn, ref_row, burst_gate_start) + v * 135° and pick
 * the v ∈ {+1,-1} such that cos(total) ≥ 0 iff ld-decode's
 * determine_field_number() requires rising=True for that phase.
 *
 * @param phase_id       1-indexed PAL 8-field phase ID (1..8)
 * @param tbc_row        TBC buffer row index (0-based field-line index, NOT interlaced frame line number)
 * @param is_first_field true for first fields (field_number % 2 == 0)
 * @return +1 (Rising) or -1 (Falling)
 */
inline int32_t pal_v_switch(int32_t phase_id, int32_t tbc_row, bool is_first_field)
{
    // V-switch sign at the first vote row for each of the 8 phases.
    // Derived analytically: for each field_number 0..7, compute
    //   pal_subcarrier_phase(fn, ref_row, burst_gate_start) + v * 135°
    // and choose v such that cos(total) matches the rising/falling polarity
    // required by ld-decode's determine_field_number() logic.
    // φ:  1    2    3    4    5    6    7    8
    static constexpr int8_t kRefSign[8] = {
        +1,  -1,  -1,  +1,  +1,  -1,  -1,  +1
    };

    const int32_t ref_row = is_first_field ? 9 : 10;
    const int32_t base    = kRefSign[phase_id - 1];
    const int32_t delta   = tbc_row - ref_row;
    // Alternates every line; ((delta % 2) + 2) % 2 is sign-safe mod-2.
    return (((delta % 2) + 2) % 2 == 0) ? base : -base;
}

} // namespace encode_orc

#endif // ENCODE_ORC_PAL_PHASE_MATH_H
