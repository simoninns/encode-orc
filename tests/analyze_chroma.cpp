/*
 * File:        analyze_chroma.cpp
 * Module:      encode-orc
 * Purpose:     Analyze chroma content in generated test files
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: 2026 Simon Inns
 */

#include "video_parameters.h"
#include <iostream>
#include <fstream>
#include <cmath>
#include <vector>
#include <algorithm>
#include <complex>
#include <iomanip>

using namespace encode_orc;

// Simple DFT to analyze chroma frequency content
void analyze_line_chroma(const std::vector<uint16_t>& line_data, int32_t active_start, int32_t active_end) {
    std::cout << "Analyzing active video samples [" << active_start << "-" << active_end << "]\n";
    
    // Extract active video samples
    std::vector<double> active_samples;
    for (int32_t i = active_start; i < active_end; ++i) {
        // Convert to normalized -1 to +1 range
        double normalized = (static_cast<double>(line_data[i]) / 65535.0) - 0.5;
        active_samples.push_back(normalized);
    }
    
    // Check min/max of active video
    double min_val = *std::min_element(active_samples.begin(), active_samples.end());
    double max_val = *std::max_element(active_samples.begin(), active_samples.end());
    double range = max_val - min_val;
    
    std::cout << "  Active video normalized range: " << min_val << " to " << max_val 
              << " (span: " << range << ")\n";
    
    // Look at subcarrier frequency (sample rate = 17734475 Hz, fSC = 4433618.75 Hz)
    // So ~4 samples per cycle at full resolution
    const double sample_rate = 17734475.0;
    const double fSC = 4433618.75;
    const int32_t samples_per_cycle = static_cast<int32_t>(sample_rate / fSC + 0.5);
    std::cout << "  Samples per subcarrier cycle: " << samples_per_cycle << "\n";
    
    // Check signal variation at subcarrier frequency
    // Decimate to look at the pattern
    std::cout << "\n  First 12 samples of active video (raw 16-bit):\n  ";
    for (int i = 0; i < std::min(12, static_cast<int>(active_samples.size())); ++i) {
        uint16_t raw = line_data[active_start + i];
        std::cout << "0x" << std::hex << std::setfill('0') << std::setw(4) << raw << " ";
    }
    std::cout << std::dec << "\n";
    
    // Check if there's any AC content at all
    // Calculate RMS of the signal
    double sum_sq = 0.0;
    for (const auto& sample : active_samples) {
        sum_sq += sample * sample;
    }
    double rms = std::sqrt(sum_sq / active_samples.size());
    std::cout << "\n  RMS of active video: " << rms << "\n";
    
    // Check if there's variation in the samples
    double variance = 0.0;
    double mean = 0.0;
    for (const auto& sample : active_samples) {
        mean += sample;
    }
    mean /= active_samples.size();
    
    for (const auto& sample : active_samples) {
        variance += (sample - mean) * (sample - mean);
    }
    variance /= active_samples.size();
    
    std::cout << "  Mean: " << mean << ", Variance: " << variance << "\n";
    
    if (variance < 0.0001) {
        std::cout << "  WARNING: Very low variance - signal appears flat (no modulation)\n";
    }
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        std::cerr << "Usage: " << argv[0] << " <tbc_file>\n";
        return 1;
    }

    VideoParameters params = VideoParameters::create_pal_composite();
    
    std::string filename = argv[1];
    std::ifstream file(filename, std::ios::binary);
    if (!file) {
        std::cerr << "Error: Cannot open file " << filename << "\n";
        return 1;
    }

    // Read first field
    const int32_t FIELD_WIDTH = params.field_width;
    const int32_t FIELD_HEIGHT = params.field_height;
    const int32_t SAMPLES_PER_FIELD = FIELD_WIDTH * FIELD_HEIGHT;
    
    std::vector<uint16_t> field_data(SAMPLES_PER_FIELD);
    file.read(reinterpret_cast<char*>(field_data.data()), SAMPLES_PER_FIELD * 2);
    
    if (!file) {
        std::cerr << "Error: Could not read field data\n";
        return 1;
    }

    std::cout << "Chroma Analysis\n";
    std::cout << "PAL Parameters:\n";
    std::cout << "  Field: " << FIELD_WIDTH << "x" << FIELD_HEIGHT << " = " << SAMPLES_PER_FIELD << " samples\n";
    std::cout << "  Subcarrier: " << params.fSC << " Hz\n";
    std::cout << "  Sample rate: " << params.sample_rate << " Hz\n";
    std::cout << "  Active video: " << params.active_video_start << "-" << params.active_video_end << "\n\n";
    
    // Analyze a few lines from the active video area
    // Start from line 30 (middle of active area)
    std::cout << "Analyzing lines from active video area:\n\n";
    
    for (int line_offset = 30; line_offset <= 50; line_offset += 10) {
        int32_t line_start = line_offset * FIELD_WIDTH;
        std::vector<uint16_t> line_data(field_data.begin() + line_start, 
                                       field_data.begin() + line_start + FIELD_WIDTH);
        
        std::cout << "Line " << line_offset << ":\n";
        analyze_line_chroma(line_data, params.active_video_start, params.active_video_end);
        std::cout << "\n";
    }
    
    return 0;
}
