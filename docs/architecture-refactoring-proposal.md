# Architecture Refactoring Proposal for encode-orc

## Executive Summary

This document analyzes the current architecture of `encode-orc` and proposes a refactoring strategy to better represent the video processing pipeline, improve code reuse, and prepare for future enhancements (like noise/dropout simulation).

## Current Architecture Analysis

### Processing Pipeline Overview

The current system implements a **frame-based video encoding pipeline** with these stages:

```
┌─────────────┐
│   Loaders   │ → Load video frames from various sources
└──────┬──────┘
       ↓
┌─────────────────┐
│ Frame → Fields  │ → Split progressive frames into interlaced fields
└──────┬──────────┘
       ↓
┌──────────────────┐
│    Generators    │ → Add VBI, VITC, VITS metadata to fields
└──────┬───────────┘
       ↓
┌──────────────────┐
│    Encoders      │ → PAL/NTSC composite encoding with sync & subcarrier
└──────┬───────────┘
       ↓
┌──────────────────┐
│     Writers      │ → Output to TBC/Standard files
└──────────────────┘
```

### Current Components

#### 1. **Loaders** (Input Stage)
- **Purpose**: Load frame-based video and convert to YUV444P16 format
- **Classes**:
  - `VideoLoaderBase` - Abstract base class
  - `YUV422Loader` - Loads raw YUV 4:2:2 images
  - `PNGLoader` - Loads PNG images (RGB → YUV conversion)
  - `MOVLoader` - Loads MOV files (v210, ProRes) via ffmpeg
  - `MP4Loader` - Loads MP4 files via ffmpeg
  - `VideoLoaderUtils` - Shared color space conversion utilities

**Current Issues**:
- Each loader has slightly different interfaces (inconsistent parameter lists)
- Color space conversion logic duplicated across loaders
- `load_frame()` vs `load_frames()` interface inconsistency
- Loaders are tightly coupled to specific file formats

#### 2. **Frame/Field Data Structures**
- **Classes**:
  - `FrameBuffer` - Stores progressive frames (RGB48 or YUV444P16)
  - `Field` - Stores individual interlaced fields (16-bit samples)
  - `Frame` - Container for two fields (field1 + field2)

**Current Issues**:
- Clear separation between input (FrameBuffer) and encoded output (Field/Frame)
- No intermediate representation for "field with metadata" before encoding
- Limited support for field-level operations (needed for noise/dropout simulation)

#### 3. **Generators** (Metadata/Signal Addition)
- **Purpose**: Add in-field metadata and test signals
- **Classes**:
  - `VITCGenerator` - VITC timecode for consumer tape
  - `PALVITSGenerator` / `NTSCVITSGenerator` - Vertical interval test signals
  - `BiphaseEncoder` - LaserDisc VBI frame numbers (lines 16-18)
  - `ColorBurstGenerator` - Color burst reference signal
  - `ManchesterEncoder` - Data encoding helper

**Current Issues**:
- Generators are **embedded inside encoders** (PALEncoder, NTSCEncoder)
- No standalone pipeline stage for metadata addition
- VBI data is passed as a pointer `const VBIData*` at encode time
- Difficult to add/remove metadata generators independently
- VITC/VITS enabled via encoder methods, not as separate pipeline stages

#### 4. **Encoders** (Signal Encoding)
- **Purpose**: Convert YUV fields to composite video signals
- **Classes**:
  - `PALEncoder` - PAL composite encoding with V-switch, subcarrier modulation
  - `NTSCEncoder` - NTSC composite encoding with YIQ, subcarrier modulation
  - `FIRFilter` - Optional chroma/luma filtering

**Current Issues**:
- **Monolithic design**: Encoders handle too many responsibilities:
  - Frame → field splitting
  - Sync pulse generation
  - Color burst generation
  - VBI line generation (biphase data)
  - VITS generation
  - VITC generation
  - Active video encoding (subcarrier modulation)
  - Filtering
- Difficult to test individual encoding stages
- Hard to insert field-level operations (like noise simulation) mid-pipeline
- Code duplication between PALEncoder and NTSCEncoder

#### 5. **Writers** (Output Stage)
- **Purpose**: Write encoded fields to disk
- **Classes**:
  - `Writer` - Abstract base class
  - `TBCWriter` - Writes TBC format (with field padding)
  - `StandardWriter` - Writes raw fields (no padding)
  - `YCTBCWriter` - Writes separate Y/C files
  - `MetadataWriter` - Writes .db metadata

**Current Issues**:
- Clear abstraction with Writer base class ✓
- Works well for current needs
- Could benefit from streaming support for large files

#### 6. **Orchestration**
- **Classes**:
  - `VideoEncoder` - Main orchestrator in `video_encoder.cpp`
  - `main.cpp` - Project file parsing and section sequencing

**Current Issues**:
- `VideoEncoder` does too much:
  - Creates loaders
  - Creates encoders
  - Manages VBI metadata generation
  - Manages writers
  - Handles frame looping
- Difficult to test individual stages
- Hard to insert custom processing steps

---

## Problems with Current Architecture

### 1. **Lack of Clear Pipeline Stages**
The processing pipeline is **implicit** rather than **explicit**. The flow is buried in nested function calls within `VideoEncoder::encode_*_image()` methods.

### 2. **Tight Coupling**
- Generators are embedded in encoders
- Encoders create their own generators (VITC, VITS, ColorBurst)
- Loaders directly produce final FrameBuffer format
- No dependency injection or factory patterns

