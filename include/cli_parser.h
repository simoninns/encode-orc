/*
 * File:        cli_parser.h
 * Module:      encode-orc
 * Purpose:     Command-line argument parser interface
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: 2026 Simon Inns
 */

#ifndef ENCODE_ORC_CLI_PARSER_H
#define ENCODE_ORC_CLI_PARSER_H

#include <string>
#include <optional>

namespace encode_orc {

/**
 * @brief Command-line options for encode-orc
 */
struct CLIOptions {
    std::string output_filename;
    std::string format;  // "pal-composite", "ntsc-composite", "pal-yc", "ntsc-yc"
    std::optional<std::string> input_filename;
    std::optional<std::string> testcard;
    std::string vits_standard = "none";  // "none", "iec60856-pal", "itu-j63-pal", etc.
    int num_frames = 1;
    bool verbose = false;
    bool show_help = false;
    bool show_version = false;
};

/**
 * @brief Parse command-line arguments
 * 
 * @param argc Argument count
 * @param argv Argument vector
 * @return Parsed options
 */
CLIOptions parse_arguments(int argc, char* argv[]);

/**
 * @brief Print usage information
 */
void print_usage(const char* program_name);

/**
 * @brief Print version information
 */
void print_version();

} // namespace encode_orc

#endif // ENCODE_ORC_CLI_PARSER_H
