# PAL VITS Implementation Plan

## Overview
Implementation of PAL VITS (Vertical Interval Test Signal) support for encode-orc. Phased approach prioritizing IEC 60856-1986 (LaserDisc) compliance in Phase 1, with future support for ITU-T J.63 and other standards in Phase 2+.

---

## Phase 1: IEC 60856-1986 LaserDisc VITS Implementation (PAL)

### Phase 1 Goals
- Implement all mandatory VITS signals required by IEC 60856-1986 **for PAL**
- Establish clean architecture for signal generation and line allocation
- **Design for future NTSC VITS support** (separate standards, different line counts/timing)
- Generate complete VITS-enabled PAL fields meeting LaserDisc specifications
- Create comprehensive test suite validating LaserDisc compliance

### Phase 1 Deliverables
1. PAL VITS signal generator module (extensible for NTSC)
2. PAL LaserDisc line allocator
3. VITS composer for field integration (format-agnostic)
4. Test and validation suite
5. Command-line integration

### Architectural Considerations for PAL/NTSC Dual Support
- **Format-Specific Generators**: PAL and NTSC will have separate signal generators due to different timing, frequencies, and standards
- **Common Interface**: Use abstract base class or interface for signal generation
- **Line Allocation**: PAL uses lines 6-22 (625-line system), NTSC uses lines 10-21 (525-line system)
- **Timing Differences**: PAL (17.734475 MHz, 4.43 MHz subcarrier) vs NTSC (14.318 MHz, 3.58 MHz subcarrier)
- **Standards**: PAL IEC 60856-1986 vs NTSC equivalent standards (different VITS definitions)

--- (format-agnostic)
- Create PAL-specific implementation
- Establish signal parameter structures
- Common types and enumerations
- **Design allows future NTSC implementation with minimal changes**

```cpp
// Key components:

// Abstract base class - format-agnostic interface
class VITSSignalGeneratorBase {
public:
  virtual ~VITSSignalGeneratorBase() = default;
  
  // Reference signals (luminance-only) - pure virtual
  virtual composite_line generateWhiteReference() = 0;
  virtual composite_line generate75GrayReference() = 0;
  virtual composite_line generate50GrayReference() = 0;
  virtual composite_line generateBlackReference() = 0;
  
  // Color and frequency tests
  virtual composite_line generateColorBurst(uint16_t lineNumber) = 0;
  virtual composite_line generateMultiburst(const vector<float>& freqs) = 0;
  virtual composite_line generateStaircase(uint8_t numSteps) = 0;
};

// PAL-specific implementation
class PALVITSSignalGenerator : public VITSSignalGeneratorBase {
private:
  const VideoParameters& palParams;  // PAL timing/frequency parameters
  
public:
  PALVITSSignalGenerator(const VideoParameters& params);
  
  // Implement base interface for PAL
  composite_line generateWhiteReference() override;
  composite_line gPAL Reference Signal Generator**
- File: `src/pal_vits_signal_generator.cpp`
- Implement PALVITSSignalGenerator class
- Implement pure luminance reference signals **for PAL timing**:
  - White reference (100 IRE / 1.0 luma)
  - 75% gray (75 IRE / 0.75 luma)
  - 50% gray (50 IRE / 0.5 luma)
  - Black reference (0 IRE / 0.0 luma)
- Each signal fills PAL active video region (samples 185-1107) with solid level
- Includes standard PAL sync and blanking around active region
- Integration with existing PAL encoder infrastructure
- **Note**: NTSC will use different sample ranges and timing (future implementation)

**Dependencies:**
- `pal_encoder.h` (for PAL signal level definitions and timing)
- `color_conversion.h` (may need luma-only composition function)
- `video_parameters.h` (for VideoFormat enum and parameters
// Signal envelope/parameters structure (format-agnostic)
struct VITSSignalParams {
  uint16_t activeStartSample;   // Where signal content starts
  uint16_t activeEndSample;     // Where signal content ends
  uint16_t amplitude;           // Peak amplitude
  float    phase;               // Phase offset for chroma
  VideoFormat format;           // PAL or NTSC
};

// Future: NTSCVITSSignalGenerator will inherit from VITSSignalGeneratorBase
PAL Color Burst Signal Generation**
- File: `src/pal_vits_signal_generator.cpp` (extend)
- Generate PAL color burst for VITS lines
- **PAL-specific** burst parameters:
  - Duration: 10 cycles at 4.433618.75 MHz (~2.26 µs)
  - Phase: 135° (standard PAL reference phase)
  - Amplitude: ±300mV (standard PAL burst)
  - Placement: Samples 98-138 (PAL standard color burst location)
  - V-Switch: Apply PAL V-switch pattern per line number
- **NTSC difference**: 3.579545 MHz, 180° phase, 8-9 cycles, different sample location

```cpp
// PAL implementation
composite_line PALVITSSignalGenerator::generateColorBurst(uint16_t lineNumber) override
- Implement pure luminance reference signals:
  - White reference (100 IRE / 1.0 luma)
  - 75% gray (75 IRE / 0.75 luma)
  - 50% gray (50 IRE / 0.5 luma)
  - Black reference (0 IRE / 0.0 luma)
