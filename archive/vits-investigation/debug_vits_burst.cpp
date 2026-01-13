/*
 * Debug utility for VITS color burst signal
 */

#include "../include/video_parameters.h"
#include "../include/vits_signal_generator.h"
#include <cstdint>
#include <cstdio>
#include <cmath>

using namespace encode_orc;

int main() {
    // Get PAL parameters
    VideoParameters params = VideoParameters::create_pal_composite();
    
    printf("PAL Video Parameters:\n");
    printf("  Sample rate: %.0f MHz\n", params.sample_rate / 1e6);
    printf("  fSC: %.6f MHz\n", params.fSC / 1e6);
    printf("  Samples per cycle: %.2f\n", params.sample_rate / params.fSC);
    printf("  Field width: %d samples\n", params.field_width);
    printf("  Burst start: %d\n", params.colour_burst_start);
    printf("  Burst end: %d\n", params.colour_burst_end);
    printf("  Burst samples: %d\n", params.colour_burst_end - params.colour_burst_start);
    printf("  Blanking level: 0x%04x\n", params.blanking_16b_ire);
    printf("  White level: 0x%04x\n", params.white_16b_ire);
    printf("\n");
    
    // Create generator
    PALVITSSignalGenerator generator(params);
    
    // Generate a line with just color burst
    uint16_t* line_buffer = new uint16_t[params.field_width];
    generator.generateColorBurst(line_buffer, 6, 0);
    
    // Analyze burst region
    printf("Color Burst Analysis (Field 0, Line 6):\n");
    printf("Sample | Value  | Diff from blanking\n");
    printf("-------|--------|-------------------\n");
    
    uint16_t blanking = params.blanking_16b_ire;
    
    for (int32_t i = params.colour_burst_start; i < params.colour_burst_end && i < params.field_width; i += 2) {
        int32_t value = line_buffer[i];
        int32_t diff = value - blanking;
        printf("%5d | 0x%04x | %+6d\n", i, value, diff);
    }
    
    printf("\n");
    
    // Check statistics
    int32_t min_val = 65535, max_val = 0;
    int64_t sum = 0;
    int32_t burst_count = params.colour_burst_end - params.colour_burst_start;
    
    for (int32_t i = params.colour_burst_start; i < params.colour_burst_end; ++i) {
        uint16_t val = line_buffer[i];
        sum += val;
        if (val < min_val) min_val = val;
        if (val > max_val) max_val = val;
    }
    
    double avg = static_cast<double>(sum) / burst_count;
    
    printf("Burst Statistics:\n");
    printf("  Min: 0x%04x\n", min_val);
    printf("  Max: 0x%04x\n", max_val);
    printf("  Avg: 0x%04x (%.0f)\n", (uint16_t)avg, avg);
    printf("  Range: 0x%04x\n", max_val - min_val);
    printf("  Expected burst amplitude: ±%.0f mV\n", (params.white_16b_ire - 0) * 300.0 / 1000.0 / 4.0);
    
    delete[] line_buffer;
    return 0;
}
