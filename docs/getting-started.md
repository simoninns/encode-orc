---
title: Getting Started
layout: default
nav_order: 2
---

# Getting Started with encode-orc

This guide will help you install and run encode-orc for the first time.

## Installation

### Prerequisites

- Linux or macOS system
- GCC/Clang C++17 compatible compiler
- CMake 3.10 or later
- YAML library (libyaml-dev)

### Building from Source

1. Clone the repository:
   ```bash
   git clone https://github.com/simoninns/encode-orc.git
   cd encode-orc
   ```

2. Create a build directory and compile:
   ```bash
   mkdir build
   cd build
   cmake ..
   make
   ```

3. Run the tests to verify installation:
   ```bash
   ../run-tests.sh
   ```

The executable will be available at `build/encode-orc`.

## Your First Project

### 1. Create a Simple YAML Configuration

Create a file named `first-project.yaml`:

```yaml
project:
  name: "My First Test Video"
  author: "Your Name"
  
video:
  format: "PAL"
  frame_count: 100

output:
  base_path: "./output"
  format: "composite"

sections:
  - name: "Test Section"
    input:
      type: "png"
      file: "testcard.png"
```

### 2. Run encode-orc

```bash
./build/encode-orc first-project.yaml
```

### 3. Check the Output

The generated TBC files will be in the `./output` directory:
- `output.tbc` - Composite video file
- `output.tbc.db` - Metadata database

## Next Steps

- Review the [User Guide](user-guide/) for detailed examples
- Explore the [User Guide](user-guide/)
- Check the `example-projects/` directory for sample configurations

## Troubleshooting

### Build Fails
- Ensure CMake version is 3.10 or later: `cmake --version`
- Install required dependencies: `sudo apt-get install libyaml-dev` (Ubuntu/Debian)

### Tests Fail
- Check that all dependencies are properly installed
- Run `cmake --version` to verify CMake is available
- Try building in a clean directory: `rm -rf build && mkdir build && cd build && cmake .. && make`

---

Need help? Check the full [User Guide](user-guide/) or visit the [repository](https://github.com/simoninns/encode-orc).