### 3. **Limited Extensibility**
Adding new features requires modifying core classes:
- **Noise/dropout simulation** would require:
  - Modifying PALEncoder/NTSCEncoder to accept pre-modified fields
  - OR duplicating encoding logic for noise insertion
- **New metadata types** require modifying encoder classes
- **Custom filters** require modifying encoder constructors

### 4. **Code Duplication**
- PALEncoder and NTSCEncoder share 70%+ similar code:
  - Sync pulse generation
  - Blanking line generation
  - VBI line structure
  - VITC/VITS integration
- Only subcarrier modulation and phase calculations differ

### 5. **Testing Challenges**
- Cannot test generators independently
- Cannot test sync/burst generation separately from encoding
- Cannot test field operations without full encode pipeline

### 6. **No Intermediate Field Representation**
Fields go from YUV data → fully encoded composite in one step. No way to:
- Inspect/modify fields mid-pipeline
- Add noise/dropouts to specific lines
- Replace specific regions (like VBI) programmatically

---

## Proposed Refactored Architecture

### Core Principle: **Pipeline Pattern with Composable Stages**

Each stage in the pipeline:
1. Has a **clear input and output type**
2. Is **independently testable**
3. Can be **composed** with other stages
4. Has **minimal dependencies** on other stages

### Refactored Pipeline

```
┌──────────────────────────────────────────────────────────────────┐
│                        VIDEO PIPELINE                             │
└──────────────────────────────────────────────────────────────────┘

┌─────────────────┐
│  1. LOADER      │  FrameSource → FrameBuffer (YUV444P16)
│  STAGE          │  
└────────┬────────┘  Classes: VideoLoader (abstract)
         │            - YUV422Loader
         │            - PNGLoader  
         │            - MOVLoader
         │            - MP4Loader
         ↓
┌─────────────────┐
│  2. FIELD       │  FrameBuffer → FieldPair (interlaced split)
│  SPLITTER       │
└────────┬────────┘  Classes: FieldSplitter
         │            - split_frame(FrameBuffer) → FieldPair
         ↓
┌─────────────────┐
│  3. FIELD       │  FieldPair → ProcessedFieldPair
│  PREPROCESSOR   │  (normalize, filter, prepare for encoding)
└────────┬────────┘  Classes: FieldPreprocessor
         │            - ChromaFilter
         │            - LumaFilter
         │            - FieldNormalizer
         ↓
┌─────────────────┐
│  4. STRUCTURE   │  ProcessedFieldPair → StructuredFieldPair
│  GENERATOR      │  (add sync, blanking, mark VBI region)
└────────┬────────┘  Classes: FieldStructureGenerator
         │            (handles sync pulses, blanking, vsync,
         │             and marks VBI lines for metadata insertion)
         ↓
┌─────────────────┐
│  5. METADATA    │  StructuredFieldPair → EnrichedFieldPair
│  GENERATORS     │  (add VBI data, VITC, VITS)
└────────┬────────┘  Classes: MetadataGenerator (abstract)
         │            - BiphaseVBIGenerator (lines 16-18)
         │            - VITCGenerator (lines 19, 21)
         │            - VITSGenerator (test signals)
         │            - ColorBurstGenerator
         ↓
┌─────────────────┐
│  6. ACTIVE      │  EnrichedFieldPair → EncodedFieldPair
│  VIDEO ENCODER  │  (encode active video lines with subcarrier)
└────────┬────────┘  Classes: ActiveVideoEncoder
         │            - PALActiveEncoder
         │            - NTSCActiveEncoder
         ↓
┌─────────────────┐
│  7. FIELD       │  ** NEW STAGE FOR FUTURE **
│  EFFECTS        │  EncodedFieldPair → EffectedFieldPair
│  (FUTURE)       │  (add noise, dropouts, artifacts)
└────────┬────────┘  Classes: FieldEffect (abstract)
         │            - NoiseGenerator
         │            - DropoutSimulator
         │            - PhaseErrorSimulator
         ↓
┌─────────────────┐
│  8. WRITER      │  EncodedFieldPair → File
│  STAGE          │
└─────────────────┘  Classes: Writer (abstract)
                      - TBCWriter
                      - StandardWriter
                      - YCTBCWriter
                      - MetadataWriter
```

---

## Detailed Refactoring Design

### 1. **Unified Loader Interface**

**Problem**: Loaders have inconsistent interfaces.

**Solution**: Standardize on `VideoLoader` abstract base class with consistent methods.

```cpp
class VideoLoader {
public:
    virtual ~VideoLoader() = default;
    
    // Open video source
    virtual bool open(const std::string& path, std::string& error) = 0;
    
    // Get video metadata
    virtual VideoMetadata get_metadata() const = 0;
    
    // Load a single frame (0-indexed)
    virtual bool load_frame(int32_t frame_index, FrameBuffer& output, std::string& error) = 0;
    
    // Load multiple frames (batch operation)
    virtual bool load_frames(int32_t start, int32_t count, 
                             std::vector<FrameBuffer>& output, std::string& error) = 0;
    
    // Close and cleanup
    virtual void close() = 0;
};

struct VideoMetadata {
    int32_t width;
    int32_t height;
    int32_t frame_count;
    double frame_rate;
    VideoColorSpace color_space;  // YUV422, RGB, etc.
    VideoBitDepth bit_depth;      // 8-bit, 10-bit, etc.
};
```

**Benefits**:
- Consistent interface across all loaders
- Metadata query separate from loading
- Easy to add new loader types
- Testable with mock implementations

---

