/*
 * File:        main.cpp
 * Module:      encode-orc
 * Purpose:     Main application entry point
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: 2026 Simon Inns
 */

#include "cli_parser.h"
#include "blanking_encoder.h"
#include "video_encoder.h"
#include "test_card_generator.h"
#include "video_parameters.h"
#include <iostream>
#include <sqlite3.h>

int main(int argc, char* argv[]) {
    using namespace encode_orc;
    
    // Parse command-line arguments
    CLIOptions options = parse_arguments(argc, argv);
    
    // Handle version flag
    if (options.show_version) {
        print_version();
        return 0;
    }
    
    // Handle help flag or missing required arguments
    if (options.show_help || options.output_filename.empty() || options.format.empty()) {
        print_usage(argv[0]);
        return options.show_help ? 0 : 1;
    }
    
    // Validate format
    if (options.format != "pal-composite" && 
        options.format != "ntsc-composite" && 
        options.format != "pal-yc" && 
        options.format != "ntsc-yc") {
        std::cerr << "Error: Invalid format '" << options.format << "'\n";
        std::cerr << "Valid formats: pal-composite, ntsc-composite, pal-yc, ntsc-yc\n";
        return 1;
    }
    
    if (options.verbose) {
        std::cout << "encode-orc starting...\n";
        std::cout << "Output: " << options.output_filename << "\n";
        std::cout << "Format: " << options.format << "\n";
        std::cout << "Frames: " << options.num_frames << "\n";
        if (options.input_filename) {
            std::cout << "Input: " << *options.input_filename << "\n";
        }
        if (options.testcard) {
            std::cout << "Test card: " << *options.testcard << "\n";
        }
        std::cout << "SQLite version: " << sqlite3_libversion() << "\n";
        std::cout << "\n";
    }
    
    // Determine video system from format
    encode_orc::VideoSystem system;
    if (options.format == "pal-composite" || options.format == "pal-yc") {
        system = encode_orc::VideoSystem::PAL;
    } else if (options.format == "ntsc-composite" || options.format == "ntsc-yc") {
        system = encode_orc::VideoSystem::NTSC;
    } else {
        std::cerr << "Error: Unsupported format\n";
        return 1;
    }
    
    // Check if test card is specified
    if (options.testcard) {
        // Map test card name to type
        encode_orc::TestCardGenerator::Type test_card_type;
        if (*options.testcard == "color-bars" || *options.testcard == "ebu" || *options.testcard == "eia" || *options.testcard == "smpte") {
            // Support multiple names for color bars: color-bars, ebu, eia, smpte (deprecated)
            test_card_type = encode_orc::TestCardGenerator::Type::COLOR_BARS;
        } else if (*options.testcard == "pm5544") {
            test_card_type = encode_orc::TestCardGenerator::Type::PM5544;
        } else if (*options.testcard == "testcard-f") {
            test_card_type = encode_orc::TestCardGenerator::Type::TESTCARD_F;
        } else {
            std::cerr << "Error: Unknown test card type: " << *options.testcard << "\n";
            return 1;
        }
        
        // Use video encoder for test card generation
        encode_orc::VideoEncoder encoder;
        if (!encoder.encode_test_card(options.output_filename, system, 
                                     test_card_type, options.num_frames, 
                                     options.verbose)) {
            std::cerr << "Error: " << encoder.get_error() << "\n";
            return 1;
        }
    } else {
        // Use blanking encoder for blanking-level output
        encode_orc::BlankingEncoder encoder;
        if (!encoder.encode(options.output_filename, system, options.num_frames, options.verbose)) {
            std::cerr << "Error: " << encoder.get_error() << "\n";
            return 1;
        }
        
        if (!options.verbose) {
            std::cout << "Successfully generated " << options.num_frames << " frames of blanking-level video\n";
        }
    }
    
    return 0;
}
