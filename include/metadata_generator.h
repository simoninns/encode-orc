/*
 * File:        metadata_generator.h
 * Module:      encode-orc
 * Purpose:     Generate metadata for TBC files with support for CAV, CLV chapter, and CLV timecode modes
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: 2026 Simon Inns
 */

#ifndef ENCODE_ORC_METADATA_GENERATOR_H
#define ENCODE_ORC_METADATA_GENERATOR_H

#include "video_parameters.h"
#include "yaml_config.h"
#include <string>
#include <cstdint>

namespace encode_orc {

/**
 * @brief Generate metadata database for a complete TBC file
 * 
 * Generates VBI data for the entire file based on section configuration,
 * preserving continuous timecode/chapter progression across all sections.
 * 
 * @param config YAML project configuration with all sections
 * @param system Video system (PAL or NTSC)
 * @param total_frames Total number of frames in output file
 * @param output_db Path to output metadata database file
 * @param error_message Output parameter for error description
 * @return true on success, false on error
 */
bool generate_metadata(const YAMLProjectConfig& config,
                      VideoSystem system,
                      int32_t total_frames,
                      const std::string& output_db,
                      std::string& error_message);

} // namespace encode_orc

#endif // ENCODE_ORC_METADATA_GENERATOR_H
