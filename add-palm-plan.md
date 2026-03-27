# PAL-M Support Plan for encode-orc

Date: 2026-03-27
Status: Planning only (no code changes in this step)

## Goal
Add PAL-M output support while reusing as much PAL chroma encoding behavior as possible, but with 525-line/59.94 timing and NTSC-like frame/field geometry.

## Requested Constraints
- Keep PAL-style color encoding path wherever practical.
- Use NTSC assets as source material for PAL-M tests and examples.
- Produce a plan only in this step; do not modify implementation yet.

## Summary of Current State
- The codebase already has a `VideoSystem::PAL_M` enum value, but most logic still branches only on PAL vs NTSC.
- Output format parsing currently accepts only: pal-composite, ntsc-composite, pal-yc, ntsc-yc.
- PAL active encoding and PAL burst generation currently hardcode 625/50 style sequence assumptions in several places.
- Many subsystems infer behavior from PAL vs NTSC only (audio samples per field, fps, active line boundaries, VBI/VITS validation, loader expected dimensions).

## PAL-M Target Behavior (Implementation Intent)
- Video geometry and timing base:
  - 525 lines, interlaced fields like NTSC geometry (field heights 262/263).
  - Frame cadence 29.97 fps (59.94 fields/s).
  - Active picture dimensions compatible with existing NTSC sources (720x480).
- Chroma behavior:
  - PAL-style color modulation (PAL V-switch behavior) but using PAL-M constants and 525/59.94 timing model.
- Source compatibility:
  - NTSC assets (e.g. existing 720x480 material) should be accepted as practical input for PAL-M encoding.

## Work Plan

### Phase 1: Define PAL-M Parameter Set and Public Format Names
1. Add PAL-M output format names to config validation and runtime selection.
2. Introduce a dedicated PAL-M video parameter factory in `VideoParameters`.
3. Confirm and document PAL-M constants:
   - Subcarrier frequency (PAL-M specific).
   - Sample rate (4xfSC).
   - Field width and active region positions.
   - Burst region and signal levels (initially PAL defaults unless PAL-M-specific values are available/required).
4. Ensure metadata string output supports PAL_M end-to-end (already partially present in writer schema).

Files to touch later:
- src/config/yaml_config.cpp
- src/main.cpp
- src/pipeline/common/video_parameters.h

Acceptance criteria:
- PAL-M appears as a valid output format in config parsing and runtime logs.
- Parameters are selectable without fallback to PAL/NTSC defaults.

### Phase 2: Encoder and Pipeline Wiring
1. Update pipeline builder to support PAL_M selection.
2. Decide implementation strategy:
   - Option A (preferred): Reuse PAL encoder with PAL-M-aware phase/V-switch calculations.
   - Option B: Add a dedicated PAL-M active encoder class if PAL-only assumptions are too invasive.
3. Remove hardcoded 625-line assumptions from PAL phase logic for PAL-M paths.
4. Update color burst generation decision points so PAL-M uses PAL-style burst generation with PAL-M timing.

Files to touch later:
- src/pipeline/orchestrator/video_encoder_pipeline.cpp
- src/pipeline/active_encoding/pal_active_encoder.cpp
- src/pipeline/active_encoding/pal_active_encoder.h
- src/pipeline/metadata_generators/color_burst_generator.cpp
- src/pipeline/metadata_generators/color_burst_pipeline_generator.cpp
- src/pipeline/metadata_generators/pipeline_generators.cpp

Acceptance criteria:
- PAL-M build path creates a valid active encoder.
- PAL-M does not enter NTSC encoder path.
- PAL-M phase/burst math no longer assumes 625-line PAL framing.

### Phase 3: Geometry, Timing, and Audio Assumptions Across Pipeline
1. Replace PAL-vs-NTSC binary branches with PAL/PAL_M/NTSC explicit handling where needed.
2. Set PAL-M frame rates and samples per field to NTSC-like timing values.
3. Ensure field structure generation uses correct PAL-M line counts and active region boundaries.
4. Ensure loaders treat PAL-M expected source dimensions as 720x480 by default.

Files to touch later:
- src/main.cpp
- src/pipeline/field_splitting/field_splitter.cpp
- src/pipeline/loaders/video_loader_base.cpp
- src/pipeline/loaders/yuv422_loader.cpp
- src/pipeline/orchestrator/video_encoder_pipeline.cpp
- src/pipeline/structure_generation/field_structure_generator.cpp
- src/pipeline/metadata_generators/metadata_generator.cpp
- src/pipeline/metadata_generators/vitc_generator.cpp
- src/pipeline/metadata_generators/pipeline_generators.cpp
- src/pipeline/common/metadata.h

