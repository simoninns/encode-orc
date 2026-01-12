/*
 * File:        test_pal_encode_debug.cpp
 * Module:      encode-orc
 * Purpose:     Debug PAL encoder - test with a single colored line
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: 2026 Simon Inns
 */

#include "pal_encoder.h"
#include "color_conversion.h"
#include "video_parameters.h"
#include <iostream>
#include <iomanip>
#include <cmath>

using namespace encode_orc;

int main() {
    std::cout << "PAL Encoder Debug Test\n\n";
    
    // Create a test frame that's tall enough for PAL
    int32_t width = 4;
    int32_t height = 576;  // Full PAL frame height (288 lines per field × 2 fields)
    
    // Create RGB frame
    FrameBuffer rgb_frame(width, height, FrameBuffer::Format::RGB48);
    
    // Fill with test colors: White, Red, Green, Blue (on first 2 rows)
    rgb_frame.set_rgb_pixel(0, 0, 0xFFFF, 0xFFFF, 0xFFFF);  // White
    rgb_frame.set_rgb_pixel(1, 0, 0xFFFF, 0x0000, 0x0000);  // Red
    rgb_frame.set_rgb_pixel(2, 0, 0x0000, 0xFFFF, 0x0000);  // Green
    rgb_frame.set_rgb_pixel(3, 0, 0x0000, 0x0000, 0xFFFF);  // Blue
    
    // Fill remaining rows with white for padding
    for (int y = 1; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            rgb_frame.set_rgb_pixel(x, y, 0xFFFF, 0xFFFF, 0xFFFF);
        }
    }
    
    // Convert to YUV
    FrameBuffer yuv_frame = ColorConverter::rgb_to_yuv_pal(rgb_frame);
    
    std::cout << "Input RGB -> Output YUV (16-bit values and normalized):\n";
    std::cout << std::setw(10) << "X" << std::setw(10) << "Y_16bit" 
              << std::setw(10) << "U_16bit" << std::setw(10) << "V_16bit"
              << std::setw(10) << "Y_norm" 
              << std::setw(10) << "U_actual" << std::setw(10) << "V_actual" << "\n";
    std::cout << std::string(70, '-') << "\n";
    
    // Display YUV values
    for (int x = 0; x < width; ++x) {
        auto yuv = yuv_frame.get_yuv_pixel(x, 0);
        
        double y_norm = yuv.y / 65535.0;
        
        // Descale U/V
        const double U_MAX = 0.436010;
        const double V_MAX = 0.614975;
        double u_actual = ((yuv.u / 65535.0) - 0.5) * 2.0 * U_MAX;
        double v_actual = ((yuv.v / 65535.0) - 0.5) * 2.0 * V_MAX;
        
        std::cout << std::setw(10) << x 
                  << std::hex << std::setfill('0')
                  << std::setw(10) << "0x" << yuv.y
                  << std::setw(10) << "0x" << yuv.u
                  << std::setw(10) << "0x" << yuv.v
                  << std::dec << std::setfill(' ')
                  << std::setw(10) << std::fixed << std::setprecision(3) << y_norm
                  << std::setw(10) << u_actual
                  << std::setw(10) << v_actual << "\n";
    }
    
    // Now encode with PAL encoder
    std::cout << "\n\nPAL Encoding (single line, first 20 samples):\n";
    
    VideoParameters params = VideoParameters::create_pal_composite();
    PALEncoder encoder(params);
    
    Frame encoded = encoder.encode_frame(yuv_frame, 0);
    
    // Get first field - Line 23 is the first active video line
    const auto& field = encoded.field1();
    
    std::cout << "Field 1, Line 23 (first active), samples 180-200 (raw 16-bit):\n";
    for (int sample = 180; sample <= 200; ++sample) {
        uint16_t value = field.get_sample(sample, 23);
        std::cout << "0x" << std::hex << std::setfill('0') << std::setw(4) << value << " ";
    }
    std::cout << std::dec << "\n";
    
    // Check what the expected white level should be for comparison
    std::cout << "\nExpected levels:\n";
    std::cout << "  Blanking (0 mV):  0x" << std::hex << 0x4000 << std::dec << "\n";
    std::cout << "  White (700 mV):   0x" << std::hex << 0xE000 << std::dec << "\n";
    std::cout << "  Red should modulate around white level\n";
    std::cout << "  Green should modulate around white level\n";
    std::cout << "  Blue should modulate around white level\n";
    
    return 0;
}
