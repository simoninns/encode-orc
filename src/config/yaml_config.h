/*
 * File:        yaml_config.h
 * Module:      encode-orc
 * Purpose:     YAML project configuration parser interface
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: 2026 Simon Inns
 */

#ifndef ENCODE_ORC_YAML_CONFIG_H
#define ENCODE_ORC_YAML_CONFIG_H

#include "video_parameters.h"
#include <string>
#include <vector>
#include <optional>
#include <cstdint>
#include <map>

namespace encode_orc {

/**
 * @brief VBI line configuration (DEPRECATED - use pipeline.metadata.generators)
 */
struct VBILineConfig {
    bool enabled = false;
    std::optional<std::string> auto_mode;  // "timecode", "picture-number", "chapter", "status"
    std::optional<std::vector<uint8_t>> bytes;  // Manual byte values
    std::optional<uint8_t> status_code;  // For status mode
};

/**
 * @brief VBI configuration for a section (DEPRECATED - use pipeline.metadata.generators)
 */
struct VBIConfig {
    bool enabled = false;
    VBILineConfig line16;
    VBILineConfig line17;
    VBILineConfig line18;
};

/**
 * @brief VITS configuration for a section (DEPRECATED - use pipeline.metadata.generators)
 */
struct VITSConfig {
    bool enabled = false;
    // Future: custom line overrides
};

/**
 * @brief Pipeline metadata generator configuration (NEW)
 */
struct PipelineGeneratorConfig {
    std::string type;           // "biphase-vbi", "vitc", "vits-pal", "vits-ntsc", "color-burst"
    bool enabled = true;        // Whether this generator is active
    
    // Configuration specific to generator type
    
    // For biphase-vbi:
    std::vector<int32_t> lines;  // Lines to encode VBI data on (0-indexed)
    std::string format = "picture-number";  // "picture-number" or "timecode"
    
    // For vitc:
    int32_t start_frame_offset = 0;  // Frame number offset for timecode
    
    // For vits-pal / vits-ntsc:
    struct VITSSignal {
        int32_t line = 0;         // Absolute line number (1-indexed in YAML, 1-525 for NTSC, 1-625 for PAL)
        std::string signal;       // "itu-composite", "uk-national", "itu-combination", "multiburst"
    };
    std::vector<VITSSignal> vits_signals;
    
    // For color-burst:
    // No additional config needed currently
};

/**
 * @brief Chroma filter configuration
 */
struct ChromaFilterConfig {
    bool enabled = true;  // Default: enabled to prevent artifacts
    // Filter type is determined by video system (PAL/NTSC)
    // PAL: 1.3 MHz Gaussian filter (13 taps)
    // NTSC: 1.3 MHz filter (9 taps) or narrowband Q filter (23 taps)
};

/**
 * @brief Luma filter configuration
 */
struct LumaFilterConfig {
    bool enabled = false;  // Default: disabled (luma typically not filtered)
    // If enabled, applies low-pass filter to Y component
};

/**
 * @brief Filter configuration for a section
 */
struct FilterConfig {
    ChromaFilterConfig chroma;
    LumaFilterConfig luma;
};

/**
 * @brief Pipeline metadata configuration (NEW)
 */
struct PipelineMetadataConfig {
    std::vector<PipelineGeneratorConfig> generators;
};

/**
 * @brief Field effect configuration (Phase 6)
 */
struct FieldEffectConfig {
    std::string type;           // "noise", "dropout", "phase-error"
    bool enabled = false;       // Whether this effect is active
    
    // For noise effect:
    std::optional<double> snr_db;         // Signal-to-noise ratio in dB
    std::optional<double> noise_level_db; // Direct noise level in dB (alternative to SNR)
    
    // For dropout effect:
    std::optional<std::string> dropout_pattern;  // "random", "periodic", "specific-lines"
    std::optional<double> dropout_density;       // Dropout density (0.0-1.0)
    std::optional<std::vector<int32_t>> dropout_lines;  // Specific lines to drop
    
    // For phase-error effect:
    std::optional<double> phase_jitter_samples;  // Maximum phase jitter
    std::optional<double> frequency_hz;          // Modulation frequency
    
