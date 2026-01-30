/*
 * File:        field_splitter.h
 * Module:      encode-orc
 * Purpose:     Split progressive frames into interlaced fields
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: 2026 Simon Inns
 */

#ifndef ENCODE_ORC_FIELD_SPLITTER_H
#define ENCODE_ORC_FIELD_SPLITTER_H

#include "field.h"
#include "frame_buffer.h"
#include "video_parameters.h"
#include <cstdint>

namespace encode_orc {

/**
 * @brief Splits progressive frames into interlaced fields
 * 
 * This class handles the conversion of progressive frames (stored as FrameBuffer)
 * into separate fields for interlaced video encoding. It extracts even/odd lines
 * from the source frame and creates YUV444P16 field data ready for encoding.
 */
class FieldSplitter {
public:
    /**
     * @brief Container for a pair of interlaced fields from one frame
     */
    struct FieldPair {
        Field field1;           ///< First field (even lines: 0, 2, 4, ...)
        Field field2;           ///< Second field (odd lines: 1, 3, 5, ...)
        int32_t field_number;   ///< Starting field number for the pair
        
        /**
         * @brief Construct an empty field pair
         */
        FieldPair() : field_number(0) {}
        
        /**
         * @brief Construct a field pair with pre-allocated fields
         * @param width Field width in samples
         * @param height1 First field height in lines
         * @param height2 Second field height in lines
         * @param field_num Starting field number
         */
        FieldPair(int32_t width, int32_t height1, int32_t height2, int32_t field_num)
            : field1(width, height1),
              field2(width, height2),
              field_number(field_num) {}
    };
    
    /**
     * @brief Construct a field splitter
     */
    FieldSplitter() = default;
    
    /**
     * @brief Split progressive frame into interlaced fields (YUV data only)
     * 
     * Extracts even lines (0, 2, 4, ...) into field1 and odd lines (1, 3, 5, ...)
     * into field2. The input frame must be in YUV444P16 format (planar layout).
     * 
     * The output fields contain YUV data in planar format (Y, U, V planes),
     * matching the field dimensions from VideoParameters.
     * 
     * @param frame Input progressive frame in YUV444P16 format
     * @param field_number Starting field number for this frame
     * @param params Video parameters (contains field dimensions)
     * @return FieldPair containing the split YUV data
     * @throws std::runtime_error if frame format is not YUV444P16
     */
    FieldPair split_frame(const FrameBuffer& frame, 
                          int32_t field_number,
                          const VideoParameters& params);
    
    /**
     * @brief Merge interlaced fields back into a progressive frame
     * 
     * This is primarily useful for testing and validation. Takes two fields
     * and interleaves them to reconstruct a progressive frame.
     * 
     * @param fields The field pair to merge
     * @param params Video parameters (contains frame dimensions)
     * @return FrameBuffer containing the merged progressive frame
     */
    FrameBuffer merge_fields(const FieldPair& fields,
                            const VideoParameters& params);
};

} // namespace encode_orc

#endif // ENCODE_ORC_FIELD_SPLITTER_H