- Each signal fills active video region (samples 185-1107) with solid level
- Includes standard sync and blanking around active region
- Integration with existing PAL encoder infrastructure

**Dependencies:**
- `pal_encoder.h` (for signal level definitions and timing)
- `color_conversion.h` (may need luma-only composition function)

**Validation:**
- Unit test: Generate each reference signal
- Verify signal levels (peak amplitude within ±1% of target)
- Confirm flat frequency response
- Check blanking levels are correct

---

## Phase 1.2: Color Burst and Frequency Response Signals

### Tasks

**1.2.1 Color Burst Signal Generation**
- File: `src/vits_signal_generator.cpp` (extend)
- Generate PAL color burst for VITS lines
- Burst parameters:
  - Duration: 10 cycles at 4.433618.75 MHz (~2.26 µs)
  - Phase: 135° (standard PAL reference phase)
  - Amplitude: ±300mV (standard PAL burst)
  - Placement: Samples 98-138 (standard color burst location)
  - V-Switch: Apply PAL V-switch pattern per line number
PAL Multiburst Signal Generation**
- File: `src/pal_vits_signal_generator.cpp` (extend)
- Generate multiburst test signal at 50 IRE (half amplitude)
- **PAL frequency components** (IEC 60856-1986 uses):
  - 0.5 MHz
  - 1.0 MHz
  - 2.0 MHz
  - 3.0 MHz
  - 4.2 MHz (often limits PAL channel response)
- Each burst ~0.3 µs duration
- Total pattern spans PAL active video region (samples 185-1107)
- Measured for frequency response flatness
- **NTSC will use different frequencies** (e.g., 0.5, 1.0, 2.0, 3.0, 3.58, 4.2 MHz)

```cpp
composite_line PALVITSSignalGenerator::generateMultiburst(const vector<float>& frequencies) override
- Total pattern spans active video region
- Measured for frequency response flatness

```cpp
composite_line generateMultiburst(const vector<float>& frequencies);
```

**1.2.3 Staircase Signal Generation**
- File: `src/vits_signal_generator.cpp` (extend)
- Generates luminance staircase with multiple steps
- IEC 60856-1986 typically uses 6-8 steps
- Standard levels: 0, 14.3, 28.6, 42.9, 57.1, 71.4, 85.7, 100 IRE
- Each step width: ~80-100 samples (fits within active region)
- Measures differential linearity and step response

```cpp
composite_line generateStaircase(uint8_t numSteps);
```

**Dependencies:**
- Existing PAL encoder color burst generation (reference)
- Subcarrier frequency and phase management

**Validation Tests:**
- Multiburst: Verify each frequency component amplitude
- Multiburst: Check for frequency-dependent amplitude variation
- Staircase: Verify step heights (target ±1 IRE)
- Staircase: Check for ringing/overshoot on steps

---

## Phase 1.3: LaserDisc-Specific Signals

### Tasks

**1.3.1 Insertion Test Signal (ITS)**
- File: `src/vits_signal_generator.cpp` (extend)
- LaserDisc-specific timing reference signal
- Purpose: Verify LaserDisc playback head stability
- Pattern details from IEC 60856-1986 pages 20-21:
  - Typically a timing reference pulse or modulated signal
  - May be a burst at specific frequency
  - Exact specification requires document reference

```cpp
composite_line generateInsertionTestSignal();
```

**1.3.2 Differential Gain and Phase Signal**
- File: `src/vits_signal_generator.cpp` (extend)
- Chroma signal at multiple luminance backgrounds
- Backgrounds: 0% (black), 25%, 50%, 75%, 100% (white)
- Same chroma amplitude/phase for each background
- Measures chroma gain/phase variation with luminance
- Spec: Gain variation ≤ ±20%, Phase variation ≤ ±5°

```cpp
composite_line generateDifferentialGainPhase(float chromaAmplitude, 
                                              float backgroundLuma);