    // Common to all effects:
    std::optional<uint32_t> seed;  // Random seed for reproducibility
};

/**
 * @brief Pipeline effects configuration (Phase 6)
 */
struct PipelineEffectsConfig {
    std::vector<FieldEffectConfig> effects;
};

/**
 * @brief Pipeline preprocessing configuration (Phase 6)
 */
struct PipelinePreprocessingConfig {
    std::optional<FilterConfig> filters;  // Chroma and luma filters
};

/**
 * @brief Pipeline configuration (NEW)
 */
struct PipelineConfig {
    std::optional<PipelineMetadataConfig> metadata;
    std::optional<PipelinePreprocessingConfig> preprocessing;  // Phase 6
    std::optional<PipelineEffectsConfig> effects;              // Phase 6
};

/**
 * @brief Biphase VBI configuration for a section (LaserDisc metadata)
 */
struct BiphaseVBIConfig {
    std::string disc_area = "programme-area";  // lead-in, programme-area, lead-out
    
    // CAV mode
    std::optional<int32_t> picture_start;
    
    // CLV mode
    std::optional<int32_t> chapter;
    std::optional<std::string> timecode_start;  // Format: HH:MM:SS:FF
    
    // Picture-numbers mode
    std::optional<int32_t> start;
    
    // VBI and VITS
    VBIConfig vbi;
    VITSConfig vits;
};

/**
 * @brief RGB30 raw image source configuration
 */
struct YUV422ImageSource {
    std::string file;  // Path to raw RGB30 file
};

/**
 * @brief PNG image source configuration
 */
struct PNGImageSource {
    std::string file;  // Path to PNG image file
};

/**
 * @brief MOV file source configuration
 */
struct MOVFileSource {
    std::string file;  // Path to MOV file (v210 or other ffmpeg-supported format)
    std::optional<int32_t> start_frame;  // Optional: which frame to start from (0-indexed, default: 0)
};

/**
 * @brief MP4 file source configuration
 */
struct MP4FileSource {
    std::string file;  // Path to MP4 file (H.264, H.265, or other ffmpeg-supported codec)
    std::optional<int32_t> start_frame;  // Optional: which frame to start from (0-indexed, default: 0)
};

/**
 * @brief Video section configuration
 */
struct VideoSection {
    std::string name;
    std::optional<int32_t> duration;  // Required for RGB30 images and MOV files
    
    std::string source_type;  // "yuv422-image", "png-image", "mov-file", or "mp4-file"
    std::optional<YUV422ImageSource> yuv422_image_source;
    std::optional<PNGImageSource> png_image_source;
    std::optional<MOVFileSource> mov_file_source;
    std::optional<MP4FileSource> mp4_file_source;
    
    std::optional<FilterConfig> filters;  // Optional filter settings
    
    // Generator-specific metadata (matches pipeline generator types)
    std::optional<BiphaseVBIConfig> biphase_vbi;  // For biphase-vbi generator (LaserDisc)
};

/**
 * @brief Video signal level configuration (16-bit IRE values)
 */
struct VideoLevelsConfig {
    std::optional<int32_t> blanking_16b_ire;  // Optional: blanking level
    std::optional<int32_t> black_16b_ire;     // Optional: black level
    std::optional<int32_t> white_16b_ire;     // Optional: white/peak level
};

/**
 * @brief Output configuration
 */
struct OutputConfig {
    std::string filename;
    std::string format;  // pal-composite, ntsc-composite, pal-yc, ntsc-yc
    std::string mode = "combined";  // combined (default), separate-yc
    std::string writer = "tbc";  // tbc (default), standard
    std::string metadata_decoder = "encode-orc";  // decoder string in metadata (default: encode-orc)
    std::optional<VideoLevelsConfig> video_levels;  // Optional: override video signal levels
};

/**
 * @brief Complete YAML project configuration
 */
struct YAMLProjectConfig {
    std::string name;
    std::string description;
    OutputConfig output;
    
    // Pipeline configuration (Phase 4+) - REQUIRED
    PipelineConfig pipeline;
    
    std::vector<VideoSection> sections;
};

/**
 * @brief Parse YAML project configuration from file
 * 
 * @param filename Path to YAML file
 * @param config Output configuration object
 * @return true on success, false on error
 */
bool parse_yaml_config(const std::string& filename, YAMLProjectConfig& config, 
                       std::string& error_message);

/**
 * @brief Validate YAML configuration
 * 
 * @param config Configuration to validate
 * @return true if valid, false otherwise
 */
bool validate_yaml_config(const YAMLProjectConfig& config, std::string& error_message);

} // namespace encode_orc

#endif // ENCODE_ORC_YAML_CONFIG_H
