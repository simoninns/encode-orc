/*
 * File:        metadata.h
 * Module:      encode-orc
 * Purpose:     TBC metadata structures matching ld-decode schema
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: 2026 Simon Inns
 */

#ifndef ENCODE_ORC_METADATA_H
#define ENCODE_ORC_METADATA_H

#include "video_parameters.h"
#include <cstdint>
#include <string>
#include <vector>
#include <optional>

namespace encode_orc {

/**
 * @brief Field-level metadata matching ld-decode's field_record table
 */
struct FieldMetadata {
    int32_t field_id = 0;                   // Zero-indexed field number
    int32_t audio_samples = 0;              // Audio samples for this field
    int32_t decode_faults = 0;              // Bit flags for decode faults
    double disk_loc = 0.0;                  // Location in file (in fields)
    int32_t efm_t_values = 0;               // EFM T-values (LaserDisc)
    int32_t field_phase_id = 0;             // Position in 4-field (NTSC) or 8-field (PAL) sequence
    int64_t file_loc = 0;                   // Sample number where field starts
    bool is_first_field = false;            // First or second field
    double median_burst_ire = 0.0;          // Median burst level in IRE
    bool pad = false;                       // Field is padding (no valid video)
    int32_t sync_conf = 100;                // Sync confidence (0-100)
    
    // NTSC-specific fields (set to std::nullopt for PAL)
    std::optional<bool> ntsc_is_fm_code_data_valid;
    std::optional<int32_t> ntsc_fm_code_data;
    std::optional<bool> ntsc_field_flag;
    std::optional<bool> ntsc_is_video_id_data_valid;
    std::optional<int32_t> ntsc_video_id_data;
    std::optional<bool> ntsc_white_flag;
};

/**
 * @brief PCM audio parameters matching ld-decode's pcm_audio_parameters table
 */
struct PCMAudioParameters {
    int32_t bits = 16;
    bool is_signed = true;
    bool is_little_endian = true;
    double sample_rate = 48000.0;
};

/**
 * @brief VITS metrics (optional) matching ld-decode's vits_metrics table
 */
struct VITSMetrics {
    double b_psnr = 0.0;    // Black line PSNR
    double w_snr = 0.0;     // White area SNR
};

/**
 * @brief VBI data (optional) matching ld-decode's vbi table
 */
struct VBIData {
    int32_t vbi0 = 0;   // VBI line 16 data
    int32_t vbi1 = 0;   // VBI line 17 data
    int32_t vbi2 = 0;   // VBI line 18 data
};

/**
 * @brief Dropout location matching ld-decode's drop_outs table
 */
struct Dropout {
    int32_t field_line = 0;
    int32_t startx = 0;
    int32_t endx = 0;
};

/**
 * @brief VITC data (optional) matching ld-decode's vitc table
 */
struct VITCData {
    int32_t vitc0 = 0;
    int32_t vitc1 = 0;
    int32_t vitc2 = 0;
    int32_t vitc3 = 0;
    int32_t vitc4 = 0;
    int32_t vitc5 = 0;
    int32_t vitc6 = 0;
    int32_t vitc7 = 0;
};

/**
 * @brief Closed caption data (optional) matching ld-decode's closed_caption table
 */
struct ClosedCaptionData {
    int32_t data0 = -1;  // -1 if invalid, 0 if present but no data
    int32_t data1 = -1;  // -1 if invalid, 0 if present but no data
};

/**
 * @brief Complete capture metadata matching ld-decode's metadata schema
 * 
 * This structure contains all metadata for a TBC capture, organized to match
 * the SQLite database schema used by ld-decode tools.
 */
struct CaptureMetadata {
    // Capture-level data
    int32_t capture_id = 1;
    VideoParameters video_params;
    std::string git_branch = "main";
    std::string git_commit = "unknown";
    std::string capture_notes;
    
    // Audio parameters (optional)
    std::optional<PCMAudioParameters> audio_params;
    
    // Field-level data
    std::vector<FieldMetadata> fields;
    
    // Optional per-field data
    std::vector<std::optional<VITSMetrics>> vits_metrics;
    std::vector<std::optional<VBIData>> vbi_data;
    std::vector<std::optional<VITCData>> vitc_data;
    std::vector<std::optional<ClosedCaptionData>> closed_caption_data;
    
    // Dropouts (can have multiple per field)
    std::vector<Dropout> dropouts;
    
    /**
     * @brief Initialize metadata for a new capture
     * @param system Video system (PAL or NTSC)
     * @param num_fields Total number of fields to generate
     */
    void initialize(VideoSystem system, int32_t num_fields) {
        // Initialize video parameters based on system
        if (system == VideoSystem::PAL) {
            video_params = VideoParameters::create_pal_composite();
        } else {
            video_params = VideoParameters::create_ntsc_composite();
        }
        
        video_params.number_of_sequential_fields = num_fields;
        
        // Reserve space for field metadata
        fields.reserve(num_fields);
        
        // Create field metadata entries
        for (int32_t i = 0; i < num_fields; ++i) {
            FieldMetadata field;
            field.field_id = i;
            field.is_first_field = (i % 2) == 0;
            field.field_phase_id = (system == VideoSystem::NTSC) ? (i % 4) : (i % 8);
            field.file_loc = i * video_params.field_width * video_params.field_height;
            field.disk_loc = static_cast<double>(i);
            field.sync_conf = 100;  // Perfect sync for generated content
            field.pad = false;
            
            // Set NTSC-specific fields
            if (system == VideoSystem::NTSC) {
                field.ntsc_field_flag = field.is_first_field;
                field.ntsc_is_fm_code_data_valid = false;
                field.ntsc_fm_code_data = 0;
                field.ntsc_is_video_id_data_valid = false;
                field.ntsc_video_id_data = 0;
                field.ntsc_white_flag = false;
            }
            
            fields.push_back(field);
        }
    }
    
    /**
     * @brief Add a dropout to the metadata
     * @param field_id Field containing the dropout
     * @param line Line number within the field
     * @param start_x Start pixel position
     * @param end_x End pixel position
     */
    void add_dropout(int32_t field_id, int32_t line, int32_t start_x, int32_t end_x) {
        Dropout dropout;
        dropout.field_line = line;
        dropout.startx = start_x;
        dropout.endx = end_x;
        dropouts.push_back(dropout);
    }
};

} // namespace encode_orc

#endif // ENCODE_ORC_METADATA_H
