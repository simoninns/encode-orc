# Multi-Threading Implementation Plan for encode-orc

## Executive Summary

This document outlines a phased approach to add multi-threading support to encode-orc to significantly improve encoding performance. The current implementation processes frames sequentially in a single thread, leaving most CPU cores idle during encoding.

**Expected Performance Gains**: 6-16x overall speedup on 8-core systems

## Current Architecture Analysis

### Sequential Processing Pipeline

The encoder currently uses a single-threaded pipeline:

1. **Frame Loading** → Load frame from source (YUV422/PNG/MOV/MP4)
2. **Field Encoding** → Encode field1 and field2 sequentially
3. **Output Writing** → Write encoded fields to disk
4. **Metadata Collection** → Gather dropout/VBI metadata

**Location**: [src/main.cpp](src/main.cpp#L900-L1146)

### Encoding Stages (Per Field)

Each field goes through:
1. Field splitting (progressive → interlaced)
2. Structure generation (sync, blanking, color burst)
3. Metadata generation (VITC, VITS, VBI)
4. Active video encoding (YUV → composite with subcarrier modulation)
5. Effects application (noise, dropouts)

**Location**: [src/pipeline/orchestrator/video_encoder_pipeline.cpp](src/pipeline/orchestrator/video_encoder_pipeline.cpp#L142-L330)

### Parallelization Opportunities

✅ **Frames are independent** - No data dependencies between frames  
✅ **Thread-local buffers already used** - Filter buffers use `thread_local`  
✅ **Clean architecture** - Value semantics, no shared mutable state  
⚠️ **Sequential output required** - Fields must be written in order  
⚠️ **Metadata synchronization needed** - Dropout collection needs protection

## Phase 1: Frame-Level Parallelism

**Priority**: HIGH  
**Impact**: 4-8x speedup  
**Complexity**: Medium

### Architecture

```
┌──────────────┐     ┌─────────────┐     ┌──────────────┐     ┌──────────────┐
│ Loader       │────▶│ Input Queue │────▶│ Worker Pool  │────▶│ Output Queue │────▶ Writer
│ Thread       │     │ (bounded)   │     │ (N threads)  │     │ (ordered)    │     Thread
└──────────────┘     └─────────────┘     └──────────────┘     └──────────────┘
```

### Components to Add

#### 1. Thread Pool (`src/utils/thread_pool.h`)

```cpp
class ThreadPool {
public:
    ThreadPool(size_t num_threads);
    ~ThreadPool();
    
    template<typename F>
    std::future<typename std::result_of<F()>::type> enqueue(F&& f);
    
    void wait_all();
    
private:
    std::vector<std::thread> workers_;
    std::queue<std::function<void()>> tasks_;
    std::mutex queue_mutex_;
    std::condition_variable condition_;
    bool stop_;
};
```

#### 2. Ordered Output Queue (`src/utils/ordered_queue.h`)

```cpp
template<typename T>
class OrderedQueue {
public:
    void push(size_t index, T&& item);
    bool try_pop(T& item);  // Only pops next expected index
    
private:
    std::map<size_t, T> buffer_;
    size_t next_index_;
    std::mutex mutex_;
    std::condition_variable cv_;
};
```

#### 3. Frame Encoding Task

Refactor existing `encode_frame` lambda into a reusable function:

```cpp
struct FrameEncodingTask {
    FrameBuffer frame_buffer;
    int32_t frame_number;
    const VBIData* vbi_data;
    std::vector<int16_t> audio_data;
};

struct EncodedFrameResult {
    int32_t frame_number;
    Frame encoded_frame;
    std::vector<DropoutMetadata> dropouts;
};

EncodedFrameResult encode_frame_task(
    const FrameEncodingTask& task,
    VideoEncoderPipeline* pipeline,
    const EncodingConfig& config
);
```

### Implementation Steps

1. **Create thread utilities** (`src/utils/thread_pool.cpp`, `ordered_queue.cpp`)
2. **Refactor main.cpp encoding loop**:
   - Extract frame encoding into standalone function
   - Add thread pool with configurable worker count
   - Implement ordered output queue
3. **Add CLI argument**: `--threads N` (default: auto-detect)
4. **Synchronize metadata collection** with mutex
5. **Test with single thread** (verify correctness)
6. **Benchmark with multiple threads** (verify performance)

### Code Changes

**Main encoding loop** (pseudocode):

```cpp
// Create thread pool
size_t num_threads = config.num_threads.value_or(
    std::thread::hardware_concurrency() - 1
);
ThreadPool pool(num_threads);
OrderedQueue<EncodedFrameResult> output_queue;

// Submit encoding tasks
for (int32_t i = 0; i < section_frames; ++i) {
    FrameEncodingTask task = prepare_task(i);
    
    pool.enqueue([task, i, &output_queue, pipeline, &config]() {
        auto result = encode_frame_task(task, pipeline.get(), config);
        output_queue.push(i, std::move(result));
    });
}

// Writer thread: consume results in order
std::thread writer([&]() {
    for (int32_t i = 0; i < section_frames; ++i) {
        EncodedFrameResult result;
        output_queue.wait_and_pop(i, result);
        write_frame_to_disk(result);
    }
});

pool.wait_all();
writer.join();
```

### Metadata Synchronization

Dropout metadata collection needs protection:

```cpp
class DropoutSimulator {
    // ...
    void add_dropout(int32_t field, int32_t line, int32_t start, int32_t end) {
        std::lock_guard<std::mutex> lock(dropout_mutex_);
        dropouts_[field].push_back({line, start, end});
    }
    
private:
    std::mutex dropout_mutex_;
};
```

### Configuration

Add to YAML config:

```yaml
processing:
  threads: auto  # or specific number: 8
```

Add CLI argument:

```bash
encode-orc --threads 8 project.yaml
encode-orc --threads auto project.yaml  # default
```

---

## Phase 2: Field-Level Parallelism

**Priority**: MEDIUM  
**Impact**: Additional 1.5-2x speedup  
**Complexity**: Low

### Implementation

Within each frame encoding task, parallelize field1 and field2 encoding:

```cpp
EncodedFrameResult encode_frame_task(...) {
    // Split frame into fields
    auto field_pair = pipeline->split_frame(frame_buffer, field_number);
    
    // Encode both fields in parallel
    auto future1 = std::async(std::launch::async, [&]() {
        return pipeline->encode_field_from_yuv(
            field_pair.field1, field_number, true, vbi_data
        );
    });
    
    auto future2 = std::async(std::launch::async, [&]() {
        return pipeline->encode_field_from_yuv(
            field_pair.field2, field_number + 1, false, vbi_data
        );
    });
    
    Frame encoded_frame;
    encoded_frame.field1() = future1.get();
    encoded_frame.field2() = future2.get();
    
    return EncodedFrameResult{frame_number, std::move(encoded_frame), dropouts};
}
```

### Considerations

- Uses `std::async` with `std::launch::async` policy
- No additional infrastructure needed
- Potential thread oversubscription if combined with frame-level pool
- May want to disable if frame-level parallelism already saturates CPU

### Configuration

```yaml
processing:
  threads: 8
  parallel_fields: true  # Enable field-level parallelism within frames
```

---

## Phase 3: I/O and Encoding Pipeline

**Priority**: MEDIUM  
**Impact**: 10-30% improvement for I/O-bound sources  
**Complexity**: Medium

### Problem

Current MOV/MP4 loading uses batch loading (50 frames) but processes serially:

```cpp
// Current: Load batch → Encode all → Load next batch
for (batch_start = 0; batch_start < section_frames; batch_start += BATCH_SIZE) {
    load_frames(batch_start, BATCH_SIZE);  // I/O wait
    for (each frame in batch) {
        encode_frame();  // CPU wait
    }
}
```

### Solution: Producer-Consumer Pipeline

```
┌──────────────┐     ┌─────────────┐     ┌──────────────┐
│ I/O Thread   │────▶│ Frame Buffer│────▶│ Worker Pool  │
│ (prefetch)   │     │ (N frames)  │     │ (encoding)   │
└──────────────┘     └─────────────┘     └──────────────┘
```

### Implementation

```cpp
class FrameLoader {
public:
    FrameLoader(VideoSource* source, size_t buffer_size);
    
    // Starts background thread
    void start_prefetch();
    
    // Blocks if buffer empty
    std::optional<FrameBuffer> get_next_frame();
    
    void stop();
    
private:
    void prefetch_worker();
    
    BlockingQueue<FrameBuffer> buffer_;
    std::thread prefetch_thread_;
    std::atomic<bool> running_;
};
```

### Usage

```cpp
FrameLoader loader(&mov_loader, 10);  // Prefetch 10 frames ahead
loader.start_prefetch();

while (auto frame = loader.get_next_frame()) {
    // Submit to encoding thread pool
    pool.enqueue([frame = std::move(*frame)]() {
        return encode_frame_task(frame, ...);
    });
}

loader.stop();
```

### Benefits

- Hides FFmpeg decoding latency
- Smooths out I/O spikes
- Particularly beneficial for network-based sources
- Minimal CPU overhead

---

## Phase 4: Line-Level Parallelism (Optional)

**Priority**: LOW  
**Impact**: Potentially 10-20% additional speedup  
**Complexity**: High

### Approach

Use OpenMP or parallel algorithms to encode multiple lines within a field simultaneously.

```cpp
void encode_field_from_yuv(...) {
    // ... setup code ...
    
    #pragma omp parallel for schedule(dynamic, 8)
    for (int32_t line = active_lines_start; line < active_lines_end; ++line) {
        encode_active_line(line_buffer[line], ...);
    }
}
```

### Challenges

- **Phase calculations**: Subcarrier phase depends on line number, but each line is independent
- **Metadata generators**: May have line dependencies
- **Thread overhead**: May exceed benefits for typical field heights (262-312 lines)
- **Complexity**: Requires careful synchronization

### Recommendation

**Skip this phase** unless profiling shows encoding is still CPU-bound after Phases 1-2.

---

## Implementation Roadmap

### Milestone 1: Basic Frame Parallelism (Week 1-2)

- [ ] Create `ThreadPool` class
- [ ] Create `OrderedQueue` class  
- [ ] Refactor `encode_frame` into standalone function
- [ ] Add `--threads` CLI argument
- [ ] Update CMakeLists.txt (link pthread on Linux)
- [ ] Unit tests for thread utilities
- [ ] Integration test: verify output identical to single-threaded
- [ ] Benchmark with 1, 2, 4, 8 threads

### Milestone 2: Field Parallelism (Week 3)

- [ ] Add field-level `std::async` within frame encoding
- [ ] Add `parallel_fields` config option
- [ ] Benchmark combined frame+field parallelism
- [ ] Document thread oversubscription behavior

### Milestone 3: I/O Pipeline (Week 4)

- [ ] Create `FrameLoader` with prefetch
- [ ] Integrate with MOV/MP4 loaders
- [ ] Add buffer size configuration
- [ ] Benchmark I/O-bound scenarios

### Milestone 4: Polish (Week 5)

- [ ] Thread-safe logging (check spdlog thread safety)
- [ ] Progress reporting from multiple threads
- [ ] Error handling and graceful shutdown
- [ ] Documentation and examples
- [ ] Performance tuning guide

---

## Technical Considerations

### Thread Safety Audit

| Component | Status | Action Required |
|-----------|--------|-----------------|
| Filter buffers | ✅ Safe | Already `thread_local` |
| Metadata collection | ⚠️ Needs sync | Add mutex to dropouts |
| Logging | ✅ Safe | spdlog is thread-safe |
| File I/O | ⚠️ Sequential | Use single writer thread |
| YAML config | ✅ Safe | Read-only after parse |
| Pipeline state | ✅ Safe | No shared mutable state |

### Memory Considerations

Each worker thread will have:
- Frame buffer: ~720×576×2×3 = 2.5 MB (YUV)
- Encoded fields: ~928×313×2×2 = 1.2 MB
- Filter state: ~50 KB

**Per-thread memory**: ~4 MB  
**8 threads**: ~32 MB  
**Queue depth (10 frames)**: ~40 MB

**Total overhead**: ~70 MB (acceptable)

### Platform Support

| Platform | Threading Support | Notes |
|----------|-------------------|-------|
| Linux | `std::thread`, pthread | Native support ✅ |
| Windows | `std::thread` | Native support ✅ |
| macOS | `std::thread`, pthread | Native support ✅ |

No external dependencies required - all using C++17 standard library.

### Build System Changes

Update [CMakeLists.txt](CMakeLists.txt):

```cmake
# Find Threads package
find_package(Threads REQUIRED)

# Link against threads
target_link_libraries(encode-orc PRIVATE
    # ... existing libraries ...
    Threads::Threads
)
```

---

## Performance Expectations

### Baseline (Current)

- **Test**: Encode 1000 PAL frames from PNG
- **CPU**: Intel i7-8700K (6 cores, 12 threads)
- **Time**: ~120 seconds
- **CPU usage**: ~17% (1 of 6 cores)

### Phase 1: Frame Parallelism (6 threads)

- **Expected time**: ~20 seconds (6x speedup)
- **CPU usage**: ~100% (all cores utilized)
- **Throughput**: 50 frames/second

### Phase 2: Frame + Field Parallelism

- **Expected time**: ~12-15 seconds (8-10x speedup)
- **CPU usage**: ~100% (may oversubscribe slightly)
- **Throughput**: 66-83 frames/second

### Phase 3: With I/O Pipeline (MOV source)

- **Expected time**: ~10-12 seconds (10-12x speedup)
- **I/O wait eliminated**: Encoding starts immediately
- **Throughput**: 83-100 frames/second

---

## Configuration Examples

### Default (Auto-detect)

```bash
encode-orc project.yaml
# Uses hardware_concurrency() - 1 threads
```

### Explicit Thread Count

```bash
encode-orc --threads 8 project.yaml
```

### Disable Parallelism (Debug)

```bash
encode-orc --threads 1 project.yaml
```

### YAML Configuration

```yaml
processing:
  threads: auto          # or specific number: 8
  parallel_fields: true  # Enable field-level parallelism
  io_prefetch: 10        # Prefetch buffer size (frames)
```

---

## Testing Strategy

### Unit Tests

- [ ] `ThreadPool`: task execution, exception handling, shutdown
- [ ] `OrderedQueue`: ordering, blocking, multiple producers
- [ ] `FrameLoader`: prefetch, buffer limits, EOF handling

### Integration Tests

- [ ] Verify output **exactly matches** single-threaded version
- [ ] Test all input formats (YUV422, PNG, MOV, MP4)
- [ ] Test with/without Y/C output
- [ ] Test with effects (noise, dropouts)
- [ ] Test metadata generation accuracy

### Performance Tests

- [ ] Benchmark 1, 2, 4, 8, 16 threads
- [ ] Measure speedup vs thread count (Amdahl's law)
- [ ] Test memory usage with valgrind/heaptrack
- [ ] Profile with perf/Instruments to find bottlenecks

### Stress Tests

- [ ] Long encodes (10,000+ frames)
- [ ] Rapid start/stop cycles
- [ ] Error injection (corrupted frames)
- [ ] Memory leak detection

---

## Risk Mitigation

### Risk 1: Output Differences

**Mitigation**: Extensive testing with binary comparison against single-threaded output

### Risk 2: Race Conditions

**Mitigation**: 
- Minimize shared mutable state
- Use thread-safe containers
- Extensive stress testing with thread sanitizer

### Risk 3: Memory Pressure

**Mitigation**:
- Bounded queues
- Configurable thread count
- Monitor memory usage in CI

### Risk 4: Performance Regression

**Mitigation**:
- Keep single-threaded path (--threads 1)
- Performance benchmarks in CI
- Document optimal thread counts per platform

---

## Future Enhancements

### GPU Acceleration (Future)

Active video encoding (YUV → composite) could benefit from GPU:
- Subcarrier modulation is embarrassingly parallel
- CUDA/OpenCL for line-level processing
- Potential 50-100x speedup for encoding kernel

### SIMD Optimization (Future)

Use AVX2/NEON for:
- YUV to composite conversion
- Filter operations
- Color space transformations

### Distributed Encoding (Future)

For very long videos, support distributed encoding across machines:
- Split video into chunks
- Encode on multiple machines
- Merge results

---

## Conclusion

Multi-threading encode-orc is feasible and will provide significant performance improvements with minimal architectural changes. The phased approach allows incremental development and testing while delivering value at each milestone.

**Key Success Factors**:
1. Clean existing architecture (value semantics, no shared state)
2. Already using `thread_local` for buffers
3. Independent frame processing
4. Standard library support (no external dependencies)

**Recommended Start**: Implement Phase 1 (frame-level parallelism) first, as it provides the best ROI with manageable complexity.
