/*
 * File:        main.cpp
 * Module:      encode-orc
 * Purpose:     Main application entry point
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: 2026 Simon Inns
 */

#ifdef _MSC_VER
#define _CRT_SECURE_NO_WARNINGS
#define popen _popen
#define pclose _pclose
#endif

#include "yaml_config.h"
#include "video_encoder_pipeline.h"
#include "metadata_generator.h"
#include "pipeline_generators.h"
#include "vits_pipeline_generator.h"
#include "field_effect.h"
#include "video_parameters.h"
#include "metadata.h"
#include "logging.h"
#include "yuv422_loader.h"
#include "png_loader.h"
#include "mov_loader.h"
#include "mp4_loader.h"
#include "frame_buffer.h"
#include "writer.h"
#include "tbc_writer.h"
#include "yc_tbc_writer.h"
#include "standard_writer.h"
#include "audio_writer.h"
#include "audio_generator.h"
#include "version.h"
#include "thread_pool.h"
#include "ordered_queue.h"
#include <iostream>
#include <fstream>
#include <cstdio>
#include <memory>
#include <array>
#include <sstream>
#include <cstring>
#include <cmath>
#include <filesystem>
#include <chrono>
#include <thread>
#include <future>
#include <mutex>

namespace {

// Structure to hold an encoding task
struct EncodingTask {
    encode_orc::FrameBuffer frame_buffer;
    int32_t section_frame;
    int32_t global_frame;
    int32_t field_number;
    const encode_orc::VBIData* vbi_data_field1;
    const encode_orc::VBIData* vbi_data_field2;
    int32_t vitc_frame_offset;  // VITC timecode offset for this section
};

// Structure to hold the result of encoding
struct EncodedResult {
    int32_t section_frame;
    int32_t global_frame;
    encode_orc::Frame encoded_frame;
    bool success;
    std::string error_message;
};

// Structure to hold pre-generated audio for a section
struct SectionAudio {
    std::vector<int16_t> samples;  // Interleaved stereo PCM (L, R, L, R, ...)
    int32_t samples_per_field;
    int32_t cursor = 0;  // Current read position in samples
    
    // Get audio for a single field (advances cursor)
    std::vector<int16_t> get_field_audio() {
        int32_t field_samples = samples_per_field * 2;  // Stereo
        if (cursor + field_samples > static_cast<int32_t>(samples.size())) {
            // Pad with silence if we've reached the end
            std::vector<int16_t> result(field_samples, 0);
            int32_t remaining = static_cast<int32_t>(samples.size()) - cursor;
            if (remaining > 0) {
                std::copy(samples.begin() + cursor, samples.end(), result.begin());
            }
            cursor = static_cast<int32_t>(samples.size());
            return result;
        }
        
        std::vector<int16_t> result(samples.begin() + cursor, samples.begin() + cursor + field_samples);
        cursor += field_samples;
        return result;
    }
};

// Function to encode a single frame (thread-safe, can be called from worker threads)
EncodedResult encode_single_frame(
    EncodingTask task,
    encode_orc::VideoEncoderPipeline* pipeline,
    SectionAudio* section_audio,
    std::mutex* audio_mutex = nullptr)
{
    EncodedResult result;
    result.section_frame = task.section_frame;
    result.global_frame = task.global_frame;
    result.success = false;
    
    try {
        // Attach audio to the frame (if enabled and section_audio is provided)
        if (section_audio) {
            // Lock mutex if provided (for multi-threaded access to section audio cursor)
            std::unique_lock<std::mutex> lock;
            if (audio_mutex) {
                lock = std::unique_lock<std::mutex>(*audio_mutex);
            }
            
            auto field1_audio = section_audio->get_field_audio();
            auto field2_audio = section_audio->get_field_audio();

            std::vector<int16_t> frame_audio;
            frame_audio.reserve(field1_audio.size() + field2_audio.size());
            frame_audio.insert(frame_audio.end(), field1_audio.begin(), field1_audio.end());
            frame_audio.insert(frame_audio.end(), field2_audio.begin(), field2_audio.end());
            task.frame_buffer.set_audio(std::move(frame_audio));
        }

        // Sequential field encoding
        result.encoded_frame = pipeline->encode_frame(
            task.frame_buffer,
            task.field_number,
            task.vbi_data_field1,
            task.vbi_data_field2,
            task.vitc_frame_offset
        );
        
        result.success = true;
    } catch (const std::exception& e) {
        result.error_message = std::string("Exception during encoding: ") + e.what();
        result.success = false;
    } catch (...) {
        result.error_message = "Unknown exception during encoding";
        result.success = false;
    }
    
    return result;
}

bool read_file_to_pcm(const std::string& filename, std::vector<int16_t>& out_pcm, std::string& error_message) {
    std::ifstream file(filename, std::ios::binary);
    if (!file.is_open()) {
        error_message = "Failed to open audio file: " + filename;
        return false;
    }

    file.seekg(0, std::ios::end);
    std::streamsize size = file.tellg();
    file.seekg(0, std::ios::beg);

    if (size <= 0 || size % sizeof(int16_t) != 0) {
        error_message = "Invalid PCM file size: " + filename;
        return false;
    }

    out_pcm.resize(static_cast<size_t>(size / sizeof(int16_t)));
    file.read(reinterpret_cast<char*>(out_pcm.data()), size);
    return file.good();
}

bool load_wav_file(const std::string& filename, std::vector<int16_t>& out_pcm, std::string& error_message) {
    std::ifstream file(filename, std::ios::binary);
    if (!file.is_open()) {
        error_message = "Failed to open WAV file: " + filename;
        return false;
    }

    char riff[4];
    uint32_t riff_size = 0;
    char wave[4];

    file.read(riff, 4);
    file.read(reinterpret_cast<char*>(&riff_size), 4);
    file.read(wave, 4);

    if (std::memcmp(riff, "RIFF", 4) != 0 || std::memcmp(wave, "WAVE", 4) != 0) {
        error_message = "Invalid WAV header: " + filename;
        return false;
    }

    (void)riff_size;

    uint16_t audio_format = 0;
    uint16_t channels = 0;
    uint32_t sample_rate = 0;
    uint16_t bits_per_sample = 0;
    bool found_fmt = false;
    bool found_data = false;
    uint32_t data_size = 0;
    std::streampos data_pos = 0;

    while (file && (!found_fmt || !found_data)) {
        char chunk_id[4];
        uint32_t chunk_size = 0;
        file.read(chunk_id, 4);
        file.read(reinterpret_cast<char*>(&chunk_size), 4);
        if (!file) break;

        if (std::memcmp(chunk_id, "fmt ", 4) == 0) {
            found_fmt = true;
            file.read(reinterpret_cast<char*>(&audio_format), 2);
            file.read(reinterpret_cast<char*>(&channels), 2);
            file.read(reinterpret_cast<char*>(&sample_rate), 4);
            uint32_t byte_rate = 0;
            uint16_t block_align = 0;
            file.read(reinterpret_cast<char*>(&byte_rate), 4);
            file.read(reinterpret_cast<char*>(&block_align), 2);
            file.read(reinterpret_cast<char*>(&bits_per_sample), 2);

            (void)byte_rate;
            (void)block_align;

            if (chunk_size > 16) {
                file.seekg(chunk_size - 16, std::ios::cur);
            }
        } else if (std::memcmp(chunk_id, "data", 4) == 0) {
            found_data = true;
            data_size = chunk_size;
            data_pos = file.tellg();
            file.seekg(chunk_size, std::ios::cur);
        } else {
            file.seekg(chunk_size, std::ios::cur);
        }
    }

    if (!found_fmt || !found_data) {
        error_message = "Missing fmt or data chunk in WAV file: " + filename;
        return false;
    }

    if (audio_format != 1 || channels != 2 || sample_rate != 44100 || bits_per_sample != 16) {
        error_message = "WAV must be 44.1kHz 16-bit stereo PCM: " + filename;
        return false;
    }

    if (data_size % sizeof(int16_t) != 0) {
        error_message = "Invalid WAV data size: " + filename;
        return false;
    }

    out_pcm.resize(data_size / sizeof(int16_t));
    file.seekg(data_pos, std::ios::beg);
    file.read(reinterpret_cast<char*>(out_pcm.data()), data_size);
    return file.good();
}

bool extract_mp4_audio_pcm(const std::string& filename, std::vector<int16_t>& out_pcm, std::string& error_message) {
    std::filesystem::path temp_dir = std::filesystem::temp_directory_path();
    auto timestamp = std::chrono::high_resolution_clock::now().time_since_epoch().count();
    std::filesystem::path temp_file = temp_dir / ("encode_orc_audio_" + std::to_string(timestamp) + ".pcm");

    std::ostringstream cmd;
    cmd << "ffmpeg -v error -i \"" << filename << "\" "
        << "-vn -ac 2 -ar 44100 -f s16le -acodec pcm_s16le -y \""
        << temp_file.string() << "\" 2>&1";

    std::array<char, 256> buffer;
    std::string ffmpeg_output;
    FILE* pipe = popen(cmd.str().c_str(), "r");
    if (!pipe) {
        error_message = "Failed to run ffmpeg for audio extraction";
        return false;
    }

    while (fgets(buffer.data(), static_cast<int>(buffer.size()), pipe) != nullptr) {
        ffmpeg_output += buffer.data();
    }

    int return_code = pclose(pipe);
    if (return_code != 0) {
        error_message = "ffmpeg audio extraction failed: " + ffmpeg_output;
        return false;
    }

    bool ok = read_file_to_pcm(temp_file.string(), out_pcm, error_message);
    std::error_code ec;
    std::filesystem::remove(temp_file, ec);
    return ok;
}

} // namespace