Acceptance criteria:
- PAL-M uses 29.97 fps behavior where timing-dependent logic currently chooses 25 vs 30.
- PAL-M uses NTSC-like source dimensions by default (720x480).
- Audio sample packing per field aligns with 59.94-field cadence assumptions.

### Phase 4: Metadata and Vertical Interval Policy
1. Define PAL-M policy for metadata generators:
   - Biphase VBI: likely not LaserDisc-standard PAL or NTSC profile; default policy should be explicit.
   - VITS: decide whether to allow PAL, NTSC, both, or neither for PAL-M in first release.
   - VITC: frame-rate behavior should follow 29.97/30 path.
2. Update source-standard capability checks to avoid accidental PAL-only gating for PAL-M where inappropriate.
3. Add explicit validation/error messaging for unsupported PAL-M metadata combinations (if any).

Files to touch later:
- src/pipeline/common/source_video_standard.h
- src/pipeline/metadata_generators/*
- src/main.cpp (generator construction and line validation)

Acceptance criteria:
- PAL-M metadata behavior is explicit and documented, not accidental.
- Invalid PAL-M metadata combinations fail with clear messages.

### Phase 5: Test Projects and Assets
1. Add a required PAL-M comprehensive test-project set under test-projects (same shape as existing NTSC and PAL sets):
   - test-comprehensive-palm-cav-composite.yaml
   - test-comprehensive-palm-clv-composite.yaml
   - test-comprehensive-palm-vitc-yc.yaml
2. Add a required PAL-M GGV test set under ggv-tests with 1958 naming:
   - ggv1958-palm-cav-composite.yaml
   - ggv1958-palm-clv-composite.yaml
   - ggv1958-palm-vitc-yc.yaml
3. Use NTSC assets as source material in PAL-M test configs (as requested).
4. Add coverage for key paths:
   - YUV422 image source.
   - MOV/MP4 source with frame-rate validation.
   - Metadata generator combinations expected to pass/fail.

Files to add later:
- test-projects/test-comprehensive-palm-cav-composite.yaml
- test-projects/test-comprehensive-palm-clv-composite.yaml
- test-projects/test-comprehensive-palm-vitc-yc.yaml
- ggv-tests/ggv1958-palm-cav-composite.yaml
- ggv-tests/ggv1958-palm-clv-composite.yaml
- ggv-tests/ggv1958-palm-vitc-yc.yaml

Acceptance criteria:
- All 3 PAL-M test-project YAMLs exist and run without code-path fallbacks to PAL or NTSC.
- All 3 PAL-M GGV YAMLs exist with the ggv1958-palm-* naming and run successfully.
- NTSC source assets are accepted for PAL-M tests.

### Phase 6: Validation and Regression Safety
1. Build and run existing PAL and NTSC tests to ensure no regressions.
2. Run PAL-M tests and inspect output metadata fields (`system`, dimensions, field counts, timing-derived values).
3. Add focused checks around:
   - Field phase identifiers.
   - Audio sample counts per field.
   - VITS/VITC line placement bounds.
4. Document expected PAL-M behavior in README or project docs.

Acceptance criteria:
- Existing PAL/NTSC tests remain green.
- PAL-M tests pass and produce internally consistent metadata/output dimensions.

## Risks and Mitigations
- Risk: PAL logic currently hardcodes 625-line assumptions.
  - Mitigation: isolate PAL-specific constants and make PAL-M-aware branch points explicit.
- Risk: Ambiguous PAL-M metadata standard behavior.
  - Mitigation: define first-release policy clearly (supported vs rejected generator combinations).
- Risk: Hidden two-way PAL/NTSC branches in unrelated modules.
  - Mitigation: search for all PAL-vs-NTSC binary conditionals and convert to explicit three-way handling.

## Open Decisions Before Implementation
1. Exact PAL-M signal-level constants to use initially:
   - Reuse PAL defaults or introduce PAL-M-specific levels from measured/reference captures.
2. PAL-M VITS policy for first iteration:
   - Reuse NTSC VITS line conventions, reuse PAL conventions, or disable by default.
3. Output format naming:
   - Use palm-composite/palm-yc (recommended for CLI/YAML consistency) or pal-m-composite/pal-m-yc.
4. Whether PAL-M should be treated as compatible with consumer-tape-only metadata by default.

## Proposed Execution Order
1. Phase 1 (format + parameters)
2. Phase 2 (encoder + burst path)
3. Phase 3 (timing/geometry branches)
4. Phase 4 (metadata policy)
5. Phase 5 (tests)
6. Phase 6 (validation + docs)

## Definition of Done
- PAL-M can be selected in project YAML.
- Encoding path uses PAL chroma behavior with PAL-M geometry/timing.
- NTSC assets work as PAL-M source material for planned test cases.
- PAL/NTSC behavior remains unchanged by regression testing.
- PAL-M behavior and limitations are documented.
