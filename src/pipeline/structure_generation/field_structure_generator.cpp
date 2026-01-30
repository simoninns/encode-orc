/*
 * File:        field_structure_generator.cpp
 * Module:      encode-orc
 * Purpose:     Field structure generation (sync, blanking, VBI layout)
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: 2026 Simon Inns
 */

#include "field_structure_generator.h"
#include "color_burst_generator.h"
#include "logging.h"
#include <algorithm>

namespace encode_orc {

// Define the sync pattern lookup tables
constexpr decltype(FieldStructureGenerator::PAL_SYNC_PATTERN) FieldStructureGenerator::PAL_SYNC_PATTERN;
constexpr decltype(FieldStructureGenerator::NTSC_SYNC_PATTERN) FieldStructureGenerator::NTSC_SYNC_PATTERN;

FieldStructureGenerator::FieldStructureGenerator(const VideoParameters& params)
    : params_(params),
      sync_level_(0),  // Sync tip is always at 0
      blanking_level_(params.blanking_16b_ire),
      white_level_(params.white_16b_ire),
      sample_rate_(params.sample_rate) {
}

StructuredField FieldStructureGenerator::create_field_structure(
    const Field& /* source_field */,
    bool is_first_field,
    int32_t field_number,
    VideoSystem system) {
    
    StructuredField result;
    
    // Determine actual field height for this field
    // NTSC: Field 1 has 262 lines, Field 2 has 263 lines
    // PAL: Field 1 has 312 lines, Field 2 has 313 lines
    int32_t actual_field_height = is_first_field ? 
                                  params_.field1_height : 
                                  params_.field2_height;
    
    // Create field with proper dimensions
    result.field_data = Field(params_.field_width, actual_field_height);
    
    // Generate field structure line by line
    for (int32_t line = 0; line < actual_field_height; ++line) {
        uint16_t* line_buffer = result.field_data.line_data(line);
        
        // Get sync pattern for this line to determine fill level
        auto [first_pulse, second_pulse] = get_sync_pattern_for_line(line, is_first_field, system);
        
        // Step 1: Fill line with appropriate level based on sync pattern
        // BR_BR lines: fill with sync level, others with blanking level
        uint16_t fill_level;
        if (first_pulse == SyncPulseType::BROAD && second_pulse == SyncPulseType::BROAD) {
            fill_level = static_cast<uint16_t>(sync_level_);
        } else {
            fill_level = static_cast<uint16_t>(blanking_level_);
        }
        std::fill_n(line_buffer, params_.field_width, fill_level);
        
        // Step 2: Add sync pulses
        generate_vsync_line(line_buffer, line, is_first_field, system);
        
        // Step 3: Add color burst on top
        add_color_burst(line_buffer, line, field_number, is_first_field, system);
    }
    
    // Create line map
    result.line_types = create_line_map(is_first_field, system);
    
    // Determine VBI and active video ranges
    result.vbi_range = determine_vbi_range(is_first_field, system);
    result.active_video_range = determine_active_video_range(is_first_field, system);
    
    return result;
}

void FieldStructureGenerator::generate_hsync_pulse(uint16_t* line_buffer, 
                                                   int32_t /* line_number */, 
                                                   VideoSystem /* system */) {
    // Horizontal sync pulse
    // Duration: 4.7 µs for both PAL and NTSC
    // The line is already filled with blanking level
    
    double sample_duration = 1.0 / sample_rate_;  // seconds per sample
    int32_t sync_duration = static_cast<int32_t>(4.7e-6 / sample_duration);  // 4.7 µs
    
    // Sync pulse at beginning of line
    for (int32_t i = 0; i < sync_duration && i < params_.field_width; ++i) {
        line_buffer[i] = static_cast<uint16_t>(sync_level_);
    }
}

void FieldStructureGenerator::generate_vsync_line(uint16_t* line_buffer, 
                                                  int32_t line_number,
                                                  bool is_first_field,
                                                  VideoSystem system) {
    // Get the sync pattern for this specific line according to standards
    auto [first_pulse, second_pulse] = get_sync_pattern_for_line(line_number, is_first_field, system);
    
    // Generate the line with the appropriate pulse pattern
    generate_sync_line(line_buffer, first_pulse, second_pulse, system);
}

std::pair<FieldStructureGenerator::SyncPulseType, FieldStructureGenerator::SyncPulseType> 
FieldStructureGenerator::get_sync_pattern_for_line(
    int32_t field_line, bool is_first_field, VideoSystem system) {
    
    using SPT = SyncPulseType;
    
    // Convert field-relative line (0-indexed) to absolute frame line (1-indexed)
    int32_t absolute_frame_line;
    
    if (system == VideoSystem::NTSC) {
        // NTSC: 525 lines total, fields both have 263 lines (with line 263 shared)
        // Field 1 (first_field=true): frame lines 1-263 (0-indexed field: 0-262)
        // Field 2 (first_field=false): frame lines 263-525 (0-indexed field: 0-262)
        // Line 263 is the transition half-line between fields
        if (is_first_field) {
            absolute_frame_line = field_line + 1;  // Lines 1-263
        } else {
            absolute_frame_line = field_line + 263;  // Lines 263-525
        }
        
        // Look up pattern in NTSC table
        for (const auto& pattern : NTSC_SYNC_PATTERN) {
            if (pattern.line == absolute_frame_line) {
                return {pattern.first, pattern.second};
            }
        }
        
        // Default: normal horizontal sync (single pulse)
        return {SPT::NORMAL, SPT::NONE};
        
    } else {
        // PAL: 625 lines total, fields are 312/313 lines
        // Field 1 (first_field=true): frame lines 1-312 (0-indexed field: 0-311)
        // Field 2 (first_field=false): frame lines 313-625 (0-indexed field: 0-312)
        if (is_first_field) {
            absolute_frame_line = field_line + 1;  // Lines 1-312
        } else {
            absolute_frame_line = field_line + 313;  // Lines 313-625
        }
        
        // Look up pattern in PAL table
        for (const auto& pattern : PAL_SYNC_PATTERN) {
            if (pattern.line == absolute_frame_line) {
                return {pattern.first, pattern.second};
            }
        }
        
        // Default: normal horizontal sync (single pulse)
        return {SPT::NORMAL, SPT::NONE};
    }
}

void FieldStructureGenerator::generate_sync_line(uint16_t* line_buffer, 
                                                 SyncPulseType first_pulse, 
                                                 SyncPulseType second_pulse,
                                                 VideoSystem system) {
    // Note: Line is already filled with appropriate base level
    // BR_BR: filled with sync_level
    // Others: filled with blanking_level
    
    // Pulse timing in microseconds
    double normal_sync = 4.7;      // 4.7 µs for both PAL and NTSC
    double eq_sync = (system == VideoSystem::PAL) ? 2.35 : 2.3;  // 2.35 µs PAL, 2.3 µs NTSC
    double broad_sync = (system == VideoSystem::PAL) ? 27.3 : 27.1; // 27.3 µs PAL, 27.1 µs NTSC
    
    double sample_duration = 1.0 / sample_rate_;
    int32_t half_line = params_.field_width / 2;
    
    // Helper to generate a sync pulse at a specific position (ONLY the pulse, not filling after)
    auto generate_pulse = [&](int32_t position, SyncPulseType type) {
        if (type == SyncPulseType::NONE) return;
        
        int32_t duration;
        switch (type) {
            case SyncPulseType::EQUALIZING:
                duration = static_cast<int32_t>(eq_sync * 1e-6 / sample_duration);
                break;
            case SyncPulseType::BROAD:
                duration = static_cast<int32_t>(broad_sync * 1e-6 / sample_duration);
                break;
            case SyncPulseType::NORMAL:
                duration = static_cast<int32_t>(normal_sync * 1e-6 / sample_duration);
                break;
            case SyncPulseType::NONE:
                return;
        }
        
        // Write ONLY the pulse itself at sync level
        for (int32_t i = 0; i < duration && (position + i) < params_.field_width; ++i) {
            line_buffer[position + i] = static_cast<uint16_t>(sync_level_);
        }
    };
    
    // For BR_BR lines: Line is at sync level, add short blanking pulses ONLY after each BR
    if (first_pulse == SyncPulseType::BROAD && second_pulse == SyncPulseType::BROAD) {
        int32_t br_duration = static_cast<int32_t>(broad_sync * 1e-6 / sample_duration);
        int32_t blanking_pulse_duration = static_cast<int32_t>(2.3e-6 / sample_duration); // Short blanking pulse
        
        // First blanking pulse (after first BR, before color burst area)
        for (int32_t i = br_duration; i < br_duration + blanking_pulse_duration && i < half_line; ++i) {
            line_buffer[i] = static_cast<uint16_t>(blanking_level_);
        }
        
        // Second blanking pulse (after second BR at mid-line)
        for (int32_t i = half_line + br_duration; 
             i < half_line + br_duration + blanking_pulse_duration && i < params_.field_width; ++i) {
            line_buffer[i] = static_cast<uint16_t>(blanking_level_);
        }
        return;
    }
    
    // For all other lines (including EQ_EQ), just add the sync pulses
    // The line is already at blanking level, we just add sync pulses
    generate_pulse(0, first_pulse);
    
    if (second_pulse != SyncPulseType::NONE) {
        generate_pulse(half_line, second_pulse);
    }
}

void FieldStructureGenerator::generate_blanking_line(uint16_t* line_buffer) {
    // Fill line with blanking level
    std::fill_n(line_buffer, params_.field_width, static_cast<uint16_t>(blanking_level_));
}

void FieldStructureGenerator::add_color_burst(uint16_t* line_buffer, int32_t line_number, 
                                               int32_t field_number, bool is_first_field, 
                                               VideoSystem system) {
    // Get the sync pattern for this line to determine center level
    auto [first_pulse, second_pulse] = get_sync_pattern_for_line(line_number, is_first_field, system);
    
    // Determine center level for color burst
    // Lines starting with BROAD: center level is sync level (burst sits on sync level)
    // All other lines: center level is blanking level
    int32_t center_level = blanking_level_;
    if (first_pulse == SyncPulseType::BROAD) {
        center_level = sync_level_;
    }
    
    // Use ColorBurstGenerator for proper envelope shaping
    ColorBurstGenerator burst_gen(params_);
    int32_t luma_range = white_level_ - blanking_level_;
    // Calculate burst amplitude based on video system
    // PAL: 300mV peak-to-peak = 150mV amplitude = 3/14 of luma range
    // NTSC: 20% of luma range per standard
    int32_t burst_amplitude = (system == VideoSystem::PAL) ? 
        static_cast<int32_t>((3.0 / 14.0) * luma_range) :
        static_cast<int32_t>((20.0 / 100.0) * luma_range);
    
    if (system == VideoSystem::NTSC) {
        burst_gen.generate_ntsc_burst(line_buffer, line_number, field_number, center_level, burst_amplitude);
    } else {
        burst_gen.generate_pal_burst(line_buffer, line_number, field_number, center_level, burst_amplitude);
    }
}

LineMap FieldStructureGenerator::create_line_map(bool /* is_first_field */, VideoSystem system) {
    LineMap line_map;
    
    int32_t vsync_lines = (system == VideoSystem::PAL) ? PAL_VSYNC_LINES : NTSC_VSYNC_LINES;
    int32_t vbi_start = (system == VideoSystem::PAL) ? PAL_VBI_START : NTSC_VBI_START;
    int32_t vbi_end = (system == VideoSystem::PAL) ? PAL_VBI_END : NTSC_VBI_END;
    int32_t active_start = (system == VideoSystem::PAL) ? PAL_ACTIVE_START : NTSC_ACTIVE_START;
    
    // Mark vsync lines
    for (int32_t line = 0; line < vsync_lines; ++line) {
        line_map[line] = LineType::VSYNC;
    }
    
    // Mark VBI lines
    for (int32_t line = vbi_start; line <= vbi_end; ++line) {
        line_map[line] = LineType::VBI;
    }
    
    // Mark active video lines
    for (int32_t line = active_start; line < params_.field_height; ++line) {
        line_map[line] = LineType::ACTIVE_VIDEO;
    }
    
    // Lines between vsync and VBI are blanking
    for (int32_t line = vsync_lines; line < vbi_start; ++line) {
        line_map[line] = LineType::BLANKING;
    }
    
    return line_map;
}

LineRange FieldStructureGenerator::determine_vbi_range(bool /* is_first_field */, VideoSystem system) {
    int32_t vbi_start = (system == VideoSystem::PAL) ? PAL_VBI_START : NTSC_VBI_START;
    int32_t vbi_end = (system == VideoSystem::PAL) ? PAL_VBI_END : NTSC_VBI_END;
    
    return LineRange(vbi_start, vbi_end);
}

LineRange FieldStructureGenerator::determine_active_video_range(bool /* is_first_field */, VideoSystem system) {
    int32_t active_start = (system == VideoSystem::PAL) ? PAL_ACTIVE_START : NTSC_ACTIVE_START;
    
    // Active video continues to the end of the field
    return LineRange(active_start, params_.field_height - 1);
}

} // namespace encode_orc