### 2. **Field Splitter (New Component)**

**Purpose**: Separate frame → field splitting from encoding logic.

```cpp
class FieldSplitter {
public:
    struct FieldPair {
        Field field1;  // Even lines (0, 2, 4, ...)
        Field field2;  // Odd lines (1, 3, 5, ...)
        int32_t field_number;  // Starting field number for pair
    };
    
    // Split progressive frame into interlaced fields
    FieldPair split_frame(const FrameBuffer& frame, 
                          int32_t field_number,
                          const VideoParameters& params);
    
    // Merge fields back into frame (for testing/validation)
    FrameBuffer merge_fields(const FieldPair& fields);
};
```

**Benefits**:
- Clear separation of concerns
- Can test field splitting independently
- Easier to support alternative interlacing patterns

---

### 3. **Field Structure Generator (New Component)**

**Purpose**: Create the basic structure of a video field (sync, blanking, VBI placeholders) **before** adding metadata or encoding active video.

```cpp
class FieldStructureGenerator {
public:
    struct StructuredField {
        Field field_data;          // Raw sample data
        LineMap line_types;        // What type each line is (VBI, active, blanking)
        Range active_video_range;  // Which lines contain active video
        Range vbi_range;           // Which lines are VBI (for metadata insertion)
    };
    
    // Create complete field structure with sync, blanking, and VBI region marked
    StructuredField create_field_structure(
        const Field& source_field,
        bool is_first_field,
        const VideoParameters& params,
        VideoSystem system);
    
private:
    void add_sync_pulses(Field& field, ...);
    void add_blanking_lines(Field& field, ...);
    void add_vsync_pattern(Field& field, ...);
    void mark_vbi_region(LineMap& line_types, ...);  // Mark VBI lines in LineMap
};
```

**Benefits**:
- Sync/blanking generation separated from active video encoding
- Easier to test sync pulse generation independently
- Line structure explicitly represented (LineMap)
- VBI lines clearly marked for later metadata insertion (stage 5)
- Single component handles all field structure concerns

---

### 4. **Metadata Generators (Refactored)**

**Problem**: Generators embedded in encoders.

**Solution**: Make generators **standalone pipeline stages** that operate on `StructuredField`.

```cpp
// Abstract base for all metadata generators
class MetadataGenerator {
public:
    virtual ~MetadataGenerator() = default;
    
    // Add metadata to structured field
    virtual void apply(StructuredField& field, 
                      const MetadataContext& context) = 0;
    
    // Which lines does this generator affect?
    virtual std::vector<int32_t> affected_lines() const = 0;
};

// Concrete implementations
class BiphaseVBIGenerator : public MetadataGenerator {
public:
    void apply(StructuredField& field, const MetadataContext& context) override {
        // Add biphase data to lines 16, 17, 18
        if (context.vbi_data) {
            encode_biphase_on_line(field, 15, context.vbi_data->vbi0);
            encode_biphase_on_line(field, 16, context.vbi_data->vbi1);
            encode_biphase_on_line(field, 17, context.vbi_data->vbi2);
        }
    }
    
    std::vector<int32_t> affected_lines() const override {
        return {15, 16, 17};  // 0-indexed
    }
};

class VITCGenerator : public MetadataGenerator {
    // Add VITC to lines 19 and 21 for consumer tape
};

class VITSGenerator : public MetadataGenerator {
    // Add VITS test signals to designated lines
};

class ColorBurstGenerator : public MetadataGenerator {
    // Add color burst reference to all active/VBI lines
};
```

**Usage**:
```cpp
// Build a metadata pipeline
std::vector<std::unique_ptr<MetadataGenerator>> metadata_pipeline;
if (source_standard_supports_vbi(standard)) {
    metadata_pipeline.push_back(std::make_unique<BiphaseVBIGenerator>());
}
if (source_standard_supports_vitc(standard)) {
    metadata_pipeline.push_back(std::make_unique<VITCGenerator>());
}
if (source_standard_supports_vits(standard)) {
    metadata_pipeline.push_back(std::make_unique<VITSGenerator>());
}
metadata_pipeline.push_back(std::make_unique<ColorBurstGenerator>());

// Apply all generators
for (auto& generator : metadata_pipeline) {
    generator->apply(structured_field, metadata_context);
}
```

**Benefits**:
- Generators are **composable** and **optional**
- Easy to add/remove generators
- Clear dependencies and ordering
- Each generator independently testable
- Easy to add new metadata types

---

### 5. **Active Video Encoder (Refactored)**

**Problem**: PALEncoder and NTSCEncoder are monolithic and duplicate code.

**Solution**: Extract active video encoding into separate, focused classes.

```cpp
// Abstract base for active video encoding
class ActiveVideoEncoder {
public:
    virtual ~ActiveVideoEncoder() = default;
    
    // Encode active video lines (with subcarrier modulation)
    virtual void encode_active_video(
        StructuredField& field,
        const Field& source_yuv_field,
        const VideoParameters& params) = 0;
};

// PAL implementation
class PALActiveEncoder : public ActiveVideoEncoder {
public:
    void encode_active_video(
        StructuredField& field,
        const Field& source_yuv_field,
        const VideoParameters& params) override;
        
private:
    void encode_line_with_pal_subcarrier(
        uint16_t* line_buffer,
        const uint16_t* y_line,
        const uint16_t* u_line,
        const uint16_t* v_line,
        int32_t line_number,
        int32_t field_number,
        const VideoParameters& params);
        
    int32_t calculate_v_switch(int32_t field_number, int32_t line_number);
    double calculate_pal_phase(int32_t field_number, int32_t line_number);
};

// NTSC implementation
class NTSCActiveEncoder : public ActiveVideoEncoder {
    // Similar but with YIQ and different phase calculation
};

// Separate Y/C encoder (no subcarrier modulation)
class SeparateYCEncoder : public ActiveVideoEncoder {
    // Outputs separate luma and chroma fields
};
```

