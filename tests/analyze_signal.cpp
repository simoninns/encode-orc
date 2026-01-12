/*
 * File:        analyze_signal.cpp
 * Module:      encode-orc
 * Purpose:     Analyze signal amplitudes in generated test files
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: 2026 Simon Inns
 */

#include "pal_encoder.h"
#include "color_conversion.h"
#include "video_parameters.h"
#include "frame_buffer.h"
#include <iostream>
#include <cmath>
#include <fstream>
#include <algorithm>

using namespace encode_orc;

int main(int argc, char* argv[]) {
    if (argc < 2) {
        std::cerr << "Usage: " << argv[0] << " <tbc_file>\n";
        return 1;
    }

    std::string filename = argv[1];
    
    // Open TBC file
    std::ifstream file(filename, std::ios::binary);
    if (!file) {
        std::cerr << "Error: Cannot open file " << filename << "\n";
        return 1;
    }

    // Read up to first 10 fields worth of data
    // PAL field: 1135 x 313 = 355,255 samples per field
    // Each sample is 2 bytes (uint16_t)
    const int32_t FIELD_WIDTH = 1135;
    const int32_t FIELD_HEIGHT = 313;
    const int32_t SAMPLES_PER_FIELD = FIELD_WIDTH * FIELD_HEIGHT;
    const int32_t FIELDS_TO_ANALYZE = 10;
    const int32_t TOTAL_SAMPLES = SAMPLES_PER_FIELD * FIELDS_TO_ANALYZE;

    std::vector<uint16_t> samples(TOTAL_SAMPLES);
    file.read(reinterpret_cast<char*>(samples.data()), TOTAL_SAMPLES * 2);
    
    if (!file) {
        std::cerr << "Error: Could not read " << TOTAL_SAMPLES << " samples\n";
        return 1;
    }

    // Analyze signal statistics
    uint16_t min_sample = 65535;
    uint16_t max_sample = 0;
    double sum = 0.0;
    int32_t count = 0;

    for (const auto& sample : samples) {
        min_sample = std::min(min_sample, sample);
        max_sample = std::max(max_sample, sample);
        sum += sample;
        count++;
    }

    double mean = sum / count;

    // Calculate standard deviation
    double variance = 0.0;
    for (const auto& sample : samples) {
        double diff = sample - mean;
        variance += diff * diff;
    }
    variance /= count;
    double stddev = std::sqrt(variance);

    // Convert to mV for display (assuming 16-bit range = ~903.3mV)
    // 0x0000 = -300mV, 0x4000 = 0mV, 0xE000 = 700mV, 0xFFFF = ~903.3mV
    auto to_mv = [](uint16_t val) {
        return (val / 65535.0) * 1203.3 - 300.0;
    };

    std::cout << "Signal Analysis for " << filename << "\n";
    std::cout << "(" << FIELDS_TO_ANALYZE << " fields, " << TOTAL_SAMPLES << " samples)\n\n";
    std::cout << "Sample Statistics (16-bit):\n";
    std::cout << "  Min:     0x" << std::hex << min_sample << std::dec 
              << " (" << to_mv(min_sample) << " mV)\n";
    std::cout << "  Max:     0x" << std::hex << max_sample << std::dec 
              << " (" << to_mv(max_sample) << " mV)\n";
    std::cout << "  Mean:    0x" << std::hex << static_cast<uint16_t>(mean) << std::dec 
              << " (" << to_mv(static_cast<uint16_t>(mean)) << " mV)\n";
    std::cout << "  StdDev:  ±" << stddev << " samples (±" 
              << (stddev / 65535.0 * 1203.3) << " mV)\n";

    std::cout << "\nExpected ranges (PAL composite video):\n";
    std::cout << "  Sync:     -300 mV  (0x0000)\n";
    std::cout << "  Blanking:    0 mV  (0x4000)\n";
    std::cout << "  White:    700 mV  (0xE000)\n";
    std::cout << "  Max:      903 mV  (0xFFFF)\n";

    // Check if values look reasonable
    std::cout << "\nValidation:\n";
    bool valid = true;
    
    if (min_sample == 0) {
        std::cout << "  WARNING: Minimum sample is at sync (-300mV)\n";
    }
    if (max_sample > 0xE000 * 1.1) {  // More than 10% above white
        std::cout << "  WARNING: Maximum sample significantly exceeds white level\n";
        valid = false;
    }
    if (max_sample > 0xF000) {  // More than 15/16 of available range
        std::cout << "  WARNING: Samples using >93% of 16-bit range - may be clipping\n";
        valid = false;
    }

    if (valid) {
        std::cout << "  OK: Signal levels look reasonable\n";
    }

    return valid ? 0 : 1;
}
