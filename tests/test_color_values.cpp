/*
 * File:        test_color_values.cpp
 * Module:      encode-orc
 * Purpose:     Test and display color values for RGB->YUV conversion
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: 2026 Simon Inns
 */

#include "color_conversion.h"
#include "frame_buffer.h"
#include <iostream>
#include <iomanip>

using namespace encode_orc;

int main() {
    std::cout << "Testing RGB to YUV conversion for color bars\n\n";
    
    // Create test colors
    struct TestColor {
        const char* name;
        uint16_t r, g, b;
    };
    
    const TestColor colors[] = {
        {"White",   0xFFFF, 0xFFFF, 0xFFFF},
        {"Yellow",  0xFFFF, 0xFFFF, 0x0000},
        {"Cyan",    0x0000, 0xFFFF, 0xFFFF},
        {"Green",   0x0000, 0xFFFF, 0x0000},
        {"Magenta", 0xFFFF, 0x0000, 0xFFFF},
        {"Red",     0xFFFF, 0x0000, 0x0000},
        {"Blue",    0x0000, 0x0000, 0xFFFF},
        {"Black",   0x0000, 0x0000, 0x0000},
    };
    
    // Create a small frame with these colors
    FrameBuffer rgb_frame(8, 1, FrameBuffer::Format::RGB48);
    for (int i = 0; i < 8; ++i) {
        rgb_frame.set_rgb_pixel(i, 0, colors[i].r, colors[i].g, colors[i].b);
    }
    
    // Convert to YUV
    FrameBuffer yuv_frame = ColorConverter::rgb_to_yuv_pal(rgb_frame);
    
    // Display results
    std::cout << std::setw(8) << "Color" 
              << std::setw(8) << "R" 
              << std::setw(8) << "G" 
              << std::setw(8) << "B"
              << std::setw(8) << "Y"
              << std::setw(8) << "U"
              << std::setw(8) << "V" << "\n";
    
    std::cout << std::string(54, '-') << "\n";
    
    for (int i = 0; i < 8; ++i) {
        auto rgb = rgb_frame.get_rgb_pixel(i, 0);
        auto yuv = yuv_frame.get_yuv_pixel(i, 0);
        
        // Convert 16-bit values to normalized 0-1 for display
        double y_norm = yuv.y / 65535.0;
        double u_norm = yuv.u / 65535.0;
        double v_norm = yuv.v / 65535.0;
        
        // Convert U/V back to original ranges for display
        const double U_MAX = 0.436010;
        const double V_MAX = 0.614975;
        double u_actual = (u_norm - 0.5) * 2.0 * U_MAX;
        double v_actual = (v_norm - 0.5) * 2.0 * V_MAX;
        
        std::cout << std::setw(8) << colors[i].name;
        std::cout << std::setw(8) << std::fixed << std::setprecision(2) 
                  << (rgb.r / 65535.0);
        std::cout << std::setw(8) << (rgb.g / 65535.0);
        std::cout << std::setw(8) << (rgb.b / 65535.0);
        std::cout << std::setw(8) << y_norm;
        std::cout << std::setw(8) << u_actual;
        std::cout << std::setw(8) << v_actual << "\n";
    }
    
    std::cout << "\nU/V ranges for PAL: U ∈ [-0.436, 0.436], V ∈ [-0.615, 0.615]\n";
    
    return 0;
}