**Benefits**:
- **Single responsibility**: Only handles active video encoding
- PAL and NTSC share no common implementation (clear separation)
- Can test subcarrier modulation independently
- Easy to add new encoding standards (e.g., SECAM)

---

### 6. **Unified Encoder with Builder Pattern**

**Problem**: Current PALEncoder/NTSCEncoder do too much.

**Solution**: Create a **VideoEncoder** that orchestrates the pipeline using **composition**.

```cpp
class VideoEncoderPipeline {
public:
    // Builder pattern for configuration
    class Builder {
    public:
        Builder& set_system(VideoSystem system);
        Builder& set_source_standard(SourceVideoStandard standard);
        Builder& enable_chroma_filter(bool enable);
        Builder& enable_luma_filter(bool enable);
        Builder& add_metadata_generator(std::unique_ptr<MetadataGenerator> gen);
        
        VideoEncoderPipeline build();
    };
    
    // Encode a single frame through the entire pipeline
    EncodedFieldPair encode_frame(const FrameBuffer& frame, 
                                  int32_t field_number,
                                  const MetadataContext& metadata);
    
private:
    std::unique_ptr<FieldSplitter> splitter_;
    std::unique_ptr<FieldStructureGenerator> structure_gen_;
    std::vector<std::unique_ptr<MetadataGenerator>> metadata_generators_;
    std::unique_ptr<ActiveVideoEncoder> active_encoder_;
    VideoParameters params_;
};
```

**Usage**:
```cpp
// Build encoder pipeline
auto encoder = VideoEncoderPipeline::Builder()
    .set_system(VideoSystem::PAL)
    .set_source_standard(SourceVideoStandard::LASERDISC_CAV)
    .enable_chroma_filter(true)
    .add_metadata_generator(std::make_unique<BiphaseVBIGenerator>())
    .add_metadata_generator(std::make_unique<VITSGenerator>())
    .add_metadata_generator(std::make_unique<ColorBurstGenerator>())
    .build();

// Encode frames
for (int32_t i = 0; i < num_frames; i++) {
    auto encoded_fields = encoder.encode_frame(frame, i * 2, metadata_ctx);
    writer.write_field(encoded_fields.field1);
    writer.write_field(encoded_fields.field2);
}
```

**Benefits**:
- **Flexible configuration** via builder
- Clear pipeline stages
- Easy to add/remove processing stages
- Testable at each stage
- Future-proof for field effects (noise, dropouts)

---

### 7. **Field Effects Stage (Future Addition)**

**Purpose**: Add noise, dropouts, and artifacts to encoded fields.

```cpp
class FieldEffect {
public:
    virtual ~FieldEffect() = default;
    
    // Apply effect to encoded field
    virtual void apply(Field& field, 
                      const FieldEffectContext& context) = 0;
};

class NoiseGenerator : public FieldEffect {
public:
    NoiseGenerator(double noise_level_db) : noise_level_(noise_level_db) {}
    
    void apply(Field& field, const FieldEffectContext& context) override {
        // Add Gaussian noise to all samples
        for (auto& sample : field.data()) {
            sample = add_noise(sample, noise_level_);
        }
    }
    
private:
    double noise_level_;
};

class DropoutSimulator : public FieldEffect {
public:
    void apply(Field& field, const FieldEffectContext& context) override {
        // Simulate tape dropouts (set lines to blanking)
        for (auto line : dropout_lines_) {
            std::fill_n(field.line_data(line), field.width(), blanking_level_);
        }
    }
    
private:
    std::vector<int32_t> dropout_lines_;
};
```

**Integration**:
```cpp
auto encoder = VideoEncoderPipeline::Builder()
    .set_system(VideoSystem::PAL)
    .add_field_effect(std::make_unique<NoiseGenerator>(-40.0))  // -40 dB SNR
    .add_field_effect(std::make_unique<DropoutSimulator>())
    .build();
```

**Benefits**:
- Effects applied **after encoding** (realistic simulation)
- Can chain multiple effects
- Easy to add new effect types (head switching noise, head clog, etc.)
- Effects isolated from encoding logic

---

## Refactoring Strategy (Phased Approach)

### Phase 1: **Standardize Loaders** (Low Risk)
1. Create unified `VideoLoader` interface
2. Refactor all existing loaders to implement it
3. Update all code to use new interface
4. Remove old loader implementations

**Benefits**: Immediate improvement in loader consistency, easier testing, cleaner API

---

### Phase 2: **Extract Field Splitter** (Low Risk)
1. Create `FieldSplitter` class
2. Move frame → field splitting logic from encoders
3. Update encoders to accept pre-split fields
4. Remove old field splitting code from encoders

**Benefits**: Cleaner separation, easier to test field splitting, simpler encoder code

---

### Phase 3: **Extract Structure Generation** (Medium Risk)
1. Create `FieldStructureGenerator` class
2. Move sync pulse, blanking, vsync generation from encoders
3. Update encoders to work with `StructuredField`
4. Remove old sync/blanking generation code

**Benefits**: Sync generation testable independently, clearer field structure, reduced encoder complexity

---

