# encode-orc

![](docs/assets/encode-orc_logotype.png)

Encode-Orc is a PAL and NTSC encoder capable of producing ld-decode and vhs-decode compatible TBC files (and SQLite metadata) to assist with testing.

Please see the [Encode-Orc Documentation](https://simoninns.github.io/encode-orc) for details.

## Overview

encode-orc is a C++17 command-line application that generates test data for [decode-orc](https://github.com/simoninns/decode-orc). It produces field-based video files in the TBC (Time Base Corrected) format along with accompanying metadata, simulating the output of RF decoding applications such as ld-decode and vhs-decode used in the LaserDisc and analog video preservation community.

The application uses YAML project files to define encoding configurations, supporting multiple input formats and output modes with both video-embedded and SQLite-based metadata generation.

## Purpose

This tool enables developers and testers to create synthetic test data for validating video decoding pipelines without requiring physical hardware or RF captures. It's particularly useful for:

- Testing decode-orc and ld-decode tool chains
- Validating color decoder implementations
- Creating reproducible test cases
- Benchmarking decoder performance
- Educational purposes and algorithm development

## Project Status

First release

## Building

This project uses Nix for reproducible builds and development environments.

### Prerequisites

- [Nix](https://nixos.org/download.html) with flakes enabled
- (Optional) [direnv](https://direnv.net/) for automatic environment loading

### Build Instructions

```bash
# Build the project
nix build

# Run the executable
./result/bin/encode-orc --help
```

### Development Environment

```bash
# Enter development shell with all dependencies
nix develop

# Or use direnv for automatic environment loading
direnv allow
```

### Running Tests

```bash
# Build first
nix build

# Run flake checks (includes build + tests)
nix flake check

```

The test suite will automatically detect and use the Nix-built executable from `result/bin/encode-orc`.

### Formatting

```bash
# Format Nix files
nix fmt
```

Note that encode-orc is designed to be a sub-module to the decode-orc project.  Therefore local build is limited to a simple test of the compilation in Fedora as well as a number of local test encode runs to check functionality.

## Contributing

Contributions are welcome! Please discuss in the Domesday86 discord, raise issues or submit PRs.

## License

See [LICENSE](LICENSE) file for details.

## Related Projects

- [decode-orc](https://github.com/simoninns/decode-orc) - Encode-Orc is designed as a sub-module to decode-orc.

## Acknowledgments

This project builds upon the work of the ld-decode community and the video preservation community at large.

encode-orc's encoding is based on the ld-chroma-encoder component from the ld-decode project:
https://github.com/happycube/ld-decode/tree/main/tools/ld-chroma-decoder