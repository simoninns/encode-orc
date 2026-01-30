/*
 * File:        field_structure_generator.h
 * Module:      encode-orc
 * Purpose:     Field structure generation (sync, blanking, VBI layout)
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: 2026 Simon Inns
 */

#ifndef ENCODE_ORC_FIELD_STRUCTURE_GENERATOR_H
#define ENCODE_ORC_FIELD_STRUCTURE_GENERATOR_H

#include "field.h"
#include "video_parameters.h"
#include <cstdint>
#include <vector>
#include <map>
#include <optional>

namespace encode_orc {

/**
 * @brief Type of line in a video field
 */
enum class LineType {
    VSYNC,          ///< Vertical sync line
    VBI,            ///< Vertical blanking interval (for metadata)
    BLANKING,       ///< Blanking line (no active video)
    ACTIVE_VIDEO    ///< Active video line
};

/**
 * @brief Map of line numbers to their types
 */
using LineMap = std::map<int32_t, LineType>;

/**
 * @brief Range of line numbers
 */
struct LineRange {
    int32_t start;  ///< First line (inclusive)
    int32_t end;    ///< Last line (inclusive)
    
    LineRange() : start(0), end(0) {}
    LineRange(int32_t s, int32_t e) : start(s), end(e) {}
    
    bool contains(int32_t line) const {
        return line >= start && line <= end;
    }
    
    int32_t count() const {
        return end - start + 1;
    }
};

/**
 * @brief A field with structural information (sync, blanking, VBI regions)
 * 
 * This structure represents a video field that has had its basic structure
 * generated (sync pulses, blanking, vsync) but may not yet have metadata
 * or active video encoded.
 */
struct StructuredField {
    Field field_data;            ///< Raw sample data for the field
    LineMap line_types;          ///< Type of each line (VBI, active, blanking, vsync)
    LineRange active_video_range; ///< Which lines contain active video
    LineRange vbi_range;         ///< Which lines are available for VBI metadata
    
    /**
     * @brief Get the type of a specific line
     * @param line Line number (0-indexed)
     * @return Line type, or BLANKING if not found
     */
    LineType get_line_type(int32_t line) const {
        auto it = line_types.find(line);
        return (it != line_types.end()) ? it->second : LineType::BLANKING;
    }
    
    /**
     * @brief Check if a line is in the VBI range
     * @param line Line number (0-indexed)
     */
    bool is_vbi_line(int32_t line) const {
        return vbi_range.contains(line);
    }
    
    /**
     * @brief Check if a line is in the active video range
     * @param line Line number (0-indexed)
     */
    bool is_active_video_line(int32_t line) const {
        return active_video_range.contains(line);
    }
};

/**
 * @brief Generates the basic structure of video fields
 * 
 * This class is responsible for creating the skeleton of a video field including:
 * - Vertical sync (vsync) patterns
 * - Horizontal sync pulses
 * - Blanking levels
 * - Marking VBI regions for metadata insertion
 * - Marking active video regions
 * 
 * The structure is created before metadata (VITS, VITC, VBI data) is added
 * and before active video is encoded.
 */
class FieldStructureGenerator {
public:
    /**
     * @brief Construct a field structure generator
     * @param params Video parameters (determines timing, levels, etc.)
     */
    explicit FieldStructureGenerator(const VideoParameters& params);
    
    /**
     * @brief Create complete field structure with sync, blanking, and VBI regions marked
     * 
     * This generates:
     * - Vsync pattern for the first few lines
     * - Horizontal sync pulses on all lines
     * - Blanking levels on appropriate lines
     * - LineMap indicating which lines are VBI, active video, etc.
     * 
     * @param source_field Optional source field with YUV data (can be empty for structure-only)
     * @param is_first_field true for first field (even lines), false for second (odd lines)
     * @param field_number Field number (for phase calculations)
     * @param system Video system (PAL or NTSC)
     * @return Structured field with sync/blanking and regions marked
     */
    StructuredField create_field_structure(
        const Field& source_field,
        bool is_first_field,
        int32_t field_number,
        VideoSystem system);
    
    /**
     * @brief Create field structure with sync/blanking only (no color burst)
     * 
     * For Y/C output, this creates the Y field with proper blanking and sync.
     * Color burst should be added to C field separately using add_color_burst_to_field.
     * 
     * @param source_field Optional source field with YUV data (can be empty for structure-only)
     * @param is_first_field true for first field (even lines), false for second (odd lines)
     * @param field_number Field number (for phase calculations)
     * @param system Video system (PAL or NTSC)
     * @return Structured field with sync/blanking only
     */
    StructuredField create_field_structure_without_burst(
        const Field& source_field,
        bool is_first_field,
        int32_t field_number,
        VideoSystem system);
    
    /**
     * @brief Add color burst to an existing field
     * 
     * For Y/C output, this adds color burst to the C field.
     * The field should already have appropriate baseline levels set.
     * 
     * @param field Field to add color burst to
     * @param field_number Field number (for phase calculations)
     * @param is_first_field true for first field, false for second
     * @param system Video system (PAL or NTSC)
     * @param force_center_level Optional center level override (e.g., 32768 for Y/C C field)
     */
    void add_color_burst_to_field(
        Field& field,
        int32_t field_number,
        bool is_first_field,
        VideoSystem system,
        std::optional<int32_t> force_center_level = std::nullopt);
    
private:
    /**
     * @brief Sync pulse types according to video standards
     */
    enum class SyncPulseType {
        NORMAL,      // N: Standard horizontal sync (4.7 µs)
        EQUALIZING,  // EQ: Half-width sync (2.3-2.35 µs), repeated every ½ line
        BROAD,       // BR: Long sync pulse (27.1-27.3 µs)
        NONE         // No second pulse (single-pulse line)
    };
    
    /**
     * @brief PAL Sync Pattern (625-line system, 1-indexed frame lines)
     * 
     * Frame lines 1-312 belong to Field 1
     * Frame lines 313-625 belong to Field 2
     * 
     * Pattern across entire frame (from issue #16):
     * Lines   1-2:   BR_BR  (Broad pulses)
     * Line    3:     BR_EQ  (Broad then Equalizing)
     * Lines   4-5:   EQ_EQ  (Equalizing pulses)
     * Lines   6-310: N      (Normal horizontal sync)
     * Lines 311-312: EQ_EQ  (Equalizing pulses)
     * Line  313:     EQ_BR  (Equalizing then Broad)
     * Lines 314-315: BR_BR  (Broad pulses)
     * Lines 316-317: BR_EQ  (Broad then Equalizing) - implied pattern completion
     * Line  318:     EQ     (Equalizing pulses)
     * Lines 319-622: N      (Normal horizontal sync)
     * Line  623:     N_EQ   (Normal then Equalizing)
     * Lines 624-625: EQ_EQ  (Equalizing pulses)
     */
    static constexpr struct {
        int32_t line;  // 1-indexed frame line number
        SyncPulseType first;
        SyncPulseType second;
    } PAL_SYNC_PATTERN[] = {
        {1, SyncPulseType::BROAD, SyncPulseType::BROAD},
        {2, SyncPulseType::BROAD, SyncPulseType::BROAD},
        {3, SyncPulseType::BROAD, SyncPulseType::EQUALIZING},
        {4, SyncPulseType::EQUALIZING, SyncPulseType::EQUALIZING},
        {5, SyncPulseType::EQUALIZING, SyncPulseType::EQUALIZING},
        {311, SyncPulseType::EQUALIZING, SyncPulseType::EQUALIZING},
        {312, SyncPulseType::EQUALIZING, SyncPulseType::EQUALIZING},
        {313, SyncPulseType::EQUALIZING, SyncPulseType::BROAD},
        {314, SyncPulseType::BROAD, SyncPulseType::BROAD},
        {315, SyncPulseType::BROAD, SyncPulseType::BROAD},
        {316, SyncPulseType::EQUALIZING, SyncPulseType::EQUALIZING},
        {317, SyncPulseType::EQUALIZING, SyncPulseType::EQUALIZING},
        {318, SyncPulseType::EQUALIZING, SyncPulseType::NONE},
        {623, SyncPulseType::NORMAL, SyncPulseType::EQUALIZING},
        {624, SyncPulseType::EQUALIZING, SyncPulseType::EQUALIZING},
        {625, SyncPulseType::EQUALIZING, SyncPulseType::EQUALIZING}
    };
    
    /**
     * @brief NTSC Sync Pattern (525-line system, 1-indexed frame lines)
     * 
     * Frame lines 1-262 belong to Field 1 (even field)
     * Frame lines 263-525 belong to Field 2 (odd field)
     * 
     * Pattern across entire frame (from issue #16):
     * Lines   1-3:   EQ_EQ  (2 EQ pulses per line = 6 total EQ pulses)
     * Lines   4-6:   BR_BR  (2 BR pulses per line = 6 total BR pulses)
     * Lines   7-9:   EQ_EQ  (2 EQ pulses per line = 6 total EQ pulses)
     * Lines  10-262: N      (1 Normal sync pulse per line)
     * Line  263:     N_EQ   (1 Normal + 1 EQ pulse)
     * Lines 264-265: EQ_EQ  (2 EQ + 2 EQ = 4 EQ pulses, total 5 EQ)
     * Line  266:     EQ_BR  (1 EQ + 1 BR, completes 6 EQ, starts BR sequence)
     * Lines 267-268: BR_BR  (2 BR + 2 BR = 4 BR pulses, total 5 BR)
     * Line  269:     BR_EQ  (1 BR + 1 EQ, completes 6 BR, starts post-EQ)
     * Lines 270-271: EQ_EQ  (2 EQ + 2 EQ = 4 EQ pulses, total 5 EQ)
     * Line  272:     EQ     (1 EQ pulse, completes 6 EQ total)
     * Lines 273-525: N      (1 Normal sync pulse per line)
     */
    static constexpr struct {
        int32_t line;  // 1-indexed frame line number
        SyncPulseType first;
        SyncPulseType second;
    } NTSC_SYNC_PATTERN[] = {
        {1, SyncPulseType::EQUALIZING, SyncPulseType::EQUALIZING},
        {2, SyncPulseType::EQUALIZING, SyncPulseType::EQUALIZING},
        {3, SyncPulseType::EQUALIZING, SyncPulseType::EQUALIZING},
        {4, SyncPulseType::BROAD, SyncPulseType::BROAD},
        {5, SyncPulseType::BROAD, SyncPulseType::BROAD},
        {6, SyncPulseType::BROAD, SyncPulseType::BROAD},
        {7, SyncPulseType::EQUALIZING, SyncPulseType::EQUALIZING},
        {8, SyncPulseType::EQUALIZING, SyncPulseType::EQUALIZING},
        {9, SyncPulseType::EQUALIZING, SyncPulseType::EQUALIZING},
        {263, SyncPulseType::NORMAL, SyncPulseType::EQUALIZING},
        {264, SyncPulseType::EQUALIZING, SyncPulseType::EQUALIZING},
        {265, SyncPulseType::EQUALIZING, SyncPulseType::EQUALIZING},
        {266, SyncPulseType::EQUALIZING, SyncPulseType::BROAD},
        {267, SyncPulseType::BROAD, SyncPulseType::BROAD},
        {268, SyncPulseType::BROAD, SyncPulseType::BROAD},
        {269, SyncPulseType::BROAD, SyncPulseType::EQUALIZING},
        {270, SyncPulseType::EQUALIZING, SyncPulseType::EQUALIZING},
        {271, SyncPulseType::EQUALIZING, SyncPulseType::EQUALIZING},
        {272, SyncPulseType::EQUALIZING, SyncPulseType::NONE}
    };
    
    /**
     * @brief Generate horizontal sync pulse on a line
     * @param line_buffer Pointer to line data
     * @param line_number Line number (0-indexed)
     * @param system Video system (PAL or NTSC)
     */
    void generate_hsync_pulse(uint16_t* line_buffer, int32_t line_number, VideoSystem system);
    
