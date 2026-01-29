# Phase 4 Implementation Summary

## Overview

Phase 4 of the encode-orc architecture refactoring has been successfully completed. This phase focused on extracting metadata generators from the monolithic encoders and creating a composable pipeline configuration system.

## What Was Implemented

### 1. Pipeline Metadata Generator Infrastructure

**New Files Created:**
- `include/pipeline_metadata_generator.h` - Abstract base class for all pipeline generators
- `include/biphase_vbi_generator.h` / `src/biphase_vbi_generator.cpp` - LaserDisc VBI generator
- `include/vitc_pipeline_generator.h` / `src/vitc_pipeline_generator.cpp` - VITC timecode generator
- `include/vits_pipeline_generator.h` / `src/vits_pipeline_generator.cpp` - PAL/NTSC VITS generators
- `include/color_burst_pipeline_generator.h` / `src/color_burst_pipeline_generator.cpp` - Color burst generator

**Key Classes:**
- `PipelineMetadataGenerator` - Base class with virtual methods:
  - `apply()` - Apply generator to a structured field
  - `affected_lines()` - Return which lines this generator modifies
  - `name()` - Human-readable generator name
  - `is_applicable()` - Check if generator applies to current context

- `MetadataContext` - Context passed to generators:
  - Field number, frame number, video system
  - VBI data (for LaserDisc formats)
  - Source video standard

**Generator Implementations:**
1. **BiphaseVBIGenerator** - Encodes 24-bit LaserDisc VBI data on lines 16-18
2. **VITCPipelineGenerator** - Encodes VITC timecode for consumer tape
3. **PALVITSPipelineGenerator** - Generates PAL VITS signals (multiburst, UK national, ITU composite, ITU ITS)
4. **NTSCVITSPipelineGenerator** - Generates NTSC VITS signals (NTC7 composite, NTC7 combination)
5. **ColorBurstPipelineGenerator** - Adds color burst to all non-vsync lines

### 2. YAML Configuration Extensions

**Modified Files:**
- `include/yaml_config.h` - Added new configuration structures
- `src/yaml_config.cpp` - Added parser for pipeline configuration

**New Configuration Structures:**
```cpp
struct PipelineGeneratorConfig {
    std::string type;           // "biphase-vbi", "vitc", "vits-pal", "vits-ntsc", "color-burst"
    bool enabled;
    std::vector<int32_t> lines;  // For biphase-vbi, vitc
    int32_t start_frame_offset;  // For vitc
    std::vector<VITSSignal> vits_signals;  // For vits-pal/vits-ntsc
};

struct PipelineMetadataConfig {
    std::vector<PipelineGeneratorConfig> generators;
};

struct PipelineConfig {
    std::optional<PipelineMetadataConfig> metadata;
};
```

**YAML Format:**
```yaml
pipeline:
  metadata:
    generators:
      - type: "biphase-vbi"
        enabled: true
        lines: [15, 16, 17]
      - type: "vits-pal"
        enabled: true
        signals:
          - { line: 12, field: 1, signal: "multiburst" }
      - type: "color-burst"
        enabled: true
```

### 3. Build System Updates

**Modified Files:**
- `CMakeLists.txt` - Added new source files to build

### 4. Documentation

**New Documentation:**
- `docs/phase4-pipeline-configuration.md` - Complete guide to pipeline configuration
- `local-projects/example-phase4-pal-cav.yaml` - Example PAL LaserDisc configuration
- `local-projects/example-phase4-ntsc-vitc.yaml` - Example NTSC consumer tape configuration

## Key Design Decisions

### 1. Composable Generators
Each generator is independent and can be enabled/disabled via configuration. This allows for flexible combinations:
- LaserDisc CAV: `biphase-vbi` + `vits-pal` + `color-burst`
- Consumer Tape: `vitc` + `color-burst`
- Minimal: `color-burst` only

### 2. Explicit Line Configuration
All generators accept custom line placements, allowing non-standard configurations for testing or special formats.

### 3. Context-Based Applicability
Generators can check if they're applicable to the current context (e.g., PAL vs NTSC, LaserDisc vs tape) and skip processing when not needed.

