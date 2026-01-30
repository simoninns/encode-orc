/*
 * File:        field_preprocessor.cpp
 * Module:      encode-orc
 * Purpose:     Field preprocessing with optional filtering (Phase 6)
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: 2026 Simon Inns
 */

#include "field_preprocessor.h"
#include "fir_filter.h"
#include <cmath>
#include <algorithm>

namespace encode_orc {

// ============================================================================
// ChromaFilter Implementation
// ============================================================================

ChromaFilter::ChromaFilter(FilterType type)
    : type_(type) {
    if (type == PAL_1_3MHZ) {
        coefficients_ = get_pal_1_3mhz_coefficients();
    } else if (type == NTSC_600KHZ) {
        coefficients_ = get_ntsc_600khz_coefficients();
    }
}

ChromaFilter::ChromaFilter(const std::vector<double>& custom_coefficients)
    : type_(CUSTOM), coefficients_(custom_coefficients) {}

std::vector<double> ChromaFilter::get_pal_1_3mhz_coefficients() {
    // PAL 1.3 MHz low-pass filter coefficients (25-tap FIR)
    // These are standardized coefficients from ld-decode-tools
    return {
        -0.0010922, -0.0016592, 0.0000000, 0.0037652, 0.0081783,
        0.0120000, 0.0130581, 0.0095783, 0.0020000, -0.0074861,
        -0.0157135, -0.0213135, -0.0227500, -0.0183500, -0.0078283,
        0.0074861, 0.0250000, 0.0413500, 0.0540000, 0.0606000,
        0.0600000, 0.0540000, 0.0413500, 0.0250000, 0.0074861
    };
}

std::vector<double> ChromaFilter::get_ntsc_600khz_coefficients() {
    // NTSC 600 kHz low-pass filter coefficients (25-tap FIR)
    // Tighter cutoff than PAL for narrower chroma bandwidth
    return {
        -0.0025000, -0.0035000, -0.0025000, 0.0015000, 0.0080000,
        0.0145000, 0.0185000, 0.0180000, 0.0115000, 0.0005000,
        -0.0130000, -0.0255000, -0.0330000, -0.0330000, -0.0255000,
        -0.0130000, 0.0005000, 0.0115000, 0.0180000, 0.0185000,
        0.0145000, 0.0080000, 0.0015000, -0.0025000, -0.0035000
    };
}

void ChromaFilter::apply_luma(uint16_t* /* y_data */, int32_t /* width */, int32_t /* height */) {
    // No-op: chroma filter doesn't affect luma
}

void ChromaFilter::apply_chroma_u(uint16_t* u_data, int32_t width, int32_t height) {
    if (!enabled_ || coefficients_.empty()) return;
    apply_filter(u_data, width, height);
}

void ChromaFilter::apply_chroma_v(uint16_t* v_data, int32_t width, int32_t height) {
    if (!enabled_ || coefficients_.empty()) return;
    apply_filter(v_data, width, height);
}

void ChromaFilter::apply_filter(uint16_t* data, int32_t width, int32_t height) {
    // Create FIR filter with current coefficients
    FIRFilter filter(coefficients_);
    
    // Apply horizontally to each line
    for (int32_t line = 0; line < height; ++line) {
        std::vector<double> line_data;
        line_data.reserve(width);
        
        // Convert to double for filtering
        for (int32_t i = 0; i < width; ++i) {
            line_data.push_back(static_cast<double>(data[line * width + i]));
        }
        
        // Apply filter
        filter.apply(line_data);
        
        // Convert back to uint16_t
        for (int32_t i = 0; i < width; ++i) {
            double val = line_data[i];
            val = std::max(0.0, std::min(65535.0, val));
            data[line * width + i] = static_cast<uint16_t>(val + 0.5);
        }
    }
}

// ============================================================================
// LumaFilter Implementation
// ============================================================================

LumaFilter::LumaFilter(FilterType type)
    : type_(type) {
    if (type == PAL_5_5MHZ) {
        coefficients_ = get_pal_5_5mhz_coefficients();
    } else if (type == NTSC_3_6MHZ) {
        coefficients_ = get_ntsc_3_6mhz_coefficients();
    }
}

LumaFilter::LumaFilter(const std::vector<double>& custom_coefficients)
    : type_(CUSTOM), coefficients_(custom_coefficients) {}

std::vector<double> LumaFilter::get_pal_5_5mhz_coefficients() {
    // PAL 5.5 MHz low-pass filter coefficients (25-tap FIR)
    // Wider bandwidth than chroma for higher-frequency luma detail
    return {
        0.0016592, 0.0016592, -0.0000000, -0.0033652, -0.0056783,
        -0.0060000, -0.0030581, 0.0020783, 0.0070000, 0.0110861,
        0.0123135, 0.0098135, 0.0032500, -0.0063500, -0.0168283,
        -0.0260861, -0.0320000, -0.0313500, -0.0240000, -0.0106000,
        0.0100000, 0.0340000, 0.0563500, 0.0750000, 0.0874861
    };
}

std::vector<double> LumaFilter::get_ntsc_3_6mhz_coefficients() {
    // NTSC 3.6 MHz low-pass filter coefficients (25-tap FIR)
    // Narrower than PAL due to different bandwidth allocation
    return {
        0.0010000, 0.0010000, -0.0005000, -0.0030000, -0.0055000,
        -0.0080000, -0.0090000, -0.0075000, -0.0035000, 0.0025000,
        0.0100000, 0.0170000, 0.0215000, 0.0225000, 0.0185000,
        0.0100000, -0.0010000, -0.0140000, -0.0260000, -0.0350000,
        -0.0385000, -0.0350000, -0.0260000, -0.0140000, -0.0010000
    };
}

void LumaFilter::apply_luma(uint16_t* y_data, int32_t width, int32_t height) {
    if (!enabled_ || coefficients_.empty()) return;
    apply_filter(y_data, width, height);
}

void LumaFilter::apply_chroma_u(uint16_t* /* u_data */, int32_t /* width */, int32_t /* height */) {
    // No-op: luma filter doesn't affect chroma
}

void LumaFilter::apply_chroma_v(uint16_t* /* v_data */, int32_t /* width */, int32_t /* height */) {
    // No-op: luma filter doesn't affect chroma
}

void LumaFilter::apply_filter(uint16_t* data, int32_t width, int32_t height) {
    // Create FIR filter with current coefficients
    FIRFilter filter(coefficients_);
    
    // Apply horizontally to each line
    for (int32_t line = 0; line < height; ++line) {
        std::vector<double> line_data;
        line_data.reserve(width);
        
        // Convert to double for filtering
        for (int32_t i = 0; i < width; ++i) {
            line_data.push_back(static_cast<double>(data[line * width + i]));
        }
        
        // Apply filter
        filter.apply(line_data);
        
        // Convert back to uint16_t
        for (int32_t i = 0; i < width; ++i) {
            double val = line_data[i];
            val = std::max(0.0, std::min(65535.0, val));
            data[line * width + i] = static_cast<uint16_t>(val + 0.5);
        }
    }
}

} // namespace encode_orc