    /**
     * @brief Generate a specific vsync line (with broad/narrow pulses)
     * @param line_buffer Pointer to line data
     * @param line_number Line number within field (0-indexed)
     * @param is_first_field true for first field, false for second
     * @param system Video system (PAL or NTSC)
     */
    void generate_vsync_line(uint16_t* line_buffer, int32_t line_number, bool is_first_field, VideoSystem system);
    
    /**
     * @brief Generate a line with specific sync pulse pattern
     * @param line_buffer Pointer to line data
     * @param first_pulse Type of pulse at line start
     * @param second_pulse Type of pulse at line middle (optional)
     * @param system Video system (PAL or NTSC)
     */
    void generate_sync_line(uint16_t* line_buffer, SyncPulseType first_pulse, 
                           SyncPulseType second_pulse, VideoSystem system);
    
    /**
     * @brief Add color burst to a line (on top of existing sync pattern)
     * @param line_buffer Pointer to line data
     * @param line_number Line number within field (0-indexed)
     * @param field_number Field number for phase calculation
     * @param is_first_field true for first field, false for second
     * @param system Video system (PAL or NTSC)
     */
    void add_color_burst(uint16_t* line_buffer, int32_t line_number, int32_t field_number,
                        bool is_first_field, VideoSystem system);
    
    /**
     * @brief Add color burst with explicit center level
     * @param line_buffer Pointer to line data
     * @param line_number Line number within field
     * @param field_number Field number for phase calculation
     * @param is_first_field true for first field, false for second
     * @param system Video system (PAL or NTSC)
     * @param center_level Explicit center level for burst (e.g., 32768 for Y/C)
     */
    void add_color_burst_with_center(uint16_t* line_buffer, int32_t line_number, int32_t field_number,
                                     bool is_first_field, VideoSystem system, int32_t center_level);
    
    /**
     * @brief Fill a line with blanking level
     * @param line_buffer Pointer to line data
     */
    void generate_blanking_line(uint16_t* line_buffer);
    
    /**
     * @brief Get sync pulse pattern for a specific field line
     * @param field_line Line number within field (0-indexed)
     * @param is_first_field true for first field, false for second
     * @param system Video system (PAL or NTSC)
     * @return Pair of pulse types (first half, second half of line)
     */
    std::pair<SyncPulseType, SyncPulseType> get_sync_pattern_for_line(
        int32_t field_line, bool is_first_field, VideoSystem system);
    
    /**
     * @brief Create the LineMap for a field
     * @param is_first_field true for first field, false for second
     * @param system Video system (PAL or NTSC)
     * @return Map of line numbers to line types
     */
    LineMap create_line_map(bool is_first_field, VideoSystem system);
    
    /**
     * @brief Determine VBI range for a field
     * @param is_first_field true for first field, false for second
     * @param system Video system (PAL or NTSC)
     * @return Range of lines available for VBI
     */
    LineRange determine_vbi_range(bool is_first_field, VideoSystem system);
    
    /**
     * @brief Determine active video range for a field
     * @param is_first_field true for first field, false for second
     * @param system Video system (PAL or NTSC)
     * @return Range of lines containing active video
     */
    LineRange determine_active_video_range(bool is_first_field, VideoSystem system);
    
    const VideoParameters& params_;
    
    // Video signal levels (from VideoParameters)
    int32_t sync_level_;
    int32_t blanking_level_;
    int32_t white_level_;
    double sample_rate_;
    
    // PAL-specific constants
    static constexpr int32_t PAL_VSYNC_LINES = 5;
    static constexpr int32_t PAL_VBI_START = 6;
    static constexpr int32_t PAL_VBI_END = 22;
    static constexpr int32_t PAL_ACTIVE_START = 23;
    
    // NTSC-specific constants - Lines 0-8 have special sync (EQ_EQ, BR_BR, EQ_EQ)
    static constexpr int32_t NTSC_VSYNC_LINES = 9;
    static constexpr int32_t NTSC_VBI_START = 10;
    static constexpr int32_t NTSC_VBI_END = 20;
    static constexpr int32_t NTSC_ACTIVE_START = 21;
};

} // namespace encode_orc

#endif // ENCODE_ORC_FIELD_STRUCTURE_GENERATOR_H
