# Video Parameters Override Implementation - Summary

## Changes Made

### 1. Core Implementation

**Files Modified:**
- [include/yaml_config.h](include/yaml_config.h) - Added `VideoLevelsConfig` struct and optional `video_levels` field to `OutputConfig`
- [src/yaml_config.cpp](src/yaml_config.cpp) - Added parsing logic for `video_levels` YAML section
- [include/video_parameters.h](include/video_parameters.h) - Added `apply_video_level_overrides()` static helper method
- [include/video_encoder.h](include/video_encoder.h) - Added static methods and members for video level override management
- [src/video_encoder.cpp](src/video_encoder.cpp) - Implemented override setters and applied overrides in both encode methods
- [src/main.cpp](src/main.cpp) - Added code to set video level overrides from YAML config before encoding

### 2. Documentation

**Files Updated:**
- [docs/yaml-project-format.md](docs/yaml-project-format.md) - Added comprehensive documentation for:
  - `video_levels` configuration options in the output section
  - Default video levels for PAL and NTSC formats
  - Use cases for custom video levels
  - Partial override examples
  - Integration with YAML project structure

### 3. Test Project

**Files Created:**
- [test-projects/ntsc-j-yuv422-eia-75.yaml](test-projects/ntsc-j-yuv422-eia-75.yaml) - New NTSC-J test project with custom black level (18196)

## How It Works

### YAML Configuration

Users can now override video levels in their YAML project files:

```yaml
output:
  filename: "output.tbc"
  format: "ntsc-composite"
  
  video_levels:
    blanking_16b_ire: 15058    # Optional
    black_16b_ire: 18196       # Optional
    white_16b_ire: 51200       # Optional
```

### Implementation Details

1. **YAML Parsing**: The `parse_yaml_config()` function reads the optional `video_levels` section
2. **Static Override Storage**: `VideoEncoder` maintains three static members to hold the overrides
3. **Override Application**: When `encode_yuv422_image()` or `encode_png_image()` is called, the overrides are applied to the VideoParameters after system defaults are loaded
4. **Method Chain**: 
   - YAML config is parsed
   - `VideoEncoder::set_video_level_overrides()` is called in main.cpp
   - Each encoder call applies the stored overrides to its local VideoParameters
   - `VideoParameters::apply_video_level_overrides()` updates the parameters in place

## Default Values

The following are the corrected default values in code:

**PAL Composite:**
- blanking_16b_ire: 0x42E5 (17125)
- black_16b_ire: 0x42E5 (17125)
- white_16b_ire: 0xD300 (54016)

**NTSC Composite:**
- blanking_16b_ire: 0x3AD2 (15058)
- black_16b_ire: 0x4568 (17768)
- white_16b_ire: 0xC800 (51200)

## Building

The implementation has been verified to compile successfully:

```bash
cd /home/sdi/Coding/github/encode-orc/build
make -j4
# Result: [100%] Built target encode-orc
```

## Usage Example

To create a project with NTSC-J black level setup:

```yaml
name: "NTSC-J Test"
description: "NTSC with Japanese black level setup"

output:
  filename: "output.tbc"
  format: "ntsc-composite"
  video_levels:
    blanking_16b_ire: 15058
    black_16b_ire: 18196      # NTSC-J specific value
    white_16b_ire: 51200

# ... sections ...
```

Then run:
```bash
./encode-orc ntsc-j-test.yaml
```

The encoder will automatically apply the custom black level while using standard defaults for blanking and white.
