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

### Phase 2: Core Encoder Logic

**Objective**: Enable Y/C output path in main.cpp and wire it to the appropriate writer

**Files Modified**:
- `src/main.cpp`

**Changes**:

1. **Remove Y/C rejection block** (lines ~269-273):
   ```cpp
   // DELETE THIS:
   bool is_separate_yc = (config.output.mode == "separate-yc" || config.output.mode == "separate-yc-legacy");
   if (is_separate_yc) {
       ENCODE_ORC_LOG_ERROR("Separate Y/C output is not supported by the pipeline encoder");
       ENCODE_ORC_LOG_ERROR("Please use combined output mode or re-enable legacy encoders");
       return 1;
   }
   ```

2. **Replace with format-based detection**:
   ```cpp
   bool is_separate_yc = (config.output.format == "pal-yc" || 
                          config.output.format == "ntsc-yc");
   ```

3. **Select appropriate writer** (around line ~298):
   ```cpp
   std::unique_ptr<Writer> writer;
   
   if (is_separate_yc) {
       // Y/C output: use YCTBCWriter with modern naming (.tbcy/.tbcc)
       writer = std::make_unique<YCTBCWriter>(YCTBCWriter::NamingMode::MODERN);
   } else if (config.output.writer == "standard") {
       writer = std::make_unique<StandardWriter>();
   } else {
       writer = std::make_unique<TBCWriter>();
   }
   ```

4. **Handle filename for Y/C output**:
   ```cpp
   // Ensure proper filename format
   std::string output_filename = config.output.filename;
   
   // For composite formats, add .tbc if missing
   if (!is_separate_yc && 
       config.output.format == "pal-composite" || config.output.format == "ntsc-composite") {
       if (output_filename.length() < 4 || output_filename.substr(output_filename.length() - 4) != ".tbc") {
           output_filename += ".tbc";
       }
   }
   // For Y/C formats, ensure NO extension (YCTBCWriter adds .tbcy/.tbcc)
   else if (is_separate_yc) {
       if (output_filename.length() >= 4 && 
           (output_filename.substr(output_filename.length() - 4) == ".tbc" ||
            output_filename.substr(output_filename.length() - 5) == ".tbcy" ||
            output_filename.substr(output_filename.length() - 5) == ".tbcc")) {
           // Remove any trailing TBC extensions
           size_t dot_pos = output_filename.find_last_of('.');
           if (dot_pos != std::string::npos) {
               output_filename = output_filename.substr(0, dot_pos);
           }
       }
   }
   ```

5. **Update metadata filename handling**:
   ```cpp
   // For metadata, use the full output filename (with extensions added)
   std::string metadata_filename;
   if (is_separate_yc) {
       // For Y/C, metadata goes next to the .tbcy file
       metadata_filename = output_filename + ".tbcy.db";
   } else {
       // For composite, metadata goes next to the .tbc file
       metadata_filename = output_filename + ".db";
   }
   ```

**Rationale**: The encoder should automatically select the correct writer based on the format field. Y/C files need special handling for the filename to ensure the writer can add the correct extensions.

---

### Phase 3: Video Parameters and Encoding

**Objective**: Verify that video encoding produces correct composite output for both YC and composite formats

**Files Affected**:
- `src/main.cpp` (verification only, no changes needed)

**Current Behavior** (correct):
- Video parameters are always composite (PAL or NTSC)
- The encoder produces full composite video internally
- Y/C separation happens at the writer stage, not in the encoder
- All pipeline stages (preprocessing, metadata generation, effects) work with composite video

**No Changes Needed**: The pipeline encoder is format-agnostic for processing—it always produces composite. The writer decides how to output it.

---

### Phase 4: Writer Integration

**Objective**: Verify YCTBCWriter implementation and ensure it's properly integrated

**Files Affected**:
- `src/pipeline/writers/yc_tbc_writer.h` (verification only)
- `src/main.cpp` (integration)

**Existing Functionality** (already implemented):
- YCTBCWriter supports both MODERN and LEGACY naming modes
- MODERN mode: `.tbcy` (luma) and `.tbcc` (chroma)
- LEGACY mode: `.tbc` (luma) and `_chroma.tbc` (chroma)
- Handles splitting composite video into Y and C channels

**Integration Points**:
1. Include header in main.cpp: `#include "pipeline/writers/yc_tbc_writer.h"`
2. Instantiate YCTBCWriter for Y/C formats (see Phase 2)
3. Pass base filename without extension

**No Code Changes**: The YCTBCWriter is already fully functional and ready to use.

---

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

Follow this order to minimize risk and allow for incremental testing:

1. **Phase 1**: Remove YAML mode field parsing and validation
   - Test: Verify config still parses correctly
   - Verify: mode is auto-derived from format

2. **Phase 2**: Update main.cpp logic
   - Remove rejection block
   - Add format-based detection
   - Wire up YCTBCWriter
   - Handle filenames correctly
   - Test: Build without errors

3. **Phase 4**: Verify YCTBCWriter integration (no changes needed)
   - Test: Run with `pal-yc` and `ntsc-yc` formats
   - Verify: `.tbcy` and `.tbcc` files are generated

4. **Phase 5**: Update test projects
   - Remove `mode` field from consumer tape projects
   - Run full test suite

5. **Phase 3**: Verify video encoding (should work as-is)
   - Test: Check output file integrity
   - Verify: Y/C separation is correct

6. **Phase 6**: Update documentation
   - Remove all `mode` field references
   - Add Y/C examples

---

## Testing Strategy

### Unit Tests
- Verify format → mode derivation logic
- Verify filename handling for all format types

### Integration Tests
1. **Composite output**:
   ```bash
   ./encode-orc test-projects/pal-cav.yaml
   # Verify: test-output/pal-cav.tbc exists
   ```

2. **Y/C output**:
   ```bash
   ./encode-orc test-projects/pal-consumer-tape.yaml
   # Verify: test-output/pal-consumer.tbcy and .tbcc exist
   ```

3. **NTSC composite**:
   ```bash
   ./encode-orc test-projects/ntsc-cav.yaml
   # Verify: test-output/ntsc-cav.tbc exists
   ```

4. **NTSC Y/C**:
   ```bash
   ./encode-orc test-projects/ntsc-consumer-tape.yaml
   # Verify: test-output/ntsc-consumer.tbcy and .tbcc exist
   ```

### Verification
- All test projects should complete successfully
- File sizes should be reasonable (Y/C files should be ~2x composite for proper separation)
- Metadata databases should be generated correctly

---

## Rollback Plan

If issues arise during implementation:

1. **Revert Phase 1 changes**: Restore YAML mode parsing
2. **Revert Phase 2 changes**: Restore rejection block
3. **Keep Phase 6 changes**: Update docs to reflect current state

The design is modular—each phase can be reverted independently without affecting others.

---

## Success Criteria

✅ Y/C output formats accepted and processed without errors
✅ `.tbcy` and `.tbcc` files generated for Y/C formats
✅ `.tbc` files still generated for composite formats
✅ YAML `mode` field no longer needed or documented
✅ All existing test projects continue to work
✅ New consumer tape projects generate correct output
✅ Documentation reflects simplified configuration

---

## Future Considerations

- Support for LEGACY Y/C naming mode (`.tbc` + `_chroma.tbc`) if needed
- Potential optimization: avoid full composite generation if Y/C output is requested
- Consider supporting other writer types (standard) with Y/C output