### Phase 4: **Refactor Metadata Generators + New YAML Config** (Medium Risk)
1. Extract `VITCGenerator`, `VITSGenerator`, `ColorBurstGenerator` from encoders
2. Make them standalone classes implementing `MetadataGenerator`
3. Update pipeline to apply generators sequentially
4. **YAML Changes**: Implement new `pipeline.metadata.generators` configuration
5. Remove old `laserdisc.standard` format entirely
6. Provide migration script to convert old YAML files to new format

**Benefits**: Composable metadata pipeline, easy to add/remove metadata, explicit configuration, cleaner YAML structure

**YAML Migration**:
- Convert projects in test-projects/ to new format
- Old format no longer supported - clean break
- Updated documentation with new YAML examples

---

### Phase 5: **Refactor Active Video Encoding** (High Risk)
1. Create `ActiveVideoEncoder` abstract base
2. Implement `PALActiveEncoder` and `NTSCActiveEncoder`
3. Move subcarrier modulation and YUV encoding to active encoders
4. Create `VideoEncoderPipeline` class with Builder pattern
5. Replace old `PALEncoder`/`NTSCEncoder` classes entirely
6. Builder pattern consumes new YAML configuration (via `PipelineConfigLoader`)

**Benefits**: Smallest, most testable components, clear separation of concerns, builder pattern enables flexible configuration

---

### Phase 6: **Add Field Effects Stage** (Future)
1. Create `FieldEffect` abstract base
2. Implement `NoiseGenerator`, `DropoutSimulator`
3. Integrate into pipeline **after** active video encoding
4. **YAML Changes**: Add `pipeline.effects` configuration section
5. Add `pipeline.preprocessing.filters` for explicit filter control

**Benefits**: Realistic noise/dropout simulation for testing decoders, fully explicit pipeline configuration, complete composable pipeline

---

## Testing Strategy

### Current Testing Gaps
- No unit tests for individual encoding stages
- Integration tests only (full encode → compare output)
- Hard to diagnose failures (which stage caused the problem?)

### Proposed Testing Approach

#### 1. **Unit Tests for Each Component**
```cpp
// Test loader independently
TEST(YUV422Loader, LoadsSingleFrame) {
    YUV422Loader loader;
    ASSERT_TRUE(loader.open("test.yuv", error));
    FrameBuffer frame;
    ASSERT_TRUE(loader.load_frame(0, frame, error));
    EXPECT_EQ(frame.width(), 720);
    EXPECT_EQ(frame.height(), 576);
}

// Test field splitter
TEST(FieldSplitter, SplitsFrameCorrectly) {
    FrameBuffer frame = create_test_frame();
    FieldSplitter splitter;
    auto fields = splitter.split_frame(frame, 0, params);
    EXPECT_EQ(fields.field1.height(), 312);
    EXPECT_EQ(fields.field2.height(), 313);
}

// Test sync generator
TEST(FieldStructureGenerator, GeneratesSyncPulses) {
    Field field = create_blank_field();
    FieldStructureGenerator gen;
    auto structured = gen.create_field_structure(field, true, params, VideoSystem::PAL);
    // Verify sync pulse amplitude and timing
    EXPECT_EQ(structured.field_data.get_sample(0, 0), 0);  // Sync tip
}

// Test VITC generator
TEST(VITCGenerator, EncodesTimecodeCorrectly) {
    StructuredField field = create_structured_field();
    VITCGenerator gen;
    MetadataContext ctx{.total_frame = 100};
    gen.apply(field, ctx);
    // Verify VITC waveform on line 19
    auto vitc_bits = decode_vitc_from_line(field.field_data.line_data(18));
    EXPECT_EQ(vitc_bits.frames, 0);
    EXPECT_EQ(vitc_bits.seconds, 4);
}
```

#### 2. **Integration Tests for Pipeline**
```cpp
TEST(VideoEncoderPipeline, EncodesFrameEndToEnd) {
    auto encoder = VideoEncoderPipeline::Builder()
        .set_system(VideoSystem::PAL)
        .build();
    
    FrameBuffer frame = load_test_frame();
    auto encoded = encoder.encode_frame(frame, 0, metadata_ctx);
    
    // Verify output characteristics
    EXPECT_EQ(encoded.field1.width(), 1135);
    EXPECT_EQ(encoded.field1.height(), 312);
    // Verify sync pulses, burst, etc.
}
```

#### 3. **Regression Tests**
```cpp
// Ensure refactored code produces identical output
TEST(RegressionTest, RefactoredOutputMatchesOriginal) {
    auto original = legacy_pal_encoder.encode_frame(frame, 0);
    auto refactored = new_pipeline.encode_frame(frame, 0);
    
    EXPECT_TRUE(fields_are_identical(original.field1, refactored.field1));
    EXPECT_TRUE(fields_are_identical(original.field2, refactored.field2));
}
```

---

## Code Reuse Opportunities

### 1. **Common Sync/Blanking Generation**
Currently duplicated in PALEncoder and NTSCEncoder. Extract to shared `SyncGenerator`:

```cpp
class SyncGenerator {
public:
    static void generate_hsync(uint16_t* line, int32_t width, 
                               int32_t sync_level, double sample_rate);
    static void generate_vsync(Field& field, int32_t sync_level, 
                               VideoSystem system);
    static void generate_blanking(uint16_t* line, int32_t width, 
                                  int32_t blanking_level);
};
```

