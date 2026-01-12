#include "cli_parser.hpp"
#include <iostream>
#include <cstring>

namespace encode_orc {

void print_version() {
    std::cout << "encode-orc version 0.1.0\n";
    std::cout << "Encoder for decode-orc (for making test TBC/Metadata files)\n";
}

void print_usage(const char* program_name) {
    std::cout << "Usage: " << program_name << " [OPTIONS]\n\n";
    std::cout << "Options:\n";
    std::cout << "  -o, --output FILE       Output filename (required)\n";
    std::cout << "  -f, --format FORMAT     Output format: pal-composite, ntsc-composite,\n";
    std::cout << "                          pal-yc, ntsc-yc (required)\n";
    std::cout << "  -i, --input FILE        Input RGB file (optional)\n";
    std::cout << "  -t, --testcard NAME     Generate test card: smpte, pm5544, testcard-f\n";
    std::cout << "  -n, --frames NUM        Number of frames to generate (default: 1)\n";
    std::cout << "  -v, --verbose           Enable verbose output\n";
    std::cout << "  -h, --help              Show this help message\n";
    std::cout << "  --version               Show version information\n";
    std::cout << "\n";
    std::cout << "Examples:\n";
    std::cout << "  " << program_name << " -o output.tbc -f pal-composite -t smpte -n 100\n";
    std::cout << "  " << program_name << " -o output.tbc -f ntsc-yc -i input.rgb\n";
}

CLIOptions parse_arguments(int argc, char* argv[]) {
    CLIOptions options;
    
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        
        if (arg == "-h" || arg == "--help") {
            options.show_help = true;
        } else if (arg == "--version") {
            options.show_version = true;
        } else if (arg == "-v" || arg == "--verbose") {
            options.verbose = true;
        } else if (arg == "-o" || arg == "--output") {
            if (i + 1 < argc) {
                options.output_filename = argv[++i];
            } else {
                std::cerr << "Error: " << arg << " requires an argument\n";
                options.show_help = true;
            }
        } else if (arg == "-f" || arg == "--format") {
            if (i + 1 < argc) {
                options.format = argv[++i];
            } else {
                std::cerr << "Error: " << arg << " requires an argument\n";
                options.show_help = true;
            }
        } else if (arg == "-i" || arg == "--input") {
            if (i + 1 < argc) {
                options.input_filename = argv[++i];
            } else {
                std::cerr << "Error: " << arg << " requires an argument\n";
                options.show_help = true;
            }
        } else if (arg == "-t" || arg == "--testcard") {
            if (i + 1 < argc) {
                options.testcard = argv[++i];
            } else {
                std::cerr << "Error: " << arg << " requires an argument\n";
                options.show_help = true;
            }
        } else if (arg == "-n" || arg == "--frames") {
            if (i + 1 < argc) {
                try {
                    options.num_frames = std::stoi(argv[++i]);
                    if (options.num_frames <= 0) {
                        std::cerr << "Error: number of frames must be positive\n";
                        options.show_help = true;
                    }
                } catch (const std::exception&) {
                    std::cerr << "Error: invalid number of frames\n";
                    options.show_help = true;
                }
            } else {
                std::cerr << "Error: " << arg << " requires an argument\n";
                options.show_help = true;
            }
        } else {
            std::cerr << "Error: unknown option '" << arg << "'\n";
            options.show_help = true;
        }
    }
    
    return options;
}

} // namespace encode_orc
