/*
 * File:        test_card_generator.cpp
 * Module:      encode-orc
 * Purpose:     Test card and color bars generator implementation
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: 2026 Simon Inns
 */

#include "test_card_generator.h"
#include "color_conversion.h"
#include <stdexcept>
#include <cmath>

namespace encode_orc {

FrameBuffer TestCardGenerator::generate(Type type, const VideoParameters& params) {
    switch (type) {
        case Type::COLOR_BARS:
            // Generate appropriate color bars for the system
            // PAL uses EBU color bars, NTSC uses EIA color bars
            if (params.system == VideoSystem::PAL) {
                return generate_ebu_bars(params);
            } else {
                return generate_eia_bars(params);
            }
        case Type::PM5544:
        case Type::TESTCARD_F:
            throw std::runtime_error("Test card type not yet implemented");
        default:
            throw std::runtime_error("Unknown test card type");
    }
}

FrameBuffer TestCardGenerator::generate_ebu_bars(const VideoParameters& params) {
    // EBU color bars for PAL: 100/0/75/0
    // Eight vertical bars:
    // 1. White (100%)
    // 2. Yellow (75%)
    // 3. Cyan (75%)
    // 4. Green (75%)
    // 5. Magenta (75%)
    // 6. Red (75%)
    // 7. Blue (75%)
    // 8. Black (0%)
    
    // Frame dimensions: 720x576 for PAL (standard active picture)
    int32_t width = 720;
    int32_t height = 576;
    
    // Create frame buffer in YUV format
    FrameBuffer frame(width, height, FrameBuffer::Format::YUV444P16);
    
    // Get pointers to YUV planes
    uint16_t* data = frame.data().data();
    int32_t pixel_count = width * height;
    uint16_t* y_plane = data;
    uint16_t* u_plane = data + pixel_count;
    uint16_t* v_plane = data + pixel_count * 2;
    
    // EBU bar colors (RGB values as fractions of full scale)
    // Using 75% saturation for colors (except white and black)
    struct Color {
        double r, g, b;
    };
    
    const Color ebu_bars[] = {
        {1.0, 1.0, 1.0},   // White (100%)
        {0.75, 0.75, 0.0}, // Yellow (75%)
        {0.0, 0.75, 0.75}, // Cyan (75%)
        {0.0, 0.75, 0.0},  // Green (75%)
        {0.75, 0.0, 0.75}, // Magenta (75%)
        {0.75, 0.0, 0.0},  // Red (75%)
        {0.0, 0.0, 0.75},  // Blue (75%)
        {0.0, 0.0, 0.0}    // Black (0%)
    };
    
    int32_t bars_per_line = 8;
    int32_t bar_width = width / bars_per_line;
    
    // Transition width in pixels - smooth the edges to reduce HF artifacts
    // Using ~4 pixels provides good balance between sharp bars and reduced ringing
    const int32_t transition_width = 4;
    
    // Generate color bars with smooth transitions
    for (int32_t y = 0; y < height; ++y) {
        for (int32_t x = 0; x < width; ++x) {
            int32_t bar = x / bar_width;
            if (bar >= bars_per_line) bar = bars_per_line - 1;
            
            // Calculate position within the bar
            int32_t x_in_bar = x % bar_width;
            
            // Determine if we're in a transition zone
            double blend = 0.0;  // 0.0 = fully current bar, 1.0 = fully next bar
            bool in_transition = false;
            
            if (x_in_bar >= (bar_width - transition_width) && bar < (bars_per_line - 1)) {
                // Right edge transition to next bar
                in_transition = true;
                int32_t transition_pos = x_in_bar - (bar_width - transition_width);
                // Use raised cosine (Hann window) for smooth transition
                blend = 0.5 * (1.0 - std::cos(3.14159265359 * transition_pos / transition_width));
            }
            
            Color color;
            if (in_transition) {
                // Blend between current bar and next bar
                const Color& color1 = ebu_bars[bar];
                const Color& color2 = ebu_bars[bar + 1];
                color.r = color1.r * (1.0 - blend) + color2.r * blend;
                color.g = color1.g * (1.0 - blend) + color2.g * blend;
                color.b = color1.b * (1.0 - blend) + color2.b * blend;
            } else {
                color = ebu_bars[bar];
            }
            
            // Convert RGB (0.0-1.0) to 16-bit (0-65535)
            uint16_t r16 = static_cast<uint16_t>(color.r * 65535.0);
            uint16_t g16 = static_cast<uint16_t>(color.g * 65535.0);
            uint16_t b16 = static_cast<uint16_t>(color.b * 65535.0);
            
            // Convert to YUV
            uint16_t yuv_y, yuv_u, yuv_v;
            rgb_to_yuv16(r16, g16, b16, yuv_y, yuv_u, yuv_v, params);
            
            // Store in planes
            int32_t pixel_idx = y * width + x;
            y_plane[pixel_idx] = yuv_y;
            u_plane[pixel_idx] = yuv_u;
            v_plane[pixel_idx] = yuv_v;
        }
    }
    
    return frame;
}

FrameBuffer TestCardGenerator::generate_eia_bars(const VideoParameters& params) {
    // EIA color bars for NTSC
    // Similar to EBU but with NTSC color specifications and brightness levels
    
    // Frame dimensions: 720x486 for NTSC (standard active picture)
    int32_t width = 720;
    int32_t height = 486;
    
    // Create frame buffer in YUV format
    FrameBuffer frame(width, height, FrameBuffer::Format::YUV444P16);
    
    // Get pointers to YUV planes
    uint16_t* data = frame.data().data();
    int32_t pixel_count = width * height;
    uint16_t* y_plane = data;
    uint16_t* u_plane = data + pixel_count;
    uint16_t* v_plane = data + pixel_count * 2;
    
    // SMPTE bar colors (RGB values as fractions of full scale)
    struct Color {
        double r, g, b;
    };
    
    const Color smpte_bars[] = {
        {0.75, 0.75, 0.75}, // 75% White
        {0.75, 0.75, 0.0},  // Yellow
        {0.0, 0.75, 0.75},  // Cyan
        {0.0, 0.75, 0.0},   // Green
        {0.75, 0.0, 0.75},  // Magenta
        {0.75, 0.0, 0.0},   // Red
        {0.0, 0.0, 0.75},   // Blue
        {0.0, 0.0, 0.0}     // Black
    };
    
    int32_t bars_per_line = 8;
    int32_t bar_width = width / bars_per_line;
    
    // Transition width in pixels - smooth the edges to reduce HF artifacts
    const int32_t transition_width = 4;
    
    // Generate color bars with smooth transitions
    for (int32_t y = 0; y < height; ++y) {
        for (int32_t x = 0; x < width; ++x) {
            int32_t bar = x / bar_width;
            if (bar >= bars_per_line) bar = bars_per_line - 1;
            
            // Calculate position within the bar
            int32_t x_in_bar = x % bar_width;
            
            // Determine if we're in a transition zone
            double blend = 0.0;
            bool in_transition = false;
            
            if (x_in_bar >= (bar_width - transition_width) && bar < (bars_per_line - 1)) {
                // Right edge transition to next bar
                in_transition = true;
                int32_t transition_pos = x_in_bar - (bar_width - transition_width);
                // Use raised cosine (Hann window) for smooth transition
                blend = 0.5 * (1.0 - std::cos(3.14159265359 * transition_pos / transition_width));
            }
            
            Color color;
            if (in_transition) {
                // Blend between current bar and next bar
                const Color& color1 = smpte_bars[bar];
                const Color& color2 = smpte_bars[bar + 1];
                color.r = color1.r * (1.0 - blend) + color2.r * blend;
                color.g = color1.g * (1.0 - blend) + color2.g * blend;
                color.b = color1.b * (1.0 - blend) + color2.b * blend;
            } else {
                color = smpte_bars[bar];
            }
            
            // Convert RGB (0.0-1.0) to 16-bit (0-65535)
            uint16_t r16 = static_cast<uint16_t>(color.r * 65535.0);
            uint16_t g16 = static_cast<uint16_t>(color.g * 65535.0);
            uint16_t b16 = static_cast<uint16_t>(color.b * 65535.0);
            
            // Convert to YUV
            uint16_t yuv_y, yuv_u, yuv_v;
            rgb_to_yuv16(r16, g16, b16, yuv_y, yuv_u, yuv_v, params);
            
            // Store in planes
            int32_t pixel_idx = y * width + x;
            y_plane[pixel_idx] = yuv_y;
            u_plane[pixel_idx] = yuv_u;
            v_plane[pixel_idx] = yuv_v;
        }
    }
    
    return frame;
}

void TestCardGenerator::rgb_to_yuv16(uint16_t r, uint16_t g, uint16_t b,
                                     uint16_t& y, uint16_t& u, uint16_t& v,
                                     const VideoParameters& /* params */) {
    // Convert RGB to YUV using ITU-R BT.601 for PAL or NTSC
    // Y  =  0.299*R + 0.587*G + 0.114*B
    // U  = -0.147*R - 0.289*G + 0.436*B + 0.5
    // V  =  0.615*R - 0.515*G - 0.100*B + 0.5
    
    double r_norm = static_cast<double>(r) / 65535.0;
    double g_norm = static_cast<double>(g) / 65535.0;
    double b_norm = static_cast<double>(b) / 65535.0;
    
    double y_norm = 0.299 * r_norm + 0.587 * g_norm + 0.114 * b_norm;
    double u_norm = -0.147 * r_norm - 0.289 * g_norm + 0.436 * b_norm + 0.5;
    double v_norm = 0.615 * r_norm - 0.515 * g_norm - 0.100 * b_norm + 0.5;
    
    // Clamp and convert to 16-bit
    if (y_norm < 0.0) y_norm = 0.0; else if (y_norm > 1.0) y_norm = 1.0;
    if (u_norm < 0.0) u_norm = 0.0; else if (u_norm > 1.0) u_norm = 1.0;
    if (v_norm < 0.0) v_norm = 0.0; else if (v_norm > 1.0) v_norm = 1.0;
    
    y = static_cast<uint16_t>(y_norm * 65535.0);
    u = static_cast<uint16_t>(u_norm * 65535.0);
    v = static_cast<uint16_t>(v_norm * 65535.0);
}

} // namespace encode_orc