### 2. **Color Space Conversion Utilities**
Already partially done with `VideoLoaderUtils`. Expand to cover:
- RGB → YUV conversion (PNG loader)
- YUV422 → YUV444 upsampling (all loaders)
- Studio range normalization (all loaders)

### 3. **Subcarrier Modulation**
Extract common phase calculation and modulation logic:

```cpp
class SubcarrierModulator {
public:
    virtual double calculate_phase(int32_t field, int32_t line, int32_t sample) = 0;
    virtual void modulate_yuv_to_composite(/* ... */) = 0;
};

class PALModulator : public SubcarrierModulator {
    // PAL-specific phase calculation (8-field sequence, V-switch)
};

class NTSCModulator : public SubcarrierModulator {
    // NTSC-specific phase calculation (4-field sequence, no V-switch)
};
```

---

## Benefits of Refactored Architecture

### 1. **Maintainability**
- **Single Responsibility Principle**: Each class has one job
- **Clear dependencies**: Easy to understand data flow
- **Smaller classes**: Easier to understand and modify

### 2. **Testability**
- **Unit tests** for each component
- **Mock implementations** for testing (e.g., MockVideoLoader)
- **Regression tests** ensure refactoring doesn't break output

### 3. **Extensibility**
- **Add new metadata** by implementing `MetadataGenerator`
- **Add field effects** by implementing `FieldEffect`
- **Add new video systems** (e.g., SECAM) by implementing `ActiveVideoEncoder`
- **Add new file formats** by implementing `VideoLoader`

### 4. **Code Reuse**
- Sync generation shared between PAL/NTSC
- Color space conversion shared across loaders
- Field splitting logic reused for all encoders

### 5. **Future-Proof**
- **Noise/dropout simulation** slots in as a pipeline stage
- **Advanced filtering** can be added as preprocessor stage
- **Custom encoders** can be built by composing stages differently

---

## Migration Approach

### Clean Break Strategy
Since backward compatibility is not required, we can implement a **clean break** approach:

1. **Complete each phase fully** - remove old code as new code is validated
2. **Update all existing YAML projects** - provide migration tool to convert
3. **Comprehensive testing** - ensure output remains bit-identical where expected
4. **Single cutover** - no need to maintain parallel implementations

### YAML Migration Tool
```bash
# Convert old format to new pipeline-based format
encode-orc-migrate old-project.yaml new-project.yaml

# Batch convert all projects in directory
encode-orc-migrate --batch ./projects/*.yaml --output ./migrated/
```

Migration tool will:
- Parse old `laserdisc.standard` settings
- Generate explicit `pipeline.metadata.generators` configuration
- Preserve all section definitions and output settings
- Add comments explaining changes

### Validation Strategy
1. Run old version on test projects → save output
2. Migrate YAML projects to new format
3. Run new version on migrated projects → compare output
4. Verify bit-identical results (or document intentional differences)

---

## Summary

### Current State
- Monolithic encoders doing too many jobs
- Generators embedded in encoders (tight coupling)
- Code duplication between PAL/NTSC
- Difficult to add field-level operations (noise, dropouts)
- Limited testability

### Proposed State
- **Clear pipeline stages**: Loader → Splitter → Structure → Metadata → Encoding → Effects → Writer
- **Composable components**: Mix and match generators, effects, encoders
- **Single responsibility**: Each class has one job
- **Testable**: Unit tests for every stage
- **Extensible**: Easy to add new metadata, effects, formats

### Key Changes
1. Standardize loaders with `VideoLoader` interface
2. Extract `FieldSplitter` for frame → field conversion
3. Extract `FieldStructureGenerator` for sync/blanking
4. Make metadata generators **standalone pipeline stages**
5. Simplify encoders to **only handle active video**
6. Introduce `VideoEncoderPipeline` orchestrator
7. Add `FieldEffect` stage for noise/dropout simulation

### Benefits
- ✅ Better code organization and clarity
- ✅ Easier testing and debugging
- ✅ Reduced code duplication
- ✅ Ready for noise/dropout simulation
- ✅ Easy to add new features
- ✅ Maintains backward compatibility during migration

---

## YAML Configuration Design

### Current YAML Structure

The existing YAML format defines:
```yaml
name: "Project Name"
output:
  format: "pal-composite"  # or "ntsc-composite", "pal-yc", etc.
  filename: "output.tbc"
  writer: "tbc"  # or "standard"

laserdisc:
  standard: "iec60857"  # or "consumer-tape", "none"

sections:
  - name: "Section 1"
    yuv422_image_source:
      file: "image.yuv"
    duration: 100
```

### Problems with Current Configuration

1. **Implicit pipeline configuration**: Metadata generators (VITS, VITC, VBI) are enabled/disabled based on `laserdisc.standard`, not explicitly configured
2. **No generator customization**: Cannot specify which VITS signals to use, or customize line placement
3. **No filter configuration**: Chroma/luma filters are hardcoded or passed as function parameters
4. **No extensibility**: Cannot add custom pipeline stages or effects
5. **Tightly coupled**: `laserdisc.standard` controls multiple independent features (VBI, VITS, VITC)

### Proposed YAML Structure

**Principle**: Make the pipeline configuration **explicit and composable** while maintaining sensible defaults.