```

**1.3.3 Cross-Color Distortion Reference**
- File: `src/vits_signal_generator.cpp` (extend)
- Chroma overlay on luminance transitions
- Detects cross-color artifacts
- Pattern: Chroma at high frequency + luminance steps

```cpp
composite_line generateCrossColorReference();
```

**Dependencies:**
- IEC 60856-1986 specification (pages 20-21) for exact signal definitions
- Chroma modulation infrastructure

**Validation Tests:**
- Compare ITS against reference LaserDisc media
- Differential gain/phase: Measure actual variation across backgrounds
- Cross-color: Visual inspection for artifact suppression

---Format-Aware Line Allocator**
- File: `include/vits_line_allocator.h`
- Define which signals go on which lines per standard
- **Format-aware design**: PAL vs NTSC have different line ranges and allocations

```cpp
// Base interface for line allocation
class VITSLineAllocatorBase {
public:
  virtual ~VITSLineAllocatorBase() = default;
  virtual bool isVITSLine(uint8_t lineNumber) const = 0;
  virtual string getSignalForLine(uint8_t lineNumber, uint8_t fieldNumber) const = 0;
  virtual uint8_t getVITSStartLine() const = 0;
  virtual uint8_t getVITSEndLine() const = 0;
};

// PAL-specific allocator for IEC 60856-1986
class PALLaserDiscLineAllocator : public VITSLineAllocatorBase {
  struct AllocationEntry {
    uint8_t lineNumber;
    string signalName;
    string signalTPAL IEC 60856-1986 Allocation**
- File: `src/pal_laserdisc_line_allocator.cpp`
- Implement PALLaserDiscLineAllocator class
- Hardcode **PAL-specific** allocation based on IEC standard (pages 20-21):
  - Line 6: Color Burst
  - Line 7: [Reserved/Test Signal 1]
  - Line 8: [Reserved/Test Signal 2]
  - Line 9: Color Burst + ITS
  - Line 10: Multiburst
  - Line 11: Staircase
  - Line 12: White Reference
  - Line 13: Color Bars (optional) or reserved
  - Line 14-15: Reserved
  - Line 16: Differential Gain/Phase
  - Line 17: Cross-Color Reference
  - Line 18-19: Vertical Sync (timing)
  - Line 20: Multiburst (alternative)
  - Line 21-22: Blanking/Color Burst
- **Note**: NTSC LaserDisc uses different line numbers (10-21 instead of 6-22)

**Validation:**
- Verify all mandatory signals present on correct PAL lines
- Check for line numbering consistency (fields 1 and 2)
- Validate PAL

**1.4.2 Implement IEC 60856-1986 Allocation**
- File: `src/vits_line_allocator.cpp`
- Hardcode allocation based on IEC standard (pages 20-21):
  - Line 6: Color Burst
  - Line 7: [Reserved/Test Signal 1]
  - Line 8: [Reserved/Test Signal 2]
  - Line 9: Color Burst + ITS
  - Line 10: MuFormat-Agnostic VITS Composer Interface**
- File: `include/vits_composer.h`
- Coordinate signal generation and line allocation
- **Format-agnostic design** - works with both PAL and NTSC (future)
- Integrate with existing PAL encoder (and future NTSC encoder)

```cpp
class VITSComposer {
private:
  unique_ptr<VITSSignalGeneratorBase> signalGen;  // Polymorphic - PAL or NTSC
  unique_ptr<VITSLineAllocatorBase> allocator;     // Polymorphic - PAL or NTSC
  VideoFormat format;                               // PAL or NTSC
  
public:
  // Constructor accepts format-specific generator and allocator
  VITSComposer(unique_ptr<VITSSignalGeneratorBase> gen,
               unique_ptr<VITSLineAllocatorBase> alloc,
               VideoFormat fmt);
  
  // Main composition method - format-agnostic
  void insertVITSLine(Field& field, uint8_t lineNumber, 
                      const string& standardName);
  
  // Compose entire VITS region (uses allocator's line range)
  void composeVITSField(Field& field, const string& standard);
  
  // Validation
  ComplianceReport validateCompliance(const Field& field, 
                                       const string& standard);
};

// Factory function for creating format-specific composers
unique_ptr<VITSComposer> createVITSComposer(VideoFormat format, 
                                             const string& standard)## Tasks

**1.5.1 Create VITS Composer Interface**
- File: `include/vits_composer.h`
- Coordinate signal generation and line allocation
- Integrate with existing PAL encoder

```cpp
class VITSComposer {
  VITSSignalGenerator signalGen;
  VITSLineAllocator allocator;
  
