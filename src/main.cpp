#include "cli_parser.hpp"
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
    }
    
    // TODO: Implement actual encoding logic
    std::cout << "encode-orc is not yet fully implemented.\n";
    std::cout << "This is a placeholder for Phase 1.1 (Project Setup).\n";
    std::cout << "Next milestone (M0): Implement blanking-level output validation.\n";
    
    return 0;
}
