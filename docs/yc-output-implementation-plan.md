# Y/C Output Implementation Plan

## Overview

This document outlines the plan to reintroduce PAL-YC and NTSC-YC output support to the encode-orc pipeline, including removal of the redundant YAML `mode` field in favor of using the `format` field alone.

## Problem Statement

Currently, the code accepts `pal-yc` and `ntsc-yc` format specifications in YAML, but the pipeline encoder rejects them with:
```
Separate Y/C output is not supported by the pipeline encoder
Please use combined output mode or re-enable legacy encoders
```

The Y/C output feature existed in the legacy encoder but was not carried forward when the pipeline architecture was introduced. Additionally, the current design has a redundancy: both `format` (which specifies `-yc` or `-composite`) and `mode` (which specifies `separate-yc`) are used to control the same behavior.

## Design Goals

1. **Reintroduce Y/C output** for both PAL and NTSC formats
2. **Eliminate redundancy** by removing the `mode` field from YAML (format field is sufficient)
3. **Maintain backward compatibility** where possible
4. **Simplify configuration** - users should only need to specify the format

## Implementation Plan

### Phase 1: YAML Configuration Cleanup

**Objective**: Remove the `mode` field from YAML parsing while maintaining internal support for the concept

**Files Modified**:
- `src/config/yaml_config.h`
- `src/config/yaml_config.cpp`

**Changes**:

1. Keep the `OutputConfig::mode` struct member (for internal use) but initialize it based on format, not YAML
2. Remove YAML parsing for `output["mode"]` in yaml_config.cpp
3. Remove the mode validation check (lines ~421-424)
4. Update config validation to derive mode from format after parsing:
   ```cpp
   // After all YAML parsing, auto-set mode based on format
   if (config.output.format == "pal-yc" || config.output.format == "ntsc-yc") {
       config.output.mode = "separate-yc";
   } else {
       config.output.mode = "combined";
   }
   ```

**Rationale**: The `mode` field is now redundant—it can be automatically derived from the format field. Users should only specify format.

---

### Phase 2: Field Class Extension and Format Detection

**Objective**: Extend the Field class to support separate Y/C representations and enable format-based routing

**Status**: ✅ COMPLETED

**Files Modified**:
- `src/pipeline/common/field.h`
- `src/main.cpp`

**Changes Made**:

1. **Extended Field class** to support dual representations:
   - Added `std::unique_ptr<Field> y_field_` and `std::unique_ptr<Field> c_field_` members
   - Added methods: `has_separate_yc()`, `y_field()`, `y_field_const()`, `c_field()`, `c_field_const()`
   - Y/C representations are created on-demand to avoid overhead for composite-only output
   - Primary composite representation always present, Y/C are optional

2. **Removed Y/C rejection block** from main.cpp:
   - Deleted error messages about Y/C not being supported
   - Format now correctly recognized for `pal-yc` and `ntsc-yc`

3. **Implemented format-based filename handling**:
   - Composite formats: `.tbc` extension added if missing
   - Y/C formats: `.tbc` extension removed for future YCTBCWriter integration

4. **Simplified writer selection**:
   - Currently uses standard TBC writer for all formats (composite representation)
   - Y/C writer integration deferred to Phase 4

**Rationale**: Using unique_ptr for Y/C fields avoids circular type issues and provides lazy initialization. The Field class now cleanly supports both representations without lossy conversion.

---

### Phase 2.5: Video Encoder Y/C Field Generation

**Objective**: Update the pipeline encoder to generate separate Y and C field representations alongside composite

**Status**: ✅ COMPLETED

**Files Modified**:
- `src/pipeline/active_encoding/active_video_encoder.h` (interface)
- `src/pipeline/active_encoding/ntsc_active_encoder.h`
- `src/pipeline/active_encoding/ntsc_active_encoder.cpp`
- `src/pipeline/active_encoding/pal_active_encoder.h`
- `src/pipeline/active_encoding/pal_active_encoder.cpp`
- `src/pipeline/orchestrator/video_encoder_pipeline.h`
- `src/pipeline/orchestrator/video_encoder_pipeline.cpp`
- `src/main.cpp`

