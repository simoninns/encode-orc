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

### Phase 2: Video Standards Implementation

**2.1 PAL Support**
- PAL timing and sync pulse generation
- PAL color encoding (YUV conversion and subcarrier modulation)
- PAL field structure (576 lines, 25fps)
- PAL-specific VITS and VBI line generation

**2.2 NTSC Support**
- NTSC timing and sync pulse generation
- NTSC color encoding (YIQ conversion and subcarrier modulation)
- NTSC field structure (486 lines, 29.97fps)
- NTSC-specific VITS and VBI line generation

**2.3 Signal Generation**
- Composite video signal synthesis
- Y/C separation for YC format
- Blanking intervals and sync levels
- Color burst generation

### Phase 3: File Format Handling

**3.1 TBC File Writer**
- .tbc composite output format
- .tbcy luma output format
- .tbcc chroma output format
- Binary field-based file structure

**3.2 Metadata Database**
- SQLite database integration
- Schema implementation per ld-decode format
- Field/frame metadata storage
- Timecode and frame number tracking

### Phase 4: Input Processing

**4.1 RGB Frame Input**
- Raw RGB file reader
- Frame size validation (PAL: 720×576, NTSC: 720×486)
- Color space conversion pipeline (RGB → YUV/YIQ)
- Frame buffering and management

**4.2 Test Card Generation**
- NTSC test patterns:
  - RCA Indian-head (monochrome)
  - SMPTE color bars
- PAL test patterns:
  - BBC Test Card F
  - Philips PM5544
  - EBU color bars
- Procedural test card rendering

### Phase 5: Command-Line Interface

**5.1 Basic Options**
- Input/output file specification
- Format selection (PAL/NTSC, Composite/YC)
- Resolution and frame rate overrides
- Verbosity levels

**5.2 Advanced Options**
- Timecode embedding configuration
- Frame numbering options
- VBI/VITS data insertion control
- Custom metadata injection

**5.3 OSD Overlay System**
- Frame number overlay
- Timecode display
- Custom text overlay
- Position and style configuration

### Phase 6: Advanced Features

**6.1 Signal Degradation**
- Dropout simulation (configurable locations and durations)
- Noise injection (Gaussian, salt-and-pepper)
- Signal level variations
- Phase/timing errors

**6.2 Quality Testing Tools**
- Signal verification utilities
- Round-trip test capability (encode → decode → compare)
- Automated test suite generation
- Benchmark test patterns

### Phase 7: Testing & Validation

**7.1 Unit Tests**
- Color space conversion accuracy
- Sync pulse timing verification
- Metadata generation correctness
- File format validation

**7.2 Integration Tests**
- Full encoding pipeline tests
- decode-orc compatibility verification
- ld-decode tool chain integration tests
- Performance benchmarks

**7.3 Test Data Sets**
- Create reference test sequences
- Document expected outputs
- Regression test suite

### Phase 8: Documentation & Release

**8.1 User Documentation**
- Usage guide and examples
- Command-line reference
- Test card descriptions
- File format specifications

**8.2 Developer Documentation**
- Architecture overview
- API documentation
- Contribution guidelines
- Code style guide

**8.3 Release Preparation**
- Version 1.0 feature freeze
- Performance optimization
- Cross-platform testing (Linux, macOS, Windows)
- Packaging and distribution

### Recommended Development Order

1. **Start with Phase 1**: Build infrastructure first
2. **Implement Phase 1.4**: Create minimal .tbc files (PAL and NTSC) with all samples at blanking IRE level, plus proper metadata in .tbc.db files. This validates file formats, field structure, and metadata generation before implementing complex encoding
3. **Implement Phase 2 (PAL Composite only)**: Get one complete path working with actual video encoding
4. **Add Phase 3**: File output for PAL Composite
5. **Add Phase 4.1**: RGB input processing
6. **Extend Phase 2**: Add NTSC Composite
7. **Extend Phase 2**: Add YC format support for both standards
8. **Add Phase 4.2**: Test card generation
9. **Implement Phase 5**: Full CLI
10. **Add Phase 6**: Advanced features
11. **Execute Phase 7**: Comprehensive testing
12. **Complete Phase 8**: Documentation and release

### Key Dependencies

- **SQLite3**: For .tbc.db metadata files
- **CLI parser**: Consider CLI11, cxxopts, or Boost.Program_options
- **Build system**: CMake 3.15+
- **Testing framework**: Google Test or Catch2
- **Optional**: FFmpeg libraries (for future RGB input format expansion)

### Milestones
0**: Blanking-level output validation - Generate PAL and NTSC .tbc files with blanking IRE level only, plus valid .tbc.db metadata (validates file formats and structure)
- **M
- **M1**: PAL Composite encoding with static test card (Phases 1-3)
- **M2**: NTSC support and RGB input (Phase 2.2, 4.1)
- **M3**: YC format support (Phase 2.3 extension)
- **M4**: Test card library and OSD (Phases 4.2, 5.3)
- **M5**: Signal degradation features (Phase 6.1)
- **M6**: Production release (Phases 7-8)

This plan provides a structured approach to building encode-orc incrementally, with early validation opportunities and clear deliverables at each milestone.