```yaml
# Global project settings
name: "My Video Project"
description: "Test video with VITS and VBI"

# Video system (affects all pipeline stages)
video:
  system: "pal"  # or "ntsc"
  format: "composite"  # or "separate-yc"
  
# Pipeline configuration
pipeline:
  # Stage 3: Field preprocessing
  preprocessing:
    filters:
      chroma:
        enabled: true
        type: "pal-1.3mhz"  # or "ntsc-600khz", "custom"
      luma:
        enabled: false
        type: "pal-5.5mhz"
  
  # Stage 4: Structure generation (usually no config needed)
  structure:
    # Could add custom sync pulse parameters here if needed
    
  # Stage 5: Metadata generators (composable list)
  metadata:
    generators:
      # VBI for LaserDisc
      - type: "biphase-vbi"
        enabled: true
        lines: [16, 17, 18]  # 1-indexed field lines
        
      # VITC for consumer tape
      - type: "vitc"
        enabled: false
        lines: [19, 21]  # Standard VITC lines
        start_frame_offset: 0
        
      # VITS test signals
      - type: "vits"
        enabled: true
        signals:
          - line: 13
            field: 1
            signal: "multiburst"
          - line: 19
            field: 1
            signal: "uk-national"
          - line: 13
            field: 2
            signal: "itu-its"
          - line: 19
            field: 2
            signal: "itu-composite"
      
      # Color burst (always needed for composite)
      - type: "color-burst"
        enabled: true
        # Could add phase/amplitude overrides
  
  # Stage 6: Active video encoding (usually auto-configured)
  encoding:
    # PAL/NTSC specific parameters if needed
    
  # Stage 7: Field effects (future)
  effects:
    - type: "noise"
      enabled: false
      level_db: -40.0
      
    - type: "dropout"
      enabled: false
      pattern: "random"
      density: 0.01  # 1% of lines

# Output configuration
output:
  filename: "output.tbc"
  writer: "tbc"  # or "standard"
  
  # Writer-specific options
  options:
    separate_yc: false
    yc_legacy_naming: false
    field1_padding: true

# Video sections (sources)
sections:
  - name: "Test Card"
    yuv422_image_source:
      file: "testcard.yuv"
    duration: 100
    
    # Per-section VBI data (CAV/CLV metadata)
    vbi:
      mode: "cav"  # or "clv-chapter", "clv-timecode", "none"
      picture_start: 1000
      # OR for CLV:
      # mode: "clv-timecode"
      # timecode_start: "00:00:00:00"
```

### Configuration Hierarchy and Defaults

#### 1. **Global Video Settings** (Required)
```yaml
video:
  system: "pal"  # Affects all stages (sync timing, subcarrier freq, etc.)
  format: "composite"  # Affects encoding stage
```

These are **global** because:
- Video system determines field dimensions, line count, frame rate
- Affects structure generation (PAL vs NTSC sync patterns)
- Affects active video encoding (PAL V-switch vs NTSC phase)
- Affects metadata generators (VITS signal parameters)

#### 2. **Pipeline Stage Configuration** (Optional with Smart Defaults)

**Default behavior** based on common use cases:

```cpp
// Smart defaults based on format and common standards
if (video.format == "composite" && section.vbi.mode == "cav") {
    // LaserDisc CAV: Enable VBI and VITS
    enable_generator("biphase-vbi");
    enable_generator("vits");
    enable_generator("color-burst");
} else if (video.format == "composite" && section.vbi.mode == "clv-timecode") {
    // LaserDisc CLV: Enable VBI and VITS
    enable_generator("biphase-vbi");
    enable_generator("vits");
    enable_generator("color-burst");
} else if (video.format == "composite") {
    // Consumer tape or other: Enable VITC
    enable_generator("vitc");
    enable_generator("color-burst");
} else if (video.format == "separate-yc") {
    // Separate Y/C: Only color burst on chroma
    enable_generator("color-burst");
}
```

**But allow explicit override**:
```yaml
pipeline:
  metadata:
    generators:
      - type: "vitc"
        enabled: true  # Force enable even for LaserDisc format
        lines: [14, 16]  # Non-standard line placement
```

#### 3. **Template System** (Convenience)

Provide **named templates** for common configurations:

```yaml
# Use a template for quick setup
pipeline:
  template: "laserdisc-cav"  # Predefined template
  
  # Override specific parts
  metadata:
    generators:
      - type: "vitc"
        enabled: true  # Add VITC to LaserDisc (non-standard)
```

**Built-in templates**:
- `laserdisc-cav` - VBI + VITS for CAV LaserDisc
- `laserdisc-clv` - VBI + VITS for CLV LaserDisc
- `consumer-tape` - VITC for consumer tape formats
- `minimal` - Just color burst, no metadata
- `custom` - No defaults, explicit configuration required

### Pipeline Builder from YAML

```cpp
class PipelineConfigLoader {
public:
    static VideoEncoderPipeline build_from_yaml(const YAMLConfig& config) {
        auto builder = VideoEncoderPipeline::Builder();
        
        // 1. Set video system (global)
        builder.set_system(parse_video_system(config.video.system));
        
        // 2. Apply template if specified (provides defaults)
        if (config.pipeline.template_name) {
            apply_template(builder, config.pipeline.template_name);
        }
        
        // 3. Configure filters (stage 3)
        if (config.pipeline.preprocessing.filters.chroma.enabled) {
            builder.enable_chroma_filter(true);
        }
        
        // 4. Add metadata generators (stage 5)
        for (const auto& gen_config : config.pipeline.metadata.generators) {
            if (gen_config.enabled) {
                auto generator = create_generator(gen_config);
                builder.add_metadata_generator(std::move(generator));
            }
        }
        
        // 5. Add effects (stage 7 - future)
        for (const auto& effect_config : config.pipeline.effects) {
            if (effect_config.enabled) {
                auto effect = create_effect(effect_config);
                builder.add_field_effect(std::move(effect));
            }
        }
        
        return builder.build();
    }
    
private:
    static void apply_template(Builder& builder, const std::string& template_name) {
        if (template_name == "laserdisc-cav") {
            builder.add_metadata_generator(std::make_unique<BiphaseVBIGenerator>());
            builder.add_metadata_generator(std::make_unique<VITSGenerator>());
            builder.add_metadata_generator(std::make_unique<ColorBurstGenerator>());
        } else if (template_name == "consumer-tape") {
            builder.add_metadata_generator(std::make_unique<VITCGenerator>());
            builder.add_metadata_generator(std::make_unique<ColorBurstGenerator>());
        }
        // etc.
    }
};
```

