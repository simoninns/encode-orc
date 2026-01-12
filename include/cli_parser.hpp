#ifndef ENCODE_ORC_CLI_PARSER_HPP
#define ENCODE_ORC_CLI_PARSER_HPP

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

#endif // ENCODE_ORC_CLI_PARSER_HPP
