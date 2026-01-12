/*
 * File:        test_pal_encoder.cpp
 * Module:      encode-orc
 * Purpose:     Test PAL encoder functionality
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: 2026 Simon Inns
 */

#include "pal_encoder.h"
#include "color_conversion.h"
#include "video_parameters.h"
#include "tbc_writer.h"
#include "metadata_writer.h"
#include <iostream>
#include <cmath>
#include <cstdio>

using namespace encode_orc;

/**
 * @brief Generate a simple test pattern (color bars)
 */
FrameBuffer generate_color_bars(int32_t width, int32_t height) {
    FrameBuffer frame(width, height, FrameBuffer::Format::RGB48);
    
    // Create 8 vertical color bars - PAL 75% amplitude (EBU bars)
    // These match the pattern used by ld-chroma-encoder's test suite
    // RGB values are at 75% saturation to avoid overmodulation
    struct ColorBar {
        uint16_t r, g, b;
    };
    
    // 75% amplitude bars (0xC000 = 75% of 0xFFFF)
    const uint16_t LEVEL_75 = 0xC000;  // 75% white
    const uint16_t LEVEL_0 = 0x0000;   // Black
    
    const ColorBar bars[] = {
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
    std::cout << "Testing PAL encoder...\n\n";
    
    // Create PAL video parameters
    VideoParameters params = VideoParameters::create_pal_composite();
    
    std::cout << "PAL Parameters:\n";
    std::cout << "  Subcarrier frequency: " << params.fSC << " Hz\n";
    std::cout << "  Sample rate: " << params.sample_rate << " Hz\n";
    std::cout << "  Field dimensions: " << params.field_width << "x" << params.field_height << "\n";
    std::cout << "  Active video: " << params.active_video_start << "-" << params.active_video_end << "\n\n";
    
    // Create PAL encoder
    PALEncoder encoder(params);
    
    // Generate test pattern (720x576 for PAL)
    int32_t width = 720;
    int32_t height = 576;
    std::cout << "Generating " << width << "x" << height << " color bars test pattern...\n";
    FrameBuffer rgb_frame = generate_color_bars(width, height);
    
    // Convert to YUV
    std::cout << "Converting RGB to YUV...\n";
    FrameBuffer yuv_frame = ColorConverter::rgb_to_yuv_pal(rgb_frame);
    
    // Encode one frame (2 fields)
    std::cout << "Encoding PAL composite fields...\n";
    Frame encoded_frame = encoder.encode_frame(yuv_frame, 0);
    
    std::cout << "Field 1: " << encoded_frame.field1().width() << "x" << encoded_frame.field1().height() << "\n";
    std::cout << "Field 2: " << encoded_frame.field2().width() << "x" << encoded_frame.field2().height() << "\n";
    
    // Verify that fields are not blank
    uint16_t first_sample_f1 = encoded_frame.field1().get_sample(0, 0);
    uint16_t first_sample_f2 = encoded_frame.field2().get_sample(0, 0);
    
    std::cout << "\nFirst samples:\n";
    std::cout << "  Field 1, sample (0,0): 0x" << std::hex << first_sample_f1 << std::dec << "\n";
    std::cout << "  Field 2, sample (0,0): 0x" << std::hex << first_sample_f2 << std::dec << "\n";
    
    // Write output file for testing
    std::string output_filename = "test-output/test-pal-colorbars.tbc";
    std::cout << "\nWriting test output to " << output_filename << "...\n";
    
    TBCWriter tbc_writer;
    if (!tbc_writer.open(output_filename)) {
        std::cerr << "Error: Failed to open TBC file\n";
        return 1;
    }
    
    // Write 50 frames (2 seconds at 25 fps)
    int32_t num_frames = 50;
    int32_t num_fields = num_frames * 2;
    params.number_of_sequential_fields = num_fields;
    
    for (int32_t i = 0; i < num_fields; i += 2) {
        Frame frame = encoder.encode_frame(yuv_frame, i);
        tbc_writer.write_field(frame.field1());
        tbc_writer.write_field(frame.field2());
        
        if ((i / 2) % 10 == 0) {
            std::cout << "  Frame " << (i / 2) << "/" << num_frames << "\r" << std::flush;
        }
    }
    std::cout << "  Frame " << num_frames << "/" << num_frames << "\n";
    
    tbc_writer.close();
    
    // Write metadata
    std::string metadata_filename = output_filename + ".db";
    std::cout << "Writing metadata to " << metadata_filename << "...\n";
    
    // Remove existing database file if it exists
    std::remove(metadata_filename.c_str());
    
    CaptureMetadata metadata;
    metadata.initialize(VideoSystem::PAL, num_fields);
    metadata.git_branch = "main";
    metadata.git_commit = "v0.1.0-dev-pal-encoder-test";
    metadata.capture_notes = "PAL encoder test - color bars pattern";
    
    MetadataWriter metadata_writer;
    if (!metadata_writer.open(metadata_filename)) {
        std::cerr << "Error: Failed to open metadata database\n";
        return 1;
    }
    
    if (!metadata_writer.write_metadata(metadata)) {
        std::cerr << "Error: Failed to write metadata\n";
        std::cerr << "Details: " << metadata_writer.get_error() << "\n";
        return 1;
    }
    
    metadata_writer.close();
    
    std::cout << "\nPAL encoder test completed successfully!\n";
    std::cout << "Output files:\n";
    std::cout << "  " << output_filename << "\n";
    std::cout << "  " << metadata_filename << "\n";
    
    return 0;
}
