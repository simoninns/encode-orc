# Implementation Plan for encode-orc

## Implementation Plan for encode-orc

### Phase 1: Core Infrastructure & Foundation

**1.1 Project Setup**
- Set up C++17 build system (CMake recommended)
- Define directory structure (src/, include/, tests/, docs/)
- Configure compiler flags and dependencies
- Set up version control hooks and CI/CD pipeline
- Create basic command-line argument parser

**1.2 Study Reference Implementation**
- Analyze ld-chroma-encoder source code from ld-decode
- Document the encoding algorithms and parameters
- Understand the .tbc file format structure
- Research ld-decode's SQLite metadata format

**1.3 Core Data Structures**
- Video frame buffer classes (RGB input)
- Field-based video structures (for interlaced formats)
- Video format parameters (PAL/NTSC dimensions, frame rates)
- Metadata structures matching ld-decode's schema

**1.4 Blanking-Level Validation Output**
- Generate minimal .tbc files with all samples at blanking IRE level
- Implement for both PAL and NTSC formats
- Create proper field structure (correct dimensions and field counts)
- Generate complete .tbc.db metadata files
- Validate output can be read by ld-decode tools
- **Purpose**: Validates file formats, metadata schema, and field structure before implementing complex encoding algorithms

### Phase 2: Test Card and Color Bars Implementation

**2.1 PAL EBU Color Bars**
- PAL timing and sync pulse generation
- PAL color encoding (YUV conversion and subcarrier modulation)
- PAL field structure (576 lines, 25fps)
- EBU color bars test pattern rendering
- Color burst generation for PAL

**2.2 NTSC EIA Color Bars**
- NTSC timing and sync pulse generation
- NTSC color encoding (YIQ conversion and subcarrier modulation)
- NTSC field structure (486 lines, 29.97fps)
- EIA color bars test pattern rendering
- Color burst generation for NTSC

**2.3 24-Bit Biphase Encoding**
- Frame number encoding (24-bit biphase format)
- Timecode encoding (24-bit biphase format)
- Integration into VBI lines
- Proper biphase clock and data signal generation

### Phase 3: VITS Implementation

**3.1 PAL VITS** ✅ **COMPLETE**
- PAL vertical interval test signal generation
- VITS line structure and timing
- Reference signals and test patterns
- Integration with blanking intervals
- **Implemented**: ITU Composite (Line 19), ITU ITS (Line 20), UK National (Line 332), Multiburst (Line 333)

**3.2 NTSC VITS**
- NTSC vertical interval test signal generation
- VITS line structure and timing
- Reference signals and test patterns
- Integration with blanking intervals

### Phase 4: Signal Generation

**4.1 Signal Synthesis**
- Composite video signal synthesis
- Y/C separation for YC format
- Blanking intervals and sync levels
- Complete signal pipeline

### Phase 5: File Format Handling

**5.1 TBC File Writer**
- .tbc composite output format
- .tbcy luma output format
- .tbcc chroma output format
- Binary field-based file structure

**5.2 Metadata Database**
- SQLite database integration
- Schema implementation per ld-decode format
- Field/frame metadata storage
- Timecode and frame number tracking

### Phase 6: Input Processing

**6.1 RGB Frame Input**
- Raw RGB file reader
- Frame size validation (PAL: 720×576, NTSC: 720×486)
- Color space conversion pipeline (RGB → YUV/YIQ)
- Frame buffering and management

**6.2 Additional Test Patterns**
- NTSC test patterns:
  - RCA Indian-head (monochrome)
  - SMPTE color bars
- PAL test patterns:
  - BBC Test Card F
  - Philips PM5544
- Procedural test card rendering

### Phase 7: Command-Line Interface

**7.1 Basic Options**
- Input/output file specification
- Format selection (PAL/NTSC, Composite/YC)
- Resolution and frame rate overrides
- Verbosity levels

**7.2 Advanced Options**
- Timecode embedding configuration
- Frame numbering options
- VBI/VITS data insertion control
- Custom metadata injection

**7.3 OSD Overlay System**
- Frame number overlay
- Timecode display
- Custom text overlay
- Position and style configuration

### Phase 8: Advanced Features

**8.1 Signal Degradation**
- Dropout simulation (configurable locations and durations)
- Noise injection (Gaussian, salt-and-pepper)
- Signal level variations
- Phase/timing errors

**8.2 Quality Testing Tools**
- Signal verification utilities
- Round-trip test capability (encode → decode → compare)
- Automated test suite generation
- Benchmark test patterns

### Phase 9: Testing & Validation

**9.1 Unit Tests**
- Color space conversion accuracy
- Sync pulse timing verification
- Metadata generation correctness
- File format validation

**9.2 Integration Tests**
- Full encoding pipeline tests
- decode-orc compatibility verification
- ld-decode tool chain integration tests
- Performance benchmarks

**9.3 Test Data Sets**
- Create reference test sequences
- Document expected outputs
- Regression test suite

### Phase 10: Documentation & Release

**10.1 User Documentation**
- Usage guide and examples
- Command-line reference
- Test card descriptions
- File format specifications

**10.2 Developer Documentation**
- Architecture overview
- API documentation
- Contribution guidelines
- Code style guide

**10.3 Release Preparation**
- Version 1.0 feature freeze
- Performance optimization
- Cross-platform testing (Linux, macOS, Windows)
- Packaging and distribution

### Recommended Development Order

1. **Start with Phase 1**: Build infrastructure first
2. **Implement Phase 1.4**: Create minimal .tbc files (PAL and NTSC) with all samples at blanking IRE level, plus proper metadata in .tbc.db files. This validates file formats, field structure, and metadata generation before implementing complex encoding
3. **Implement Phase 2.1**: PAL EBU color bars (full encoding pipeline)
4. **Implement Phase 2.2**: NTSC EIA color bars
5. **Implement Phase 2.3**: 24-bit biphase encoding for frame numbers and timecode
6. **Implement Phase 3.1**: PAL VITS
7. **Implement Phase 3.2**: NTSC VITS
8. **Add Phase 4**: Complete signal generation pipeline
9. **Add Phase 5**: File output (TBC and metadata)
10. **Add Phase 6.1**: RGB input processing
11. **Add Phase 6.2**: Additional test card patterns
12. **Implement Phase 7**: Full CLI
13. **Add Phase 8**: Advanced features
14. **Execute Phase 9**: Comprehensive testing
15. **Complete Phase 10**: Documentation and release

### Key Dependencies

- **SQLite3**: For .tbc.db metadata files
- **CLI parser**: Consider CLI11, cxxopts, or Boost.Program_options
- **Build system**: CMake 3.15+
- **Testing framework**: Google Test or Catch2
- **Optional**: FFmpeg libraries (for future RGB input format expansion)

### Milestones
- **M0**: Blanking-level output validation - Generate PAL and NTSC .tbc files with blanking IRE level only, plus valid .tbc.db metadata (validates file formats and structure)
- **M1**: PAL and NTSC EBU/EIA color bars with proper encoding (Phases 1, 2.1, 2.2)
- **M2**: 24-bit biphase frame number and timecode encoding (Phase 2.3)
- **M3**: PAL VITS implementation (Phase 3.1)
- **M4**: NTSC VITS implementation (Phase 3.2)
- **M5**: Full signal pipeline and file output (Phases 4, 5)
- **M6**: RGB input and additional test patterns (Phase 6)
- **M7**: Complete CLI and advanced features (Phases 7, 8)
- **M8**: Production release (Phases 9-10)

This plan provides a structured approach to building encode-orc incrementally, with early validation opportunities and clear deliverables at each milestone.