### Generator-Specific Configuration

Each generator type can have its own configuration structure:

```cpp
struct VITCGeneratorConfig {
    std::vector<int32_t> lines = {19, 21};  // Default VITC lines
    int32_t start_frame_offset = 0;
};

struct VITSGeneratorConfig {
    struct Signal {
        int32_t line;
        int32_t field;  // 1 or 2
        std::string type;  // "multiburst", "itu-composite", etc.
    };
    std::vector<Signal> signals;
};

class VITSGenerator : public MetadataGenerator {
public:
    VITSGenerator(const VITSGeneratorConfig& config) : config_(config) {}
    
    void apply(StructuredField& field, const MetadataContext& context) override {
        // Generate only configured VITS signals
        for (const auto& signal : config_.signals) {
            if (matches_current_field(signal, context)) {
                generate_vits_signal(field, signal.line, signal.type);
            }
        }
    }
};
```

### Migration from Old YAML Format

Old YAML format is **not supported** - projects must be migrated:

```yaml
# OLD FORMAT (no longer supported):
laserdisc:
  standard: "iec60857"

# NEW FORMAT (required):
pipeline:
  template: "laserdisc-cav"
  # OR explicit configuration:
  metadata:
    generators:
      - type: "biphase-vbi"
        enabled: true
      - type: "vits"
        enabled: true
```

Use migration tool: `encode-orc-migrate old.yaml new.yaml`

### Example: Fully Configured Project

```yaml
name: "Advanced Test Project"

video:
  system: "pal"
  format: "composite"

pipeline:
  preprocessing:
    filters:
      chroma:
        enabled: true
        type: "pal-1.3mhz"
      luma:
        enabled: true
        type: "pal-5.5mhz"
        
  metadata:
    generators:
      # VBI on standard lines
      - type: "biphase-vbi"
        enabled: true
        lines: [16, 17, 18]
        
      # VITS on specific lines
      - type: "vits"
        enabled: true
        signals:
          - { line: 13, field: 1, signal: "multiburst" }
          - { line: 19, field: 1, signal: "uk-national" }
          
      # Color burst
      - type: "color-burst"
        enabled: true
        
  effects:
    # Add noise simulation
    - type: "noise"
      enabled: true
      level_db: -35.0
      distribution: "gaussian"
      
    # Random dropouts
    - type: "dropout"
      enabled: true
      pattern: "random"
      seed: 42
      density: 0.005  # 0.5% of lines

output:
  filename: "test-output.tbc"
  writer: "tbc"

sections:
  - name: "Test Pattern"
    yuv422_image_source:
      file: "testcard.yuv"
    duration: 100
    vbi:
      mode: "cav"
      picture_start: 1000
```

### Benefits of New YAML Design

1. **Explicit Configuration**: Clear what pipeline stages are active
2. **Composable**: Mix and match generators, effects, filters
3. **Extensible**: Easy to add new generator/effect types without changing schema
4. **Testable**: Can create minimal configs for testing specific stages
5. **No Legacy Baggage**: Clean, modern design without backward compatibility constraints

### YAML Format Timeline

**Integrated with Code Refactoring Phases**:

| Code Phase | YAML Changes | Migration |
|------------|--------------|----------|
| **Phase 1-3** | No YAML changes needed | N/A |
| **Phase 4** | Implement new `pipeline.metadata.generators` | **Migrate all projects** |
| **Phase 5** | Builder consumes new YAML format | Already migrated |
| **Phase 6** | Add `pipeline.effects`, `pipeline.preprocessing` | Update YAML schema |

**Phase 4 Implementation**:
```cpp
// Only new format supported
if (!config.has("pipeline.metadata.generators")) {
    throw std::runtime_error(
        "Old YAML format not supported. "
        "Use 'encode-orc-migrate' to convert your project.");
}

apply_generator_config(builder, config.pipeline.metadata.generators);
```

**Migration Tool**:
```bash
# Convert old to new format
encode-orc-migrate old-project.yaml new-project.yaml

# Batch convert directory
encode-orc-migrate --batch projects/*.yaml --output migrated/

# Validate new format
encode-orc-migrate --validate new-project.yaml
```

---

## Questions for Discussion

1. **Priority**: Which phase should we tackle first?
2. **Testing**: Should we write tests before or during refactoring?
3. **Breaking changes**: Is it acceptable to change the API for `VideoEncoder`?
4. **Performance**: Are there concerns about performance with the pipeline approach?
5. **Timeline**: What's a reasonable timeline for each phase?
6. **YAML Design**: Should we support both old and new YAML formats indefinitely, or deprecate the old format?
7. **Configuration Complexity**: Is the proposed YAML structure too complex for simple use cases?

---

**Document Version**: 1.1  
**Date**: 2026-01-29  
**Author**: Architecture Analysis based on encode-orc codebase