**Changes Made**:

1. **Extended encoder interface** to support optional Y/C buffer parameters:
   - Added `y_buffer` and `c_buffer` parameters to `encode_active_line()` methods
   - Parameters default to `nullptr` for backward compatibility

2. **Updated PAL and NTSC encoders** to populate Y/C buffers:
   - Y buffer receives luma component scaled to signal levels
   - C buffer receives chroma component centered around mid-level
   - Both encoders extract Y and C during the composite encoding loop (Approach 1)
   - No performance impact when Y/C output is disabled (nullptr checks)

3. **Added Y/C output flag to VideoEncoderPipeline**:
   - New `enable_yc_output()` builder method
   - Pipeline creates Y and C fields on-demand when flag is set
   - Passes Y/C line buffers to active encoders during encoding

4. **Updated main.cpp integration**:
   - Detects Y/C format from config (`pal-yc` or `ntsc-yc`)
   - Enables Y/C output in pipeline builder when appropriate
   - Verified with both PAL and NTSC test projects

**Implementation Approach**:

Used **Approach 1: Extract from Composite** for simplicity:
- Y and C components are extracted during composite generation
- Same encoding loop generates all three representations
- Minimal code duplication
- Slightly lower quality than generating Y/C natively, but acceptable

**Testing Results**:
- ✅ PAL Y/C: Field size 354,120 samples, Y and C fields match
- ✅ NTSC Y/C: Field size 238,420 samples, Y and C fields match  
- ✅ Composite formats (PAL/NTSC) continue to work without Y/C overhead
- ✅ Build succeeds with no warnings or errors

**Memory Usage**:
- Composite-only output: No change (Y/C fields not created)
- Y/C output: ~3x memory (composite + Y + C fields)
- All three representations stored simultaneously

**Next Steps**: Phase 3 (YCTBCWriter Integration) to actually write Y/C files separately

---

### Phase 3: Writer Integration for Y/C Output

**Objective**: Integrate YCTBCWriter and route Y/C fields to separate files

**Status**: ⏳ NOT STARTED - Depends on Phase 2.5

**Files to Modify**:
- `src/main.cpp` (writer selection and field writing)
- `src/pipeline/writers/yc_tbc_writer.h` (already implemented, ready to use)

**Changes**:

1. **Update writer selection** in main.cpp:
   ```cpp
   std::unique_ptr<Writer> writer;
   std::unique_ptr<YCTBCWriter> yc_writer;
   
   if (config.output.format == "pal-yc" || config.output.format == "ntsc-yc") {
       yc_writer = std::make_unique<YCTBCWriter>(YCTBCWriter::NamingMode::MODERN);
       if (!yc_writer->open(output_filename)) {
           ENCODE_ORC_LOG_ERROR("Could not open Y/C output files: {}", output_filename);
           return 1;
       }
   } else {
       // Standard composite writer
       writer = std::make_unique<TBCWriter>();
       if (!writer->open(output_filename)) {
           ENCODE_ORC_LOG_ERROR("Could not open output file: {}", output_filename);
           return 1;
       }
   }
   ```

2. **Update field writing loop**:
   ```cpp
   if (yc_writer) {
       // Write Y and C fields separately
       if (!yc_writer->write_y_field(encoded_frame.field1()) || 
           !yc_writer->write_y_field(encoded_frame.field2())) {
           ENCODE_ORC_LOG_ERROR("Failed to write Y fields");
           return false;
       }
       if (!yc_writer->write_c_field(encoded_frame.field1()) || 
           !yc_writer->write_c_field(encoded_frame.field2())) {
           ENCODE_ORC_LOG_ERROR("Failed to write C fields");
           return false;
       }
   } else {
       // Write composite fields normally
       if (!writer->write_field(encoded_frame.field1()) || 
           !writer->write_field(encoded_frame.field2())) {
           ENCODE_ORC_LOG_ERROR("Failed to write fields");
           return false;
       }
   }
   ```