int main(int argc, char* argv[]) {
    try {
    using namespace encode_orc;
    
    // Check for help and version flags first
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--version" || arg == "-v") {
            std::cout << "encode-orc git commit: " << ENCODE_ORC_GIT_COMMIT << "\n";
            std::cout << "Encoder for decode-orc (for making test TBC/Metadata files)\n";
            return 0;
        }
        if (arg == "--help" || arg == "-h") {
            std::cout << "Usage: " << argv[0] << " <project.yaml> [OPTIONS]\n\n";
            std::cout << "Arguments:\n";
            std::cout << "  <project.yaml>          YAML project file to process\n";
            std::cout << "\n";
            std::cout << "Options:\n";
            std::cout << "  -h, --help              Show this help message\n";
            std::cout << "  -v, --version           Show version information\n";
            std::cout << "  -q, --quiet             Suppress output except errors\n";
            std::cout << "  --log-level LEVEL       Set logging verbosity\n";
            std::cout << "                          (trace, debug, info, warn, error, critical, off)\n";
            std::cout << "                          Default: info\n";
            std::cout << "  --log-file FILE         Write logs to specified file\n";
            std::cout << "\n";
            std::cout << "Examples:\n";
            std::cout << "  " << argv[0] << " project.yaml\n";
            std::cout << "  " << argv[0] << " project.yaml --log-level debug\n";
            std::cout << "  " << argv[0] << " project.yaml --log-level debug --log-file debug.log\n";
            return 0;
        }
    }
    
    // Parse command-line arguments to extract logging options and other flags
    std::string log_level = "info";
    std::string log_file = "";
    bool quiet_mode = false;
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--log-level" && i + 1 < argc) {
            log_level = argv[++i];
        } else if (arg == "--log-file" && i + 1 < argc) {
            log_file = argv[++i];
        } else if (arg == "--quiet" || arg == "-q") {
            quiet_mode = true;
        }
    }
    
    // Apply quiet mode if enabled (overrides --log-level)
    if (quiet_mode) {
        log_level = "error";
    }
    
    // Initialize logging system
    init_logging(log_level, "[%Y-%m-%d %H:%M:%S.%e] [%n] [%^%l%$] %v", log_file);
    
    // Require exactly one argument - a YAML project file
    if (argc < 2) {
        ENCODE_ORC_LOG_ERROR("No YAML project file specified");
        ENCODE_ORC_LOG_ERROR("Usage: {} <project.yaml>", argv[0]);
        ENCODE_ORC_LOG_ERROR("       {} --help", argv[0]);
        return 1;
    }
    
    // Find the YAML filename (first non-option argument)
    std::string yaml_file;
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg[0] != '-' && (i + 1 >= argc || argv[i + 1][0] == '-' || 
            (i > 0 && (std::string(argv[i - 1]) == "--log-level" || 
                       std::string(argv[i - 1]) == "--log-file")))) {
            // Check if this is a value for an option
            if (i > 0) {
                std::string prev_arg = argv[i - 1];
                if (prev_arg == "--log-level" || prev_arg == "--log-file") {
                    continue;
                }
            }
            yaml_file = arg;
            break;
        }
    }
    
    if (yaml_file.empty()) {
        ENCODE_ORC_LOG_ERROR("No YAML project file specified");
        return 1;
    }
    
    // Check if it's a YAML file
    bool is_yaml = (yaml_file.length() > 5 && yaml_file.substr(yaml_file.length() - 5) == ".yaml") ||
                   (yaml_file.length() > 4 && yaml_file.substr(yaml_file.length() - 4) == ".yml");
    if (!is_yaml) {
        ENCODE_ORC_LOG_ERROR("File must be a YAML project (.yaml or .yml), got: {}", yaml_file);
        return 1;
    }
    
    YAMLProjectConfig config;
    std::string error_msg;
    
    if (!parse_yaml_config(yaml_file, config, error_msg)) {
        ENCODE_ORC_LOG_ERROR("Error parsing YAML config: {}", error_msg);
        return 1;
    }
    
    if (!validate_yaml_config(config, error_msg)) {
        ENCODE_ORC_LOG_ERROR("Error validating YAML config: {}", error_msg);
        return 1;
    }
    
    // Validate all input files exist before processing
    ENCODE_ORC_LOG_DEBUG("Validating input files...");
    bool validation_failed = false;
    for (const auto& section : config.sections) {
        if (section.yuv422_image_source) {
            if (!std::filesystem::exists(section.yuv422_image_source->file)) {
                ENCODE_ORC_LOG_ERROR("Input file does not exist: {}", section.yuv422_image_source->file);
                validation_failed = true;
            }
        }
        if (section.png_image_source) {
            if (!std::filesystem::exists(section.png_image_source->file)) {
                ENCODE_ORC_LOG_ERROR("Input file does not exist: {}", section.png_image_source->file);
                validation_failed = true;
            }
        }
        if (section.mov_file_source) {
            if (!std::filesystem::exists(section.mov_file_source->file)) {
                ENCODE_ORC_LOG_ERROR("Input file does not exist: {}", section.mov_file_source->file);
                validation_failed = true;
            }
        }
        if (section.mp4_file_source) {
            if (!std::filesystem::exists(section.mp4_file_source->file)) {
                ENCODE_ORC_LOG_ERROR("Input file does not exist: {}", section.mp4_file_source->file);
                validation_failed = true;
            }
        }
        // Check audio files
        if (section.sound.has_value()) {
            const auto& sound_cfg = section.sound.value();
            if (sound_cfg.type == "wav" && sound_cfg.file.has_value()) {
                if (!std::filesystem::exists(sound_cfg.file.value())) {
                    ENCODE_ORC_LOG_ERROR("Audio file does not exist: {}", sound_cfg.file.value());
                    validation_failed = true;
                }
            }
        }
    }
    
    if (validation_failed) {
        ENCODE_ORC_LOG_ERROR("Input file validation failed. Please check that all files exist.");
        return 1;
    }
    
    // Validate and create output directory if needed
    ENCODE_ORC_LOG_DEBUG("Validating output destination...");
    std::filesystem::path output_path(config.output.filename);
    std::filesystem::path output_dir = output_path.parent_path();
    
    // If no directory specified, use current directory
    if (output_dir.empty()) {
        output_dir = ".";
    }
    
    // Check if output directory exists or can be created
    if (!output_dir.empty() && !std::filesystem::exists(output_dir)) {
        ENCODE_ORC_LOG_DEBUG("Output directory does not exist, creating: {}", output_dir.string());
        std::error_code ec;
        if (!std::filesystem::create_directories(output_dir, ec)) {
            ENCODE_ORC_LOG_ERROR("Failed to create output directory '{}': {}", output_dir.string(), ec.message());
            return 1;
        }
    }
    
    // Verify we can write to the output directory
    if (!std::filesystem::is_directory(output_dir)) {
        ENCODE_ORC_LOG_ERROR("Output path '{}' exists but is not a directory", output_dir.string());
        return 1;
    }
    
    // Process YAML configuration
    ENCODE_ORC_LOG_INFO("encode-orc YAML Project Encoder");
    ENCODE_ORC_LOG_INFO("Project: {}", config.name);
    ENCODE_ORC_LOG_INFO("Description: {}", config.description);
    ENCODE_ORC_LOG_INFO("Output: {} ({})", config.output.filename, config.output.format);
    
    // Determine video system
    VideoSystem system;
    if (config.output.format == "pal-composite" || config.output.format == "pal-yc") {
        system = VideoSystem::PAL;
    } else if (config.output.format == "ntsc-composite" || config.output.format == "ntsc-yc") {
        system = VideoSystem::NTSC;
    } else {
        ENCODE_ORC_LOG_ERROR("Unsupported format: {}", config.output.format);
        return 1;
    }
    
    // Store video level overrides for later use in encoding
    const auto& video_levels = config.output.video_levels;
    bool has_video_level_overrides = video_levels.has_value();
    
    if (has_video_level_overrides && video_levels.value().blanking_16b_ire.has_value()) {
        ENCODE_ORC_LOG_INFO("Video level overrides detected");
        if (video_levels.value().blanking_16b_ire.has_value()) {
            ENCODE_ORC_LOG_INFO("  blanking_16b_ire: {}", video_levels.value().blanking_16b_ire.value());
        }
        if (video_levels.value().black_16b_ire.has_value()) {
            ENCODE_ORC_LOG_INFO("  black_16b_ire: {}", video_levels.value().black_16b_ire.value());
        }
        if (video_levels.value().white_16b_ire.has_value()) {
            ENCODE_ORC_LOG_INFO("  white_16b_ire: {}", video_levels.value().white_16b_ire.value());
        }
    }
    
    // Preprocessing: probe MOV files without duration and populate them
    // This ensures all downstream code can rely on duration being set
    for (auto& section : config.sections) {
        if (section.mov_file_source && !section.duration) {
            ENCODE_ORC_LOG_DEBUG("Probing MOV file for section: {}", section.name);
            MOVLoader probe_loader;
            std::string probe_error;
            if (!probe_loader.open(section.mov_file_source->file, probe_error)) {
                ENCODE_ORC_LOG_ERROR("Error probing MOV file for section '{}': {}", section.name, probe_error);
                return 1;
            }
            
            int32_t total_frames = probe_loader.get_metadata().frame_count;
            int32_t start_frame = section.mov_file_source->start_frame.value_or(0);
            probe_loader.close();
            
            if (total_frames <= 0) {
                ENCODE_ORC_LOG_ERROR("Could not determine frame count from MOV file for section '{}'", section.name);
                return 1;
            }
            
            if (start_frame >= total_frames) {
                ENCODE_ORC_LOG_ERROR("start_frame {} is beyond available frames ({}) in section '{}'", 
                                   start_frame, total_frames, section.name);
                return 1;
            }
            
            // Set duration to remaining frames from start_frame
            const_cast<VideoSection&>(section).duration = total_frames - start_frame;
            ENCODE_ORC_LOG_DEBUG("MOV file duration set to {} frames", total_frames - start_frame);
        }
    }
    
    // Preprocessing: probe MP4 files without duration and populate them
    for (auto& section : config.sections) {
        if (section.mp4_file_source && !section.duration) {
            ENCODE_ORC_LOG_DEBUG("Probing MP4 file for section: {}", section.name);
            MP4Loader probe_loader;
            std::string probe_error;
            if (!probe_loader.open(section.mp4_file_source->file, probe_error)) {
                ENCODE_ORC_LOG_ERROR("Error probing MP4 file for section '{}': {}", section.name, probe_error);
                return 1;
            }
            
            int32_t total_frames = probe_loader.get_metadata().frame_count;
            int32_t start_frame = section.mp4_file_source->start_frame.value_or(0);
            probe_loader.close();
            
            if (total_frames <= 0) {
                ENCODE_ORC_LOG_ERROR("Could not determine frame count from MP4 file for section '{}'", section.name);
                return 1;
            }
            
            if (start_frame >= total_frames) {
                ENCODE_ORC_LOG_ERROR("start_frame {} is beyond available frames ({}) in section '{}'", 
                                   start_frame, total_frames, section.name);
                return 1;
            }
            
            // Set duration to remaining frames from start_frame
            const_cast<VideoSection&>(section).duration = total_frames - start_frame;
            ENCODE_ORC_LOG_DEBUG("MP4 file duration set to {} frames", total_frames - start_frame);
        }
    }
    
    // Calculate total frames now that all durations are known
    int32_t total_frames = 0;
    for (const auto& section : config.sections) {
        if (section.duration) {
            total_frames += section.duration.value();
        }
    }
    
    // Display section information
    ENCODE_ORC_LOG_INFO("Sections to encode: {}", config.sections.size());
    for (const auto& section : config.sections) {
        ENCODE_ORC_LOG_INFO("Section: {}", section.name);
        
        if (section.yuv422_image_source) {
            ENCODE_ORC_LOG_INFO("  File: {}", section.yuv422_image_source->file);
        }
        if (section.png_image_source) {
            ENCODE_ORC_LOG_INFO("  File: {}", section.png_image_source->file);
        }
        if (section.mov_file_source) {
            ENCODE_ORC_LOG_INFO("  MOV File: {}", section.mov_file_source->file);
            if (section.mov_file_source->start_frame) {
                ENCODE_ORC_LOG_INFO("  Start Frame: {}", section.mov_file_source->start_frame.value());
            }
        }
        if (section.mp4_file_source) {
            ENCODE_ORC_LOG_INFO("  MP4 File: {}", section.mp4_file_source->file);
            if (section.mp4_file_source->start_frame) {
                ENCODE_ORC_LOG_INFO("  Start Frame: {}", section.mp4_file_source->start_frame.value());
            }
        }
        if (section.duration) {
            ENCODE_ORC_LOG_INFO("  Frames: {}", section.duration.value());
        }
    }
    
    ENCODE_ORC_LOG_INFO("Total frames to encode: {}", total_frames);

    // Build video parameters (with optional overrides)
    VideoParameters params = (system == VideoSystem::PAL) ?
        VideoParameters::create_pal_composite() :
        VideoParameters::create_ntsc_composite();

    if (has_video_level_overrides) {
        VideoParameters::apply_video_level_overrides(
            params,
            video_levels.value().blanking_16b_ire,
            video_levels.value().black_16b_ire,
            video_levels.value().white_16b_ire
        );
    }

    // Handle filename for different output formats
    std::string output_filename = config.output.filename;
    std::string base_filename = config.output.filename;  // Base name for metadata
    if (config.output.format == "pal-composite" || config.output.format == "ntsc-composite") {
        // Add .tbc extension if not already present
        if (output_filename.length() < 4 || output_filename.substr(output_filename.length() - 4) != ".tbc") {
            output_filename += ".tbc";
        }
    } else if (config.output.format == "pal-yc" || config.output.format == "ntsc-yc") {
        // For Y/C formats, remove any .tbc extension (will be handled by Y/C writer)
        if (output_filename.length() >= 4 && output_filename.substr(output_filename.length() - 4) == ".tbc") {
            output_filename = output_filename.substr(0, output_filename.length() - 4);
        }
    }

    // Open audio writer if sound output is enabled
    std::unique_ptr<AudioWriter> audio_writer;
    bool sound_enabled = config.output.sound_format.has_value();
    int32_t samples_per_field = (system == VideoSystem::PAL) ? 882 : 735;
    if (sound_enabled) {
        std::string audio_base = output_filename;
        if (audio_base.size() >= 4 && audio_base.substr(audio_base.size() - 4) == ".tbc") {
            audio_base = audio_base.substr(0, audio_base.size() - 4);
        }
        std::string audio_ext = (config.output.sound_format.value() == "wav") ? ".wav" : ".pcm";
        std::string audio_filename = audio_base + audio_ext;

        audio_writer = std::make_unique<AudioWriter>();
        AudioWriter::Format audio_format = (config.output.sound_format.value() == "wav")
            ? AudioWriter::Format::WAV
            : AudioWriter::Format::PCM;

        if (!audio_writer->open(audio_filename, audio_format, 44100, 2, 16)) {
            ENCODE_ORC_LOG_ERROR("Could not open audio output file: {}", audio_filename);
            ENCODE_ORC_LOG_ERROR("Ensure the output directory exists and you have write permissions.");
            return 1;
        }
    }

    // Open output writer
    std::unique_ptr<Writer> writer;
    std::unique_ptr<YCTBCWriter> yc_writer;
    
    if (config.output.format == "pal-yc" || config.output.format == "ntsc-yc") {
        // Use Y/C writer for separate Y and C output
        yc_writer = std::make_unique<YCTBCWriter>(YCTBCWriter::NamingMode::MODERN);
        if (!yc_writer->open(output_filename)) {
            ENCODE_ORC_LOG_ERROR("Could not open Y/C output files: {}", output_filename);
            ENCODE_ORC_LOG_ERROR("Ensure the output directory exists and you have write permissions.");
            return 1;
        }
        
        // Set padding for both Y and C writers
        int32_t field_height_diff = params.field2_height - params.field1_height;
        if (auto* y_tbc = yc_writer->y_writer()) {
            y_tbc->set_field1_padding(params.field_width, static_cast<uint16_t>(params.blanking_16b_ire), field_height_diff);
        }
        if (auto* c_tbc = yc_writer->c_writer()) {
            c_tbc->set_field1_padding(params.field_width, static_cast<uint16_t>(params.blanking_16b_ire), field_height_diff);
        }
    } else {
        // Use standard composite writer
        if (config.output.writer == "standard") {
            writer = std::make_unique<StandardWriter>();
        } else {
            writer = std::make_unique<TBCWriter>();
        }

        if (!writer->open(output_filename)) {
            ENCODE_ORC_LOG_ERROR("Could not open output file: {}", output_filename);
            ENCODE_ORC_LOG_ERROR("Ensure the output directory exists and you have write permissions.");
            return 1;
        }

        if (auto* tbc = dynamic_cast<TBCWriter*>(writer.get())) {
            int32_t field_height_diff = params.field2_height - params.field1_height;
            tbc->set_field1_padding(params.field_width, static_cast<uint16_t>(params.blanking_16b_ire), field_height_diff);
        }
    }

    // Pre-generate VBI data if required (LaserDisc biphase)
    bool needs_vbi_data = false;
    if (config.pipeline.metadata.has_value()) {
        for (const auto& gen : config.pipeline.metadata->generators) {
            if (gen.type == "biphase-vbi" && gen.enabled) {
                needs_vbi_data = true;
                break;
            }
        }
    }

    CaptureMetadata pre_metadata;
    if (needs_vbi_data) {
        std::string prep_error;
        if (!generate_metadata(config, system, total_frames, "", prep_error, &pre_metadata)) {
            ENCODE_ORC_LOG_ERROR("Metadata preparation error: {}", prep_error);
            return 1;
        }
    }

    // Determine number of threads to use
    size_t num_threads;
    size_t hw_threads = std::thread::hardware_concurrency();
    
    if (config.processing.has_value() && config.processing->threads.has_value()) {
        int32_t thread_config = config.processing->threads.value();
        if (thread_config <= 0) {
            // Auto-detect: use hardware_concurrency() - 1, minimum 1
            num_threads = (hw_threads > 1) ? (hw_threads - 1) : 1;
            ENCODE_ORC_LOG_INFO("Multi-threading: AUTO (detected {} hardware threads, using {} encoding threads)", 
                               hw_threads, num_threads);
        } else {
            num_threads = static_cast<size_t>(thread_config);
            ENCODE_ORC_LOG_INFO("Multi-threading: ENABLED ({} encoding thread(s) configured)", num_threads);
        }
    } else {
        // Default: auto-detect
        num_threads = (hw_threads > 1) ? (hw_threads - 1) : 1;
        ENCODE_ORC_LOG_INFO("Multi-threading: AUTO (default - detected {} hardware threads, using {} encoding threads)", 
                           hw_threads, num_threads);
    }
    
    // Create thread pool if multi-threading is enabled
    std::unique_ptr<ThreadPool> thread_pool;
    if (num_threads > 1) {
        thread_pool = std::make_unique<ThreadPool>(num_threads);
        ENCODE_ORC_LOG_DEBUG("Thread pool created with {} worker threads", num_threads);
    }

    int32_t frame_offset = 0;
    int32_t vitc_frame_offset = 0;  // VITC timecode offset (independent from frame_offset)
    
    // Audio mutex for thread-safe access to section audio cursor
    std::mutex audio_mutex;

    for (const auto& section : config.sections) {
        ENCODE_ORC_LOG_INFO("Encoding section: {}", section.name);

        // Calculate VITC frame offset for this section
        int32_t current_vitc_offset = vitc_frame_offset;
        if (section.vitc.has_value() && section.vitc->timecode_start.has_value()) {
            // Parse timecode_start (HH:MM:SS.FF format)
            int32_t hh = 0, mm = 0, ss = 0, ff = 0;
            std::sscanf(section.vitc->timecode_start.value().c_str(), "%d:%d:%d.%d", &hh, &mm, &ss, &ff);
            int32_t fps = (system == VideoSystem::PAL) ? 25 : 30;
            current_vitc_offset = (hh * 3600 + mm * 60 + ss) * fps + ff;
            vitc_frame_offset = current_vitc_offset;  // Update running offset
            ENCODE_ORC_LOG_DEBUG("Section '{}' VITC timecode start: {}:{}:{}.{} (offset: {})", 
                               section.name, hh, mm, ss, ff, current_vitc_offset);
        }

        // Track actual number of frames encoded in this section
        int32_t section_frames = 0;

        if (section.yuv422_image_source || section.png_image_source || section.mov_file_source || section.mp4_file_source) {
            // Pre-generate audio for this section
            SectionAudio section_audio;
            section_audio.samples_per_field = samples_per_field;
            
            if (sound_enabled && section.sound.has_value()) {
                const auto& sound_cfg = section.sound.value();
                
                // Calculate total number of fields in section
                int32_t num_fields = section.duration.value() * 2;  // 2 fields per frame
                int32_t total_audio_samples = num_fields * samples_per_field;
                
                ENCODE_ORC_LOG_DEBUG("Pre-generating audio for section '{}': {} fields, {} samples/field, {} total samples", 
                                    section.name, num_fields, samples_per_field, total_audio_samples);
                
                if (sound_cfg.type == "silence") {
                    section_audio.samples = AudioGenerator::generate_silence(total_audio_samples);
                } else if (sound_cfg.type == "sine" || sound_cfg.type == "square" || sound_cfg.type == "sawtooth") {
                    AudioGenerator::WaveformType waveform_type = AudioGenerator::string_to_type(sound_cfg.type);
                    double start_freq = sound_cfg.start_freq_hz.value_or(440.0);
                    double end_freq = sound_cfg.end_freq_hz.value_or(start_freq);  // Default to start_freq if not specified
                    double amplitude = sound_cfg.amplitude.value_or(75.0);  // Default to 75%
                    double balance = sound_cfg.balance.value_or(0.0);  // Default to 0 (centered)
                    
                    section_audio.samples = AudioGenerator::generate(
                        waveform_type,
                        total_audio_samples,
                        44100,  // Sample rate
                        start_freq,
                        end_freq,
                        amplitude,
                        sound_cfg.seed.value_or(0),
                        balance
                    );
                } else if (sound_cfg.type == "pink" || sound_cfg.type == "white" || sound_cfg.type == "brown") {
                    AudioGenerator::WaveformType waveform_type = AudioGenerator::string_to_type(sound_cfg.type);
                    double amplitude = sound_cfg.amplitude.value_or(75.0);  // Default to 75%
                    double balance = sound_cfg.balance.value_or(0.0);  // Default to 0 (centered)
                    
                    section_audio.samples = AudioGenerator::generate(
                        waveform_type,
                        total_audio_samples,
                        44100,
                        0.0,  // Frequency not used for noise
                        0.0,
                        amplitude,
                        sound_cfg.seed.value_or(0),
                        balance
                    );
                } else if (sound_cfg.type == "wav") {
                    // Load WAV file
                    std::string wav_error;
                    std::vector<int16_t> wav_pcm;
                    if (!load_wav_file(sound_cfg.file.value(), wav_pcm, wav_error)) {
                        ENCODE_ORC_LOG_ERROR("Failed to load WAV audio for section '{}': {}", section.name, wav_error);
                        return 1;
                    }
                    
                    // Use WAV data up to the section length, pad with silence if shorter
                    int32_t total_samples_needed = total_audio_samples * 2;  // Stereo
                    if (static_cast<int32_t>(wav_pcm.size()) >= total_samples_needed) {
                        section_audio.samples.assign(wav_pcm.begin(), wav_pcm.begin() + total_samples_needed);
                    } else {
                        section_audio.samples = wav_pcm;
                        section_audio.samples.resize(total_samples_needed, 0);  // Pad with silence
                    }
                } else if (sound_cfg.type == "source") {
                    // Extract audio from source file (MOV or MP4)
                    std::vector<int16_t> source_pcm;
                    std::string extract_error;
                    
                    if (section.mp4_file_source) {
                        if (!extract_mp4_audio_pcm(section.mp4_file_source->file, source_pcm, extract_error)) {
                            ENCODE_ORC_LOG_ERROR("Failed to extract MP4 audio for section '{}': {}", section.name, extract_error);
                            return 1;
                        }
                    } else if (section.mov_file_source) {
                        if (!extract_mp4_audio_pcm(section.mov_file_source->file, source_pcm, extract_error)) {
                            ENCODE_ORC_LOG_ERROR("Failed to extract MOV audio for section '{}': {}", section.name, extract_error);
                            return 1;
                        }
                    }
                    
                    // Use source audio up to the section length, pad with silence if shorter
                    int32_t total_samples_needed = total_audio_samples * 2;  // Stereo
                    if (static_cast<int32_t>(source_pcm.size()) >= total_samples_needed) {
                        section_audio.samples.assign(source_pcm.begin(), source_pcm.begin() + total_samples_needed);
                    } else {
                        section_audio.samples = source_pcm;
                        section_audio.samples.resize(total_samples_needed, 0);  // Pad with silence
                    }
                }
            } else if (sound_enabled) {
                // No sound configured, use silence
                int32_t num_fields = section.duration.value() * 2;
                int32_t total_audio_samples = num_fields * samples_per_field;
                section_audio.samples = AudioGenerator::generate_silence(total_audio_samples);
            }
            
            // Determine which pipeline configuration to use
            // Section-level pipeline settings extend/override global pipeline
            // - If section has metadata, use section metadata; otherwise use global metadata
            // - If section has preprocessing, use section preprocessing; otherwise use global preprocessing
            // - If section has effects, use section effects; otherwise use global effects
            const PipelineMetadataConfig* metadata_config = nullptr;
            const PipelinePreprocessingConfig* preprocessing_config = nullptr;
            const PipelineEffectsConfig* effects_config = nullptr;
            
            // Determine metadata config (section overrides global)
            if (section.pipeline.has_value() && section.pipeline->metadata.has_value()) {
                metadata_config = &section.pipeline->metadata.value();
                ENCODE_ORC_LOG_DEBUG("Using section-level metadata configuration for section '{}'", section.name);
            } else if (config.pipeline.metadata.has_value()) {
                metadata_config = &config.pipeline.metadata.value();
            }
            
            // Determine preprocessing config (section overrides global)
            if (section.pipeline.has_value() && section.pipeline->preprocessing.has_value()) {
                preprocessing_config = &section.pipeline->preprocessing.value();
                ENCODE_ORC_LOG_DEBUG("Using section-level preprocessing configuration for section '{}'", section.name);
            } else if (config.pipeline.preprocessing.has_value()) {
                preprocessing_config = &config.pipeline.preprocessing.value();
            }
            
            // Determine effects config (section overrides global)
            if (section.pipeline.has_value() && section.pipeline->effects.has_value()) {
                effects_config = &section.pipeline->effects.value();
                ENCODE_ORC_LOG_DEBUG("Using section-level effects configuration for section '{}'", section.name);
            } else if (config.pipeline.effects.has_value()) {
                effects_config = &config.pipeline.effects.value();
            }

            // Instantiate metadata generators from YAML configuration
            std::vector<std::unique_ptr<MetadataGenerator>> generators;
            if (metadata_config != nullptr) {
                for (const auto& gen_config : metadata_config->generators) {
                    if (!gen_config.enabled) {
                        continue;  // Skip disabled generators
                    }

                    if (gen_config.type == "color-burst") {
                        generators.push_back(std::make_unique<ColorBurstMetadataGenerator>(params));
                        ENCODE_ORC_LOG_DEBUG("Added ColorBurst generator to pipeline");
                    }
                    else if (gen_config.type == "vitc") {
                        // Parse lines if provided, otherwise use defaults
                        std::vector<int32_t> lines;
                        if (!gen_config.lines.empty()) {
                            // Convert from 1-indexed (YAML) to 0-indexed (internal)
                            for (int32_t line_1indexed : gen_config.lines) {
                                lines.push_back(line_1indexed - 1);
                            }
                        }
                        generators.push_back(std::make_unique<VITCMetadataGenerator>(params, lines));
                        ENCODE_ORC_LOG_DEBUG("Added VITC generator to pipeline");
                    }
                    else if (gen_config.type == "vits") {
                        generators.push_back(std::make_unique<VITSMetadataGenerator>(params));
                        ENCODE_ORC_LOG_DEBUG("Added VITS generator to pipeline");
                    }
                    else if (gen_config.type == "vits-pal") {
                        // Parse VITS signal configuration from YAML
                        std::vector<VITSSignalConfig> vits_signals;
                        bool has_errors = false;
                        
                        for (const auto& signal_yaml : gen_config.vits_signals) {
                            VITSSignalConfig signal_cfg;
                            
                            // Line numbers in YAML are 1-indexed absolute
                            // Convert to field number and field-relative 0-indexed line
                            int32_t absolute_line_1indexed = signal_yaml.line;
                            int32_t absolute_line_0indexed = absolute_line_1indexed - 1;
                            int32_t total_lines = params.field1_height + params.field2_height;
                            if (absolute_line_0indexed < 0 || absolute_line_0indexed >= total_lines) {
                                ENCODE_ORC_LOG_ERROR("VITS line {} is out of range for this video system (valid: 1-{})", absolute_line_1indexed, total_lines);
                                has_errors = true;
                                continue;
                            }
                            
                            if (absolute_line_0indexed < params.field1_height) {
                                signal_cfg.field = 1;
                                signal_cfg.line = absolute_line_0indexed;
                            } else {
                                signal_cfg.field = 2;
                                signal_cfg.line = absolute_line_0indexed - params.field1_height;
                            }
                            
                            if (!parse_vits_signal_type(signal_yaml.signal, signal_cfg.signal)) {
                                ENCODE_ORC_LOG_WARN("Unknown VITS signal type '{}', skipping", signal_yaml.signal);
                                continue;
                            }
                            
                            // Validate that only PAL-specific signals are used
                            if (signal_cfg.signal == VITSSignalType::VIR ||
                                signal_cfg.signal == VITSSignalType::NTC7_COMPOSITE ||
                                signal_cfg.signal == VITSSignalType::NTC7_COMBINATION) {
                                ENCODE_ORC_LOG_ERROR("VITS signal '{}' is NTSC-specific and cannot be used in PAL projects", signal_yaml.signal);
                                has_errors = true;
                                continue;
                            }
                            
                            vits_signals.push_back(signal_cfg);
                        }
                        
                        if (has_errors) {
                            ENCODE_ORC_LOG_ERROR("PAL VITS configuration has errors. Use only PAL-specific signals: itu-composite, uk-national, itu-combination, multiburst");
                            return 1;
                        }
                        
                        generators.push_back(std::make_unique<PALVITSMetadataGenerator>(params, vits_signals));
                        ENCODE_ORC_LOG_DEBUG("Added PAL VITS generator to pipeline with {} signals", vits_signals.size());
                    }
                    else if (gen_config.type == "vits-ntsc") {
                        // Parse VITS signal configuration from YAML
                        std::vector<VITSSignalConfig> vits_signals;
                        bool has_errors = false;
                        
                        for (const auto& signal_yaml : gen_config.vits_signals) {
                            VITSSignalConfig signal_cfg;
                            
                            // Line numbers in YAML are 1-indexed absolute
                            // Convert to field number and field-relative 0-indexed line
                            int32_t absolute_line_1indexed = signal_yaml.line;
                            int32_t absolute_line_0indexed = absolute_line_1indexed - 1;
                            int32_t total_lines = params.field1_height + params.field2_height;
                            if (absolute_line_0indexed < 0 || absolute_line_0indexed >= total_lines) {
                                ENCODE_ORC_LOG_ERROR("VITS line {} is out of range for this video system (valid: 1-{})", absolute_line_1indexed, total_lines);
                                has_errors = true;
                                continue;
                            }
                            
                            if (absolute_line_0indexed < params.field1_height) {
                                signal_cfg.field = 1;
                                signal_cfg.line = absolute_line_0indexed;
                            } else {
                                signal_cfg.field = 2;
                                signal_cfg.line = absolute_line_0indexed - params.field1_height;
                            }
                            
                            if (!parse_vits_signal_type(signal_yaml.signal, signal_cfg.signal)) {
                                ENCODE_ORC_LOG_WARN("Unknown VITS signal type '{}', skipping", signal_yaml.signal);
                                continue;
                            }
                            
                            // Validate that only NTSC-specific signals are used
                            if (signal_cfg.signal == VITSSignalType::UK_NATIONAL ||
                                signal_cfg.signal == VITSSignalType::ITU_COMPOSITE ||
                                signal_cfg.signal == VITSSignalType::ITU_ITS ||
                                signal_cfg.signal == VITSSignalType::MULTIBURST) {
                                ENCODE_ORC_LOG_ERROR("VITS signal '{}' is PAL-specific and cannot be used in NTSC projects", signal_yaml.signal);
                                has_errors = true;
                                continue;
                            }
                            
                            vits_signals.push_back(signal_cfg);
                        }
                        
                        if (has_errors) {
                            ENCODE_ORC_LOG_ERROR("NTSC VITS configuration has errors. Use only NTSC-specific signals: vir, ntc7-composite, ntc7-combination");
                            return 1;
                        }
                        
                        generators.push_back(std::make_unique<NTSCVITSMetadataGenerator>(params, vits_signals));
                        ENCODE_ORC_LOG_DEBUG("Added NTSC VITS generator to pipeline with {} signals", vits_signals.size());
                    }
                    else if (gen_config.type == "biphase-vbi") {
                        // Parse lines if provided, otherwise use defaults
                        std::vector<int32_t> lines;
                        if (!gen_config.lines.empty()) {
                            // Convert from 1-indexed (YAML) to 0-indexed (internal)
                            for (int32_t line_1indexed : gen_config.lines) {
                                lines.push_back(line_1indexed - 1);
                            }
                        }
                        generators.push_back(std::make_unique<BiphaseVBIMetadataGenerator>(params, lines));
                        ENCODE_ORC_LOG_DEBUG("Added BiphaseVBI generator to pipeline");
                    }
                    else {
                        ENCODE_ORC_LOG_WARN("Unknown generator type '{}' in pipeline configuration", gen_config.type);
                    }
                }
            }

            // Instantiate field effects from YAML configuration
            std::vector<std::unique_ptr<FieldEffect>> effects;
            if (effects_config != nullptr) {
                for (const auto& effect_config : effects_config->effects) {
                    if (!effect_config.enabled) {
                        continue;  // Skip disabled effects
                    }

                    if (effect_config.type == "noise") {
                        std::unique_ptr<NoiseGenerator> noise_gen;

                        // Create noise generator with SNR or direct noise level
                        if (effect_config.snr_db.has_value()) {
                            noise_gen = std::make_unique<NoiseGenerator>(NoiseGenerator::from_snr(effect_config.snr_db.value()));
                            ENCODE_ORC_LOG_DEBUG("Added Noise generator with SNR: {} dB", effect_config.snr_db.value());
                        } else if (effect_config.noise_level_db.has_value()) {
                            noise_gen = std::make_unique<NoiseGenerator>(effect_config.noise_level_db.value());
                            ENCODE_ORC_LOG_DEBUG("Added Noise generator with noise level: {} dB", effect_config.noise_level_db.value());
                        } else {
                            // Use default (40 dB SNR)
                            noise_gen = std::make_unique<NoiseGenerator>(NoiseGenerator::from_snr(40.0));
                            ENCODE_ORC_LOG_DEBUG("Added Noise generator with default 40 dB SNR");
                        }

                        // Set seed if provided
                        if (effect_config.seed.has_value()) {
                            noise_gen->set_seed(effect_config.seed.value());
                        }

                        effects.push_back(std::move(noise_gen));
                    }
                    else if (effect_config.type == "dropout") {
                        double density = effect_config.dropout_density.value_or(0.01);
                        auto dropout = std::make_unique<DropoutSimulator>(density);
                        if (effect_config.seed.has_value()) {
                            dropout->set_seed(effect_config.seed.value());
                        }
                        if (effect_config.dropout_multi_field_prob.has_value()) {
                            dropout->set_multi_field_probability(effect_config.dropout_multi_field_prob.value());
                        }
                        if (effect_config.dropout_single_field_prob.has_value()) {
                            dropout->set_single_field_probability(effect_config.dropout_single_field_prob.value());
                        }
                        effects.push_back(std::move(dropout));
                        ENCODE_ORC_LOG_DEBUG("Added Dropout simulator with density: {}", density);
                    }
                    else if (effect_config.type == "phase-error") {
                        // Phase error effects would be created here
                        ENCODE_ORC_LOG_WARN("Phase-error effect type '{}' not yet implemented in main.cpp", effect_config.type);
                    }
                    else {
                        ENCODE_ORC_LOG_WARN("Unknown effect type '{}' in pipeline configuration", effect_config.type);
                    }
                }
            }

            // Build pipeline for this section
            VideoEncoderPipeline::Builder pipeline_builder;
            pipeline_builder.set_system(system)
                            .set_parameters(params);
            
            // Apply filter settings from preprocessing config (defaults: chroma=true, luma=false)
            bool enable_chroma_filter = true;
            bool enable_luma_filter = false;
            if (preprocessing_config != nullptr && preprocessing_config->filters.has_value()) {
                enable_chroma_filter = preprocessing_config->filters->chroma.enabled;
                enable_luma_filter = preprocessing_config->filters->luma.enabled;
            }
            pipeline_builder.enable_chroma_filter(enable_chroma_filter)
                            .enable_luma_filter(enable_luma_filter);
            
            // Enable Y/C output if format is Y/C
            if (config.output.format == "pal-yc" || config.output.format == "ntsc-yc") {
                pipeline_builder.enable_yc_output(true);
            }

            if (!generators.empty()) {
                pipeline_builder.set_metadata_generators(std::move(generators));
            }

            if (!effects.empty()) {
                pipeline_builder.set_field_effects(std::move(effects));
            }

            auto pipeline = pipeline_builder.build();
            if (!pipeline) {
                ENCODE_ORC_LOG_ERROR("Failed to create pipeline for section '{}'", section.name);
                return 1;
            }

            // Lambda to write a single encoded frame (must be called sequentially)
            auto write_encoded_frame = [&](const EncodedResult& result) -> bool {
                if (!result.success) {
                    ENCODE_ORC_LOG_ERROR("Failed to encode frame {}: {}", 
                                       result.global_frame, result.error_message);
                    return false;
                }
                
                const auto& encoded_frame = result.encoded_frame;
                int32_t field_number = result.global_frame * 2;
                
                // Write fields based on output format
                if (yc_writer) {
                    // Write Y and C fields separately
                    const Field* y_field1 = encoded_frame.field1().y_field_const();
                    const Field* c_field1 = encoded_frame.field1().c_field_const();
                    const Field* y_field2 = encoded_frame.field2().y_field_const();
                    const Field* c_field2 = encoded_frame.field2().c_field_const();
                    
                    if (!y_field1 || !c_field1 || !y_field2 || !c_field2) {
                        ENCODE_ORC_LOG_ERROR("Y/C fields not generated for frame {}", result.global_frame);
                        return false;
                    }
                    
                    if (!yc_writer->write_y_field(*y_field1) || !yc_writer->write_y_field(*y_field2)) {
                        ENCODE_ORC_LOG_ERROR("Failed to write Y fields for frame {}", result.global_frame);
                        return false;
                    }
                    if (!yc_writer->write_c_field(*c_field1) || !yc_writer->write_c_field(*c_field2)) {
                        ENCODE_ORC_LOG_ERROR("Failed to write C fields for frame {}", result.global_frame);
                        return false;
                    }
                } else {
                    // Write composite fields normally
                    if (!writer->write_field(encoded_frame.field1()) || !writer->write_field(encoded_frame.field2())) {
                        ENCODE_ORC_LOG_ERROR("Failed to write encoded fields for frame {}", result.global_frame);
                        return false;
                    }
                }

                // Write audio samples (if enabled)
                if (sound_enabled && audio_writer) {
                    if (!audio_writer->write_samples(encoded_frame.field1().audio()) ||
                        !audio_writer->write_samples(encoded_frame.field2().audio())) {
                        ENCODE_ORC_LOG_ERROR("Failed to write audio for frame {}", result.global_frame);
                        return false;
                    }
                }
                
                // Collect dropout metadata from the dropout effect
                DropoutSimulator* dropout_simulator = pipeline->get_dropout_simulator();
                if (dropout_simulator) {
                    // Field 1 dropouts (field_number)
                    auto field1_dropouts = dropout_simulator->get_field_dropouts(field_number);
                    for (const auto& dropout : field1_dropouts) {
                        int32_t line = std::get<0>(dropout);
                        int32_t startx = std::get<1>(dropout);
                        int32_t endx = std::get<2>(dropout);
                        pre_metadata.add_dropout(field_number, line, startx, endx);
                    }
                    
                    // Field 2 dropouts (field_number + 1)
                    auto field2_dropouts = dropout_simulator->get_field_dropouts(field_number + 1);
                    for (const auto& dropout : field2_dropouts) {
                        int32_t line = std::get<0>(dropout);
                        int32_t startx = std::get<1>(dropout);
                        int32_t endx = std::get<2>(dropout);
                        pre_metadata.add_dropout(field_number + 1, line, startx, endx);
                    }
                }

                if ((result.section_frame + 1) % 10 == 0 || result.section_frame == section_frames - 1) {
                    ENCODE_ORC_LOG_DEBUG("Writing field {} / {}", (result.global_frame + 1) * 2, total_frames * 2);
                }

                return true;
            };

            // Lambda to prepare an encoding task
            auto prepare_task = [&](FrameBuffer&& frame_buffer, int32_t section_frame) -> EncodingTask {
                EncodingTask task;
                task.frame_buffer = std::move(frame_buffer);
                task.section_frame = section_frame;
                task.global_frame = frame_offset + section_frame;
                task.field_number = task.global_frame * 2;
                task.vitc_frame_offset = current_vitc_offset;
                
                task.vbi_data_field1 = nullptr;
                task.vbi_data_field2 = nullptr;
                if (needs_vbi_data) {
                    if (task.field_number < static_cast<int32_t>(pre_metadata.vbi_data.size()) &&
                        pre_metadata.vbi_data[task.field_number].has_value()) {
                        task.vbi_data_field1 = &pre_metadata.vbi_data[task.field_number].value();
                    }
                    int32_t field2_number = task.field_number + 1;
                    if (field2_number < static_cast<int32_t>(pre_metadata.vbi_data.size()) &&
                        pre_metadata.vbi_data[field2_number].has_value()) {
                        task.vbi_data_field2 = &pre_metadata.vbi_data[field2_number].value();
                    }
                }
                
                return task;
            };

            if (section.yuv422_image_source) {
                std::string yuv422_file = section.yuv422_image_source->file;
                section_frames = section.duration.value();

                int32_t img_width = 720;
                int32_t img_height = (system == VideoSystem::PAL) ? 576 : 480;

                YUV422Loader yuv422_loader;
                if (!yuv422_loader.open(yuv422_file, img_width, img_height)) {
                    ENCODE_ORC_LOG_ERROR("Failed to open YUV422 file: {}", yuv422_file);
                    return 1;
                }

                FrameBuffer frame_buffer;
                std::string load_error;
                if (!yuv422_loader.load_frame(0, frame_buffer, load_error)) {
                    ENCODE_ORC_LOG_ERROR("Failed to load YUV422 frame: {}", load_error);
                    yuv422_loader.close();
                    return 1;
                }
                yuv422_loader.close();

                if (num_threads > 1) {
                    // Multi-threaded encoding
                    OrderedQueue<EncodedResult> result_queue;
                    
                    // Submit all encoding tasks
                    // Capture pipeline raw pointer by value to avoid dangling reference
                    auto* pipeline_ptr = pipeline.get();
                    SectionAudio* section_audio_ptr = sound_enabled ? &section_audio : nullptr;
                    for (int32_t i = 0; i < section_frames; ++i) {
                        FrameBuffer fb_copy = frame_buffer;  // Copy frame buffer for each task
                        EncodingTask task = prepare_task(std::move(fb_copy), i);
                        
                        thread_pool->enqueue([task, i, &result_queue, pipeline_ptr, section_audio_ptr, &audio_mutex]() mutable {
                            EncodedResult result = encode_single_frame(std::move(task), pipeline_ptr, section_audio_ptr, &audio_mutex);
                            result_queue.push(i, std::move(result));
                        });
                    }
                    
                    // Write results in order
                    for (int32_t i = 0; i < section_frames; ++i) {
                        EncodedResult result;
                        if (!result_queue.wait_and_pop(i, result)) {
                            ENCODE_ORC_LOG_ERROR("Failed to get encoding result for frame {}", i);
                            return 1;
                        }
                        if (!write_encoded_frame(result)) {
                            return 1;
                        }
                    }
                    thread_pool->wait_all();
                } else {
                    // Single-threaded encoding
                    SectionAudio* section_audio_ptr = sound_enabled ? &section_audio : nullptr;
                    for (int32_t i = 0; i < section_frames; ++i) {
                        FrameBuffer fb_copy = frame_buffer;
                        EncodingTask task = prepare_task(std::move(fb_copy), i);
                        EncodedResult result = encode_single_frame(std::move(task), pipeline.get(), section_audio_ptr, nullptr);
                        if (!write_encoded_frame(result)) {
                            return 1;
                        }
                    }
                }
            } else if (section.png_image_source) {
                std::string png_file = section.png_image_source->file;
                section_frames = section.duration.value();

                PNGLoader png_loader;
                std::string load_error;
                if (!png_loader.open(png_file, load_error)) {
                    ENCODE_ORC_LOG_ERROR("Failed to open PNG file: {}", load_error);
                    return 1;
                }

                FrameBuffer frame_buffer;
                if (!png_loader.load_frame(0, frame_buffer, load_error)) {
                    ENCODE_ORC_LOG_ERROR("Failed to load PNG frame: {}", load_error);
                    png_loader.close();
                    return 1;
                }
                png_loader.close();

                if (num_threads > 1) {
                    // Multi-threaded encoding
                    OrderedQueue<EncodedResult> result_queue;
                    
                    // Submit all encoding tasks
                    // Capture pipeline raw pointer by value to avoid dangling reference
                    auto* pipeline_ptr = pipeline.get();
                    SectionAudio* section_audio_ptr = sound_enabled ? &section_audio : nullptr;
                    for (int32_t i = 0; i < section_frames; ++i) {
                        FrameBuffer fb_copy = frame_buffer;
                        EncodingTask task = prepare_task(std::move(fb_copy), i);
                        
                        thread_pool->enqueue([task, i, &result_queue, pipeline_ptr, section_audio_ptr, &audio_mutex]() mutable {
                            EncodedResult result = encode_single_frame(std::move(task), pipeline_ptr, section_audio_ptr, &audio_mutex);
                            result_queue.push(i, std::move(result));
                        });
                    }
                    
                    // Write results in order
                    for (int32_t i = 0; i < section_frames; ++i) {
                        EncodedResult result;
                        if (!result_queue.wait_and_pop(i, result)) {
                            ENCODE_ORC_LOG_ERROR("Failed to get encoding result for frame {}", i);
                            return 1;
                        }
                        if (!write_encoded_frame(result)) {
                            return 1;
                        }
                    }
                    thread_pool->wait_all();
                } else {
                    // Single-threaded encoding
                    SectionAudio* section_audio_ptr = sound_enabled ? &section_audio : nullptr;
                    for (int32_t i = 0; i < section_frames; ++i) {
                        FrameBuffer fb_copy = frame_buffer;
                        EncodingTask task = prepare_task(std::move(fb_copy), i);
                        EncodedResult result = encode_single_frame(std::move(task), pipeline.get(), section_audio_ptr, nullptr);
                        if (!write_encoded_frame(result)) {
                            return 1;
                        }
                    }
                }
            } else if (section.mov_file_source) {
                std::string mov_file = section.mov_file_source->file;
                int32_t start_frame = section.mov_file_source->start_frame.value_or(0);
                section_frames = section.duration.value();

                MOVLoader mov_loader;
                std::string load_error;
                if (!mov_loader.open(mov_file, load_error)) {
                    ENCODE_ORC_LOG_ERROR("Failed to open MOV file: {}", load_error);
                    return 1;
                }

                if (num_threads > 1) {
                    // Multi-threaded encoding
                    OrderedQueue<EncodedResult> result_queue;
                    
                    // Submit encoding tasks
                    // Capture pipeline raw pointer by value to avoid dangling reference
                    auto* pipeline_ptr = pipeline.get();
                    SectionAudio* section_audio_ptr = sound_enabled ? &section_audio : nullptr;
                    for (int32_t i = 0; i < section_frames; ++i) {
                        FrameBuffer frame_buffer;
                        if (!mov_loader.load_frame(start_frame + i, frame_buffer, load_error)) {
                            ENCODE_ORC_LOG_ERROR("Failed to load MOV frame {}: {}", start_frame + i, load_error);
                            mov_loader.close();
                            return 1;
                        }

                        EncodingTask task = prepare_task(std::move(frame_buffer), i);
                        
                        thread_pool->enqueue([task, i, &result_queue, pipeline_ptr, section_audio_ptr, &audio_mutex]() mutable {
                            EncodedResult result = encode_single_frame(std::move(task), pipeline_ptr, section_audio_ptr, &audio_mutex);
                            result_queue.push(i, std::move(result));
                        });
                    }
                    mov_loader.close();
                    
                    // Write results in order
                    for (int32_t i = 0; i < section_frames; ++i) {
                        EncodedResult result;
                        if (!result_queue.wait_and_pop(i, result)) {
                            ENCODE_ORC_LOG_ERROR("Failed to get encoding result for frame {}", i);
                            return 1;
                        }
                        if (!write_encoded_frame(result)) {
                            return 1;
                        }
                    }
                    thread_pool->wait_all();
                } else {
                    // Single-threaded encoding
                    SectionAudio* section_audio_ptr = sound_enabled ? &section_audio : nullptr;
                    for (int32_t i = 0; i < section_frames; ++i) {
                        FrameBuffer frame_buffer;
                        if (!mov_loader.load_frame(start_frame + i, frame_buffer, load_error)) {
                            ENCODE_ORC_LOG_ERROR("Failed to load MOV frame {}: {}", start_frame + i, load_error);
                            mov_loader.close();
                            return 1;
                        }

                        EncodingTask task = prepare_task(std::move(frame_buffer), i);
                        EncodedResult result = encode_single_frame(std::move(task), pipeline.get(), section_audio_ptr, nullptr);
                        if (!write_encoded_frame(result)) {
                            mov_loader.close();
                            return 1;
                        }
                    }
                    mov_loader.close();
                }
            } else if (section.mp4_file_source) {
                std::string mp4_file = section.mp4_file_source->file;
                int32_t start_frame = section.mp4_file_source->start_frame.value_or(0);
                section_frames = section.duration.value();

                MP4Loader mp4_loader;
                std::string load_error;
                if (!mp4_loader.open(mp4_file, load_error)) {
                    ENCODE_ORC_LOG_ERROR("Failed to open MP4 file: {}", load_error);
                    return 1;
                }

                // Batch decode frames in chunks to reduce ffmpeg overhead
                constexpr int32_t BATCH_SIZE = 50;  // Decode 50 frames at a time
                
                if (num_threads > 1) {
                    // Multi-threaded encoding
                    OrderedQueue<EncodedResult> result_queue;
                    
                    // Capture pipeline raw pointer by value to avoid dangling reference
                    auto* pipeline_ptr = pipeline.get();
                    SectionAudio* section_audio_ptr = sound_enabled ? &section_audio : nullptr;
                    for (int32_t batch_start = 0; batch_start < section_frames; batch_start += BATCH_SIZE) {
                        int32_t batch_count = std::min(BATCH_SIZE, section_frames - batch_start);
                        
                        std::vector<FrameBuffer> frame_buffers;
                        if (!mp4_loader.load_frames(start_frame + batch_start, batch_count, frame_buffers, load_error)) {
                            ENCODE_ORC_LOG_ERROR("Failed to load MP4 frames {}-{}: {}", 
                                               start_frame + batch_start, 
                                               start_frame + batch_start + batch_count - 1, 
                                               load_error);
                            mp4_loader.close();
                            return 1;
                        }

                        for (int32_t i = 0; i < batch_count; ++i) {
                            int32_t frame_index = batch_start + i;
                            EncodingTask task = prepare_task(std::move(frame_buffers[i]), frame_index);
                            
                            thread_pool->enqueue([task, frame_index, &result_queue, pipeline_ptr, section_audio_ptr, &audio_mutex]() mutable {
                                EncodedResult result = encode_single_frame(std::move(task), pipeline_ptr, section_audio_ptr, &audio_mutex);
                                result_queue.push(frame_index, std::move(result));
                            });
                        }
                    }
                    mp4_loader.close();
                    
                    // Write results in order
                    for (int32_t i = 0; i < section_frames; ++i) {
                        EncodedResult result;
                        if (!result_queue.wait_and_pop(i, result)) {
                            ENCODE_ORC_LOG_ERROR("Failed to get encoding result for frame {}", i);
                            return 1;
                        }
                        if (!write_encoded_frame(result)) {
                            return 1;
                        }
                    }
                    thread_pool->wait_all();
                } else {
                    // Single-threaded encoding
                    SectionAudio* section_audio_ptr = sound_enabled ? &section_audio : nullptr;
                    for (int32_t batch_start = 0; batch_start < section_frames; batch_start += BATCH_SIZE) {
                        int32_t batch_count = std::min(BATCH_SIZE, section_frames - batch_start);
                        
                        std::vector<FrameBuffer> frame_buffers;
                        if (!mp4_loader.load_frames(start_frame + batch_start, batch_count, frame_buffers, load_error)) {
                            ENCODE_ORC_LOG_ERROR("Failed to load MP4 frames {}-{}: {}", 
                                               start_frame + batch_start, 
                                               start_frame + batch_start + batch_count - 1, 
                                               load_error);
                            mp4_loader.close();
                            return 1;
                        }

                        for (int32_t i = 0; i < batch_count; ++i) {
                            int32_t frame_index = batch_start + i;
                            EncodingTask task = prepare_task(std::move(frame_buffers[i]), frame_index);
                            EncodedResult result = encode_single_frame(std::move(task), pipeline.get(), section_audio_ptr, nullptr);
                            if (!write_encoded_frame(result)) {
                                mp4_loader.close();
                                return 1;
                            }
                        }
                    }
                    mp4_loader.close();
                }
            }

            frame_offset += section_frames;
            vitc_frame_offset += section_frames;  // Advance VITC offset for next section
            ENCODE_ORC_LOG_INFO("  ✓ Encoded {} frames", section_frames);
        }
    }

    // Close writers
    if (yc_writer) {
        yc_writer->close();
    }
    if (writer) {
        writer->close();
    }
    if (audio_writer) {
        audio_writer->close();
    }
    
    // Generate metadata for entire file (only for TBC writer, not standard writer)
    if (config.output.writer != "standard") {
        std::string meta_error;
        std::string metadata_filename = base_filename + ".tbc.db";
        
        if (!generate_metadata(config, system, total_frames, metadata_filename, meta_error, nullptr, &pre_metadata)) {
            ENCODE_ORC_LOG_ERROR("Metadata generation error: {}", meta_error);
            return 1;
        }
    }
    
    ENCODE_ORC_LOG_INFO("Successfully generated {} frames", total_frames);
    
    // Log output file(s)
    if (config.output.format == "pal-yc" || config.output.format == "ntsc-yc") {
        ENCODE_ORC_LOG_INFO("Output files: {}.tbcy, {}.tbcc", output_filename, output_filename);
    } else {
        ENCODE_ORC_LOG_INFO("Output file: {}", output_filename);
    }
    
    return 0;
    
    } catch (const std::exception& e) {
        ENCODE_ORC_LOG_ERROR("Fatal error: {}", e.what());
        return 1;
    } catch (...) {
        ENCODE_ORC_LOG_ERROR("Fatal error: Unknown exception occurred");
        return 1;
    }
}
