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
 * @brief VBI line configuration
 */
struct VBILineConfig {
    bool enabled = false;
    std::optional<std::string> auto_mode;  // "timecode", "picture-number", "chapter", "status"
    std::optional<std::vector<uint8_t>> bytes;  // Manual byte values
    std::optional<uint8_t> status_code;  // For status mode
};

/**
 * @brief VBI configuration for a section
 */
struct VBIConfig {
    bool enabled = false;
    VBILineConfig line16;
    VBILineConfig line17;
    VBILineConfig line18;
};

/**
 * @brief VITS configuration for a section
 */
struct VITSConfig {
    bool enabled = false;
    // Future: custom line overrides
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
 * @brief LaserDisc configuration for a section
 */
struct LaserDiscConfig {
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
struct RGB30ImageSource {
    std::string file;  // Path to raw RGB30 file
};

/**
 * @brief Video section configuration
 */
struct VideoSection {
    std::string name;
    std::optional<int32_t> duration;  // Required for RGB30 images
    
    std::string source_type;  // "rgb30-image"
    std::optional<RGB30ImageSource> rgb30_image_source;
    
    std::optional<FilterConfig> filters;  // Optional filter settings
    std::optional<LaserDiscConfig> laserdisc;
};

/**
 * @brief Output configuration
 */
struct OutputConfig {
    std::string filename;
    std::string format;  // pal-composite, ntsc-composite, pal-yc, ntsc-yc
    std::string mode = "combined";  // combined (default), separate-yc
};

/**
 * @brief Project-level LaserDisc settings
 */
struct ProjectLaserDiscConfig {
    std::string standard = "none";  // iec60857-1986, iec60856-1986, none
    std::string mode = "none";  // cav, clv, picture-numbers, none
};

/**
 * @brief Complete YAML project configuration
 */
struct YAMLProjectConfig {
    std::string name;
    std::string description;
    OutputConfig output;
    ProjectLaserDiscConfig laserdisc;
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