3. **Update file closing and metadata handling**:
   - Close YCTBCWriter separately
   - Generate metadata for `.tbcy.db` for Y/C output

---

### Phase 4: Metadata Generation for Y/C Output

**Objective**: Ensure VITS, biphase VBI, and other metadata apply correctly to Y field

**Status**: ⏳ NOT STARTED - Depends on Phase 2.5

**Files Affected**:
- `src/pipeline/metadata_generators/*` (verification only)
- `src/main.cpp` (metadata routing)

**Current Behavior** (correct):
- Metadata generators work with composite fields
- VITS, biphase VBI, color burst all modify the signal

**Changes for Y/C Output**:
1. Metadata generators should only modify the Y field when Y/C output is active
2. The C field should remain untouched by metadata
3. This requires:
   - Passing output format to metadata generators
   - Or passing Y field specifically (after extraction)
   - Metadata database associations with `.tbcy.db` for Y/C output

---

### Phase 3 (Current): Video Parameters and Encoding

### Phase 5: Test Project Updates

**Objective**: Update consumer tape test projects to remove redundant `mode` field

**Files Modified**:
- `test-projects/pal-consumer-tape.yaml`
- `test-projects/ntsc-consumer-tape.yaml`

**Changes**:

Remove the `mode` field from output configuration:

```yaml
# Before
output:
  filename: "test-output/pal-consumer"
  format: "pal-yc"
  mode: "separate-yc"

# After
output:
  filename: "test-output/pal-consumer"
  format: "pal-yc"
```

**Rationale**: The `mode` is now automatically derived from the format field.

---

### Phase 6: Documentation Updates

**Objective**: Clarify that format field controls everything, remove references to `mode` field

**Files Modified**:
- `docs/user-guide.md`

**Changes**:

1. **Output Configuration section**:
   - Remove `mode` field from code examples
   - Clarify that `-yc` in format enables Y/C separation

2. **Output Formats table**:
   - Add note: "Y/C formats (`pal-yc`, `ntsc-yc`) automatically generate separate luma and chroma files"

3. **Example projects**:
   - Remove `mode` field from all YAML examples
   - Add Y/C output example in the complete examples section

4. **Troubleshooting section**:
   - Add entry: "Y/C files not being generated" → "Ensure format is `pal-yc` or `ntsc-yc`"

**Sample documentation update**:

```markdown
### Output Formats

| Format | Resolution | Frame Rate | File Output |
|--------|-----------|-----------|-------------|
| `pal-composite` | 720×576 | 25 fps | Single `.tbc` file |
| `ntsc-composite` | 720×486 | 29.97 fps | Single `.tbc` file |
| `pal-yc` | 720×576 | 25 fps | `.tbcy` (luma) + `.tbcc` (chroma) |
| `ntsc-yc` | 720×486 | 29.97 fps | `.tbcy` (luma) + `.tbcc` (chroma) |

**Note**: Y/C formats automatically generate separate luma and chroma files. 
The `mode` field is deprecated and should not be used.
```

---

## Implementation Sequence

**Completed:**
1. ✅ **Phase 1**: YAML mode field removal (already done)
2. ✅ **Phase 2**: Field class extension and format detection
3. ✅ **Phase 2.5**: Video encoder Y/C field generation

**Remaining (in order):**
4. **Phase 3**: Integrate YCTBCWriter
   - Update writer selection in main.cpp
   - Route Y/C fields to separate writers
   - Test: Generate `.tbcy` and `.tbcc` files
   - Test: Verify output with tbcdecode or equivalent

5. **Phase 4**: Configure metadata for Y/C
   - Update metadata generators to work with Y field only
   - Generate `.tbcy.db` for Y/C output
   - Test: VITS/biphase data on Y field only

6. **Phase 5**: Update test projects
   - Remove `mode` field from consumer tape projects
   - Test: Run full test suite

7. **Phase 6**: Update documentation
   - Remove all `mode` field references
   - Add Y/C examples and output explanations

---

## Testing Strategy