### 4. Zero-Indexed Line Numbers
YAML configuration uses 0-indexed line numbers (matching internal representation) rather than 1-indexed (broadcast standard). This reduces conversion errors.

### 5. Backward Compatibility Preserved
The old `laserdisc.standard` configuration is still parsed (marked DEPRECATED) to support existing YAML files during transition. However, the new pipeline format is **not yet integrated into VideoEncoder**.

## Current Limitations

### 1. Not Yet Integrated with VideoEncoder
The generators exist and compile, but are **not yet called** by the main encoding pipeline. This requires Phase 8 work to:
- Create a pipeline builder that constructs generators from YAML config
- Replace encoder-embedded generator calls with pipeline application
- Remove old generator code from PALEncoder/NTSCEncoder

### 2. No Validation
The YAML parser accepts the new configuration but doesn't validate:
- Valid signal types for each generator
- Valid line numbers for the video system
- Conflicts between generators (e.g., two generators on same line)

### 3. Old Format Still Works
The old `laserdisc.standard` format is still the **only working format**. The new pipeline configuration is parsed but ignored by the encoder.

## Testing Status

✅ **Compiles successfully** - All new code compiles without errors
❌ **Not runtime tested** - Generators not yet integrated, so cannot test end-to-end
✅ **YAML parsing works** - Parser successfully reads new configuration format
❌ **No unit tests** - Phase 4 focused on infrastructure; tests to be added later

## Next Steps (For Future Work)

### Immediate: Phase 8 - VideoEncoder Integration
1. Create `PipelineBuilder` class to construct generator pipeline from YAML
2. Modify `VideoEncoder::encode_*_image()` methods to:
   - Build generator pipeline from config
   - Apply generators to structured fields
   - Remove embedded generator calls
3. Remove old generator creation in encoders
4. Add validation for pipeline configuration

### Medium-Term: Migration
1. Create migration tool to convert old YAML to new format
2. Migrate all test projects in `test-projects/`
3. Remove support for `laserdisc.standard` format
4. Update all documentation

### Long-Term: Phase 5+
1. Continue architecture refactoring (Phase 5: Active Video Encoding)
2. Add field effects stage (noise, dropouts) in Phase 6
3. Complete pipeline-based architecture

## Files Modified/Created Summary

**New Headers (8 files):**
- `include/pipeline_metadata_generator.h`
- `include/biphase_vbi_generator.h`
- `include/vitc_pipeline_generator.h`
- `include/vits_pipeline_generator.h`
- `include/color_burst_pipeline_generator.h`

**New Source Files (4 files):**
- `src/biphase_vbi_generator.cpp`
- `src/vitc_pipeline_generator.cpp`
- `src/vits_pipeline_generator.cpp`
- `src/color_burst_pipeline_generator.cpp`

**Modified Files (3 files):**
- `include/yaml_config.h` - Added pipeline configuration structures
- `src/yaml_config.cpp` - Added pipeline configuration parser
- `CMakeLists.txt` - Added new source files

**Documentation (3 files):**
- `docs/phase4-pipeline-configuration.md`
- `local-projects/example-phase4-pal-cav.yaml`
- `local-projects/example-phase4-ntsc-vitc.yaml`

**Total: 18 files created/modified**

## Success Criteria

✅ **Infrastructure Complete** - All generator classes implemented
✅ **YAML Support** - Configuration parsing works
✅ **Compiles** - No build errors
✅ **Documented** - Examples and guide written
❌ **Integrated** - Not yet functional in encoder (awaits Phase 8)
❌ **Tested** - No runtime tests yet

## Conclusion

Phase 4 successfully created the **infrastructure** for a composable metadata generator pipeline. While not yet integrated into the main encoder, all the building blocks are in place:

- Clean abstraction via `PipelineMetadataGenerator`
- Five concrete generator implementations
- YAML configuration support
- Comprehensive documentation

The next major step is **Phase 8: VideoEncoder Integration**, which will make this infrastructure functional by replacing the current monolithic encoding with the new pipeline-based approach.

---

**Implementation Date**: January 29, 2026
**Status**: Infrastructure Complete, Integration Pending
**Estimated Integration Effort**: Medium (Phase 8)