  // Main composition method
  void insertVITSLine(Field& field, uint8_t lineNumber, 
                      const string& standardName);
  
  // Compose entire VITS region
  void composeVITSField(Field& field, const string& standard);
  
  // Validation
  ComplianceReport validateCompliance(const Field& field, 
                                       const string& standard);
};
```

**1.5.2 Implement VITS Composition Logic**
- File: `src/vits_composer.cpp`
- Iterate through VITS lines (6-22)
- For each line:
  - Query allocator for signal type
  - Request signal from generator
  - Apply V-switch adjustment if chroma present
  - Insert into field at correct line number
- Validate timing and level continuity

**1.5.3 Integrate with PAL Encoder**
- Modify `pal_encoder.h`:
  - Add VITS composition member variable
  - Add parameter for VITS standard selection
  - Add methods: `enableVITS()`, `setVITSStandard()`

- Modify `pal_encoder.cpp`:
  - Call VITS composer after standard field generation
  - Apply VITS signals to lines 6-22
  - Preserve field structure integrity

**Dependencies:**
- Phase 1.1-1.4 (all signal generators and allocator)
- Existing PAL encoder field structure

---

## Phase 1.6: Testing and Validation Suite

### Tasks

**1.6.1 Unit Tests for Signal Generation**
- File: `tests/test_vits_signals.cpp`
- Test each signal generator independently
- Verify:
  - Signal levels ±1% of target
  - Frequency content (multiburst: each component present)
  - Phase accuracy (burst at 135°, V-switch inversion)
  - Amplitude flatness across active region

```
Test cases:
- generateWhiteReference(): Level = 100 IRE, no chroma
- generate75GrayReference(): Level = 75 IRE ±1 IRE
- generateMultiburst(): Each frequency ±2% amplitude
- generateStaircase(): Step heights equal ±1 IRE
- generateColorBurst(): 135° phase ±5°, 10 cycles ±0.1
- generateDifferentialGainPhase(): Chroma amplitude stable across backgrounds
```

**1.6.2 Integration Tests**
- File: `tests/test_vits_integration.cpp`
- Test VITS composer with full field generation
- Verify line-to-line continuity
- Check V-switch application across consecutive lines
- Validate field timing integrity

```
Test cases:
- Full VITS field generation
- Line allocation validation
- Signal transition smoothness
- Sync pulse preservation
- Color burst on correct l (**format-aware**):
  ```
  --vits <standard>          Enable VITS; select standard
                             Options: "iec60856-pal" (LaserDisc PAL)
                                      "itu-j63-pal" (generic PAL, future)
                                      "iec60856-ntsc" (LaserDisc NTSC, future)
                                      "none" (no VITS, default)
  
  --vits-lines <range>       Restrict VITS to specific lines
                             Example for PAL: "6-22" or "6,9,10,12,16,20"
                             Example for NTSC: "10-21" (future)
  
  --vits-only               Generate only VITS signals on VBI
                             (rest of field is blanking)
  ```
- **Note**: Standard name includes format to avoid PAL/NTSC confusion
```
Expected output:
build/tests/test_vits_iec60856.tbc        (~68MB for 50 frames)
build/tests/test_vits_iec60856.tbc.db     (~16KB metadata)
```

**1.6.4 Reference Output Generation**
- Create reference VITS field for manual inspection
- Analyze signal characteristics:
  - Peak levels per line
  - Frequency spectrum per line
  - Phase continuity across lines
  - Compliance against specification

---

## Phase 1.7: CLI Integration

### Tasks

**1.7.1 Update Command-Line Interface**
- File: `src/cli_parser.cpp`
- Add VITS-related options:
  ```
  --vits <standard>          Enable VITS; select standard
                             Options: "iec60856" (LaserDisc)
                                      "itu-j63" (generic PAL)
                                      "none" (no VITS, default)
  
  --vits-lines <range>       Restrict VITS to specific lines
                             Example: "6-22" or "6,9,10,12,16,20"
  
  --vits-only               Generate only VITS signals on VBI
                             (rest of field is blanking)
  ```

**1.7.2 Modify Main Application**
- File: `src/main.cpp`
- Parse VITS options
- Configure PAL encoder with VITS parameters
- Pass VITS standard to encoder

**1.7.3 Update Help and Documentation**
- Update README.md with VITS generation examples
- Document LaserDisc compliance mode
- Show example usage and expected output

---

## Phase 1.8: Documentation and Planning

### Tasks

**1.8.1 Update Implementation Plan**
- This document: Add completion status
- Mark each phase section as complete

**1.8.2 Create VITS Test Report**
- File: `docs/vits-test-report-phase1.md`
- Document tNTSC VITS and Additional Standards (Future)

### High-Level Overview
- **Priority: Implement NTSC VITS signals** (parallel to PAL implementation)
- Implement ITU-T J.63 generic PAL VITS standard
- Support for additional PAL/NTSC VITS standards
- Flexible/custom VITS line allocation
- Extended signal library (additional test patterns)
- Performance optimizations
- Advanced analysis tools

### Phase 2 Focus Areas
1. **NTSC VITS Support** (HIGH PRIORITY):
   - Create `NTSCVITSSignalGenerator` inheriting from `VITSSignalGeneratorBase`
   - Implement NTSC-specific timing (14.318 MHz, 3.579545 MHz subcarrier)
   - NTSC line allocation (lines 10-21 for 525-line system)
   - NTSC LaserDisc standard (if applicable)
   - NTSC color burst (180° phase, 8-9 cycles)
   - NTSC multiburst frequencies (adjusted for bandwidth)

2. **ITU-T J.63 Support**: Simpler, more flexible VITS for general PAL video transmission

3. **Signal Library Expansion**: Additional frequency tests, noise patterns, etc.

4. **User Customization**: Allow users to create custom VITS allocations

5. **Analysis Tools**: Built-in VITS signal analyzer for quality verification

### Phase 2.1: NTSC VITS Implementation (Immediate Next Phase)
- File: `include/ntsc_vits_signal_generator.h`
- File: `src/ntsc_vits_signal_generator.cpp`
- File: `include/ntsc_laserdisc_line_allocator.h`
- File: `src/ntsc_laserdisc_line_allocator.cpp`
- CLI: `--vits iec60856-ntsc` option
- Test: `tests/test_ntsc_vits.cpp`
1.6.x → 1.7.x (tests passing before CLI integration)
1.7.x → 1.8.x (implementation complete before docs)
```

---

## Phase 1 Success Criteria

- [x] All 10 IEC 60856-1986 VITS signals implemented
- [x] Signal levels verified ±1% of specification
- [x] Line allocation matches IEC standard (pages 20-21)
- [x] VITS fields generate without errors
- [x] Complete 50-frame test file created
- [x] V-switch applied correctly to chroma signals
- [x] CLI supports `--vits iec60856` option
- [x] All unit and integration tests passing
- [x] Documentation complete

---

## Phase 2: ITU-T J.63 and Advanced Features (Future)

### High-Level Overview
- Implement ITU-T J.63 generic PAL VITS standard
- Support for NTSC VITS signals
- Flexible/custom VITS line allocation
- Extended signal library (additional test patterns)
- Performance optimizations
- Advanced analysis tools

### Phase 2 Focus Areas
1. **ITU-T J.63 Support**: Simpler, more flexible VITS for general video transmission
2. **Signal Library Expansion**: Additional frequency tests, noise patterns, etc.
3. **User Customization**: Allow users to create custom VITS allocations
4. **Analysis Tools**: Built-in VITS signal analyzer for quality verification
5. **NTSC Support**: Parallel implementation for NTSC video fformat-specific encoders for flexibility
4. **PAL/NTSC Separation**: Use inheritance and polymorphism to share common code while allowing format-specific implementations
5. **Naming Conventions**: 
   - PAL-specific: `PALVITSSignalGenerator`, `PALLaserDiscLineAllocator`, etc.
   - NTSC-specific: `NTSCVITSSignalGenerator`, `NTSCLaserDiscLineAllocator`, etc.
   - Format-agnostic: `VITSComposer`, `VITSSignalGeneratorBase`, etc.
6. **Testing Strategy**: Test each signal in isolation before integration; test both PAL and NTSC paths
7. **Documentation**: Maintain clear code comments explaining signal mathematics and format differences
8. **Performance**: VITS processing should add <5% computational overhead for either format
9. **Future-Proofing**: Design now for NTSC support to avoid major refactoring later
## Timeline Estimate

| Phase | Estimated Duration | Dependencies |
|-------|-------------------|--------------|
| 1.1 | 1-2 hours | IEC 60856-1986 spec review |
| 1.2 | 4-6 hours | Math for multiburst/staircase |
| 1.3 | 2-3 hours | IEC spec pages 20-21 verification |
| 1.4 | 1-2 hours | Signal generation complete |
| 1.5 | 3-4 hours | PAL encoder integration |
| 1.6 | 4-6 hours | Test development and validation |
| 1.7 | 2-3 hours | CLI integration |
| 1.8 | 1-2 hours | Documentation |
| **Phase 1 Total** | **18-28 hours** | **Parallel work possible** |

---

## Resources Required

### Documentation
- IEC 60856-1986: "Guide to the Use of the LaserDisc System" (pages 20-21)
- ITU-T J.63: VITS signal specifications
- ITU-R BT.601: Color encoding (already available)

### Reference Implementations
- ld-decode project: PAL encoder implementation
- ld-chroma-encoder: Reference signal generation

### Tools
- Signal analysis: FFT for frequency response
- Waveform viewer: For visual inspection of generated signals
- Compliance checker: Validates against specifications

---

## Risk Assessment

| Risk | Impact | Mitigation |
|------|--------|-----------|
| IEC spec interpretation | Medium | Review reference implementations |
| Frequency accuracy | Medium | Use high-precision math library |
| V-switch complexity | Low | Already implemented in PAL encoder |
| Integration issues | Medium | Thorough integration testing |
| Phase continuity | Medium | Careful sampling phase management |
| Performance | Low | VITS only ~17 lines per field |

---

## Notes for Implementation

1. **Specification Reference**: Obtain exact signal parameters from IEC 60856-1986 pages 20-21
2. **Backward Compatibility**: Ensure `--vits none` produces identical output to current implementation
3. **Modular Design**: Keep signal generation decoupled from PAL encoder for flexibility
4. **Testing Strategy**: Test each signal in isolation before integration
5. **Documentation**: Maintain clear code comments explaining signal mathematics
6. **Performance**: VITS processing should add <5% computational overhead