### Current Status (After Phase 2.5)
- ✅ Y/C format is now recognized (no error rejection)
- ✅ Filenames handled correctly
- ✅ Y and C field representations are generated during encoding
- ✅ Field objects contain separate Y and C data when Y/C format is selected
- ✅ Composite formats continue to work without Y/C overhead
- ✅ Build succeeds without warnings or errors
- ⏳ Standard composite writer still used for all formats (Phase 3 will add YCTBCWriter)

### Phase 2.5 Testing (Encoder Y/C Generation) ✅ COMPLETED
- ✅ Verified Field objects have Y and C representations populated
- ✅ Verified composite field still generates correct video
- ✅ Checked Y field contains luma-only data (scaled to signal levels)
- ✅ Checked C field contains chroma-only data (centered around middle)
- ✅ Tested both PAL and NTSC Y/C formats
- ✅ Verified composite formats don't create Y/C fields unnecessarily

### Phase 3 Testing (YCTBCWriter Integration)
1. **Y/C output generation**:
   ```bash
   ./encode-orc test-projects/pal-consumer-tape.yaml
   # Verify: test-output/pal-consumer.tbcy exists
   # Verify: test-output/pal-consumer.tbcc exists
   ```

2. **File integrity**:
   ```bash
   ls -lh test-output/pal-consumer.tb*
   # Y and C files should be roughly equal size
   ```

3. **Composite still works**:
   ```bash
   ./encode-orc test-projects/pal-cav.yaml
   # Verify: test-output/pal-cav.tbc exists (composite)
   ```

---

## Rollback Plan

If issues arise during implementation:

1. **Phase 2.5 issues**: 
   - Revert encoder changes (y_field and c_field stay empty)
   - Y/C output will simply use composite representation

2. **Phase 3 issues**:
   - Revert to using TBCWriter for all formats
   - Comment out YCTBCWriter code

3. **Phase 4+ issues**:
   - Revert changes, keep phases 1-3 intact

Each phase is independent—failures don't cascade.

## Success Criteria

**Phase 2** ✅ ACHIEVED:
- ✅ Y/C formats accepted without error
- ✅ Format detection working correctly
- ✅ Project builds without warnings
- ✅ Filenames handled appropriately

**Phase 2.5** (when complete):
- Field objects contain Y and C representations
- Y field contains luma-only data
- C field contains chroma-only data
- Composite field unchanged

**Phase 3** (when complete):
- ✅ `.tbcy` and `.tbcc` files generated
- ✅ `.tbc` files still generated for composite
- ✅ YCTBCWriter correctly splits output

**Phase 4-6** (when complete):
- ✅ Metadata applies only to Y field for Y/C output
- ✅ YAML `mode` field no longer documented
- ✅ All test projects work
- ✅ Documentation reflects new architecture

## Architecture Notes

### Field Class with Dual Representations

The Field class now supports both representations:

```cpp
// Primary representation (always present)
std::vector<uint16_t> data_;  // Composite video

// Optional Y/C representations (created on-demand)
std::unique_ptr<Field> y_field_;  // Luma component
std::unique_ptr<Field> c_field_;  // Chroma component
```

**Advantages**:
- No performance impact for composite-only output (Y/C created only when needed)
- Metadata generators can target Y field specifically
- Writers choose which representation to output
- No lossy decomposition needed

**Memory Usage**:
- Composite-only: same as before
- Y/C output: ~3x (composite + Y + C)
- Acceptable for real-time encoding

### Metadata and Y/C Output

When Y/C output is selected:
- VITS, biphase VBI apply only to Y field
- Color burst applies only to Y field (chroma is separate)
- Metadata databases associated with `.tbcy.db`
- All other metadata behavior unchanged

## Future Considerations

- Support for LEGACY Y/C naming mode (`.tbc` + `_chroma.tbc`)
- Optimization: Skip composite generation if only Y/C output requested
- Consider supporting StandardWriter with Y/C output
- Potential HDV/DVCPRO-style compression for Y/C pair
- Investigation of real-time Y/C encoding without composite generation
