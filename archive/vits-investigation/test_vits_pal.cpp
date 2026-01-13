/*
 * File:        test_vits_pal.cpp
 * Module:      encode-orc
 * Purpose:     Test PAL VITS signal generation
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: 2026 Simon Inns
 */

#include "pal_encoder.h"
#include "frame_buffer.h"
#include "tbc_writer.h"
#include "metadata_writer.h"
#include "color_conversion.h"
#include <iostream>
#include <cstring>
#include <cstdio>

using namespace encode_orc;

/**
 * @brief Generate SMPTE color bars test pattern
 */
FrameBuffer generate_color_bars(int32_t width, int32_t height) {
    FrameBuffer frame(width, height, FrameBuffer::Format::RGB48);
    
    // SMPTE color bars (75% intensity)
    constexpr uint16_t LEVEL_75 = 0xBFFF;  // 75% of full scale
    constexpr uint16_t LEVEL_0 = 0x0000;   // 0%
    
    struct RGB {
        uint16_t r, g, b;
    };
    
    const RGB bars[] = {
        {LEVEL_75, LEVEL_75, LEVEL_75},  // White (75%)
        {LEVEL_75, LEVEL_75, LEVEL_0},   // Yellow (75%)
        {LEVEL_0,  LEVEL_75, LEVEL_75},  // Cyan (75%)
        {LEVEL_0,  LEVEL_75, LEVEL_0},   // Green (75%)
        {LEVEL_75, LEVEL_0,  LEVEL_75},  // Magenta (75%)
        {LEVEL_75, LEVEL_0,  LEVEL_0},   // Red (75%)
        {LEVEL_0,  LEVEL_0,  LEVEL_75},  // Blue (75%)
        {LEVEL_0,  LEVEL_0,  LEVEL_0}    // Black
    };
    
    int32_t bar_width = width / 8;
    
    for (int32_t y = 0; y < height; ++y) {
        for (int32_t x = 0; x < width; ++x) {
            int32_t bar_index = x / bar_width;
            if (bar_index > 7) bar_index = 7;
            
            frame.set_rgb_pixel(x, y, bars[bar_index].r,
                              bars[bar_index].g, bars[bar_index].b);
        }
    }
    
    return frame;
}

int main() {
    std::cout << "PAL VITS Test Signal Generator\n";
    std::cout << "===============================\n\n";
    
    // Create PAL video parameters
    VideoParameters params = VideoParameters::create_pal_composite();
    
    // Create frame buffer for 720×576 PAL
    constexpr int32_t FRAME_WIDTH = 720;
    constexpr int32_t FRAME_HEIGHT = 576;
    
    // Generate color bars test pattern (RGB)
    std::cout << "Generating SMPTE color bars pattern...\n";
    FrameBuffer rgb_frame = generate_color_bars(FRAME_WIDTH, FRAME_HEIGHT);
    
    // Convert to YUV
    std::cout << "Converting to YUV...\n";
    FrameBuffer yuv_frame = ColorConverter::rgb_to_yuv_pal(rgb_frame);
    
    // Create PAL encoder with VITS enabled
    PALEncoder encoder(params);
    encoder.enableVITS(VITSStandard::IEC60856_PAL);
    
    std::cout << "VITS enabled: " << (encoder.isVITSEnabled() ? "YES" : "NO") << "\n";
    std::cout << "VITS standard: IEC 60856-1986 PAL LaserDisc\n\n";
    
    // Output file path
    const std::string output_path = "test-output/test-vits-pal-colorbars.tbc";
    
    // Create TBC writer
    TBCWriter tbc_writer;
    if (!tbc_writer.open(output_path)) {
        std::cerr << "Error: Failed to open TBC file\n";
        return 1;
    }
    
    // Generate 50 frames (2 seconds at 25 fps)
    constexpr int32_t NUM_FRAMES = 50;
    constexpr int32_t NUM_FIELDS = NUM_FRAMES * 2;
    params.number_of_sequential_fields = NUM_FIELDS;
    
    std::cout << "Generating " << NUM_FRAMES << " frames with VITS...\n";
    
    for (int32_t frame_num = 0; frame_num < NUM_FRAMES; ++frame_num) {
        if (frame_num % 10 == 0) {
            std::cout << "  Frame " << frame_num << "/" << NUM_FRAMES << "\n";
        }
        
        // Encode frame (with VITS on VBI lines)
        Frame frame = encoder.encode_frame(yuv_frame, frame_num * 2);
        
        // Write fields to TBC file
        tbc_writer.write_field(frame.field1());
        tbc_writer.write_field(frame.field2());
    }
    
    tbc_writer.close();
    
    // Write metadata
    std::string metadata_filename = output_path + ".db";
    std::cout << "\nWriting metadata to " << metadata_filename << "...\n";
    
    // Remove existing database file if it exists
    std::remove(metadata_filename.c_str());
    
    CaptureMetadata metadata;
    metadata.initialize(VideoSystem::PAL, NUM_FIELDS);
    metadata.git_branch = "main";
    metadata.git_commit = "v0.1.0-dev-vits-test";
    metadata.capture_notes = "PAL VITS test - IEC 60856-1986 LaserDisc compliance";
    
    MetadataWriter metadata_writer;
    if (!metadata_writer.open(metadata_filename)) {
        std::cerr << "Error: Failed to open metadata database\n";
        return 1;
    }
    
    if (!metadata_writer.write_metadata(metadata)) {
        std::cerr << "Error: Failed to write metadata: " << metadata_writer.get_error() << "\n";
        return 1;
    }
    
    metadata_writer.close();
    
    std::cout << "\nTest completed successfully!\n";
    std::cout << "Output files:\n";
    std::cout << "  " << output_path << "\n";
    std::cout << "  " << output_path << ".db\n";
    std::cout << "\nVITS signals inserted per IEC 60856-1986:\n";
    std::cout << "  Line  19: Luminance transient & amplitude (B2, B1, F, D1)\n";
    std::cout << "  Line  20: Frequency response multiburst (C1, C2, C3)\n";
    std::cout << "  Line 332: Differential gain & phase (B2, B1, D2)\n";
    std::cout << "  Line 333: Chrominance amplitude & linearity (G1, E)\n";
    
    return 0;
}
