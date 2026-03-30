/*
 * File:        field_splitter.cpp
 * Module:      encode-orc
 * Purpose:     Split progressive frames into interlaced fields implementation
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: 2026 Simon Inns
 */

#include "field_splitter.h"
#include <stdexcept>
#include <cstring>

namespace encode_orc {

FieldSplitter::FieldPair FieldSplitter::split_frame(const FrameBuffer& frame,
                                                     int32_t field_number,
                                                     const VideoParameters& params) {
    // Verify input format
    if (frame.format() != FrameBuffer::Format::YUV444P16) {
        throw std::runtime_error("FieldSplitter: Frame must be in YUV444P16 format");
    }
    
    // Get source frame dimensions and data
    int32_t frame_width = frame.width();
    int32_t frame_height = frame.height();
    const uint16_t* frame_data = frame.data().data();
    int32_t pixel_count = frame_width * frame_height;
    
    // Get YUV plane pointers from source (planar layout: Y, U, V)
    const uint16_t* y_plane_src = frame_data;
    const uint16_t* u_plane_src = frame_data + pixel_count;
    const uint16_t* v_plane_src = frame_data + (pixel_count * 2);
    
    // Create fields with enough space for YUV planar data (3x the normal size)
    // Width represents the actual image width, but we need 3 planes worth of data
    // We'll use "height * 3" to allocate space for Y, U, V planes
    FieldPair result;
    result.field1 = Field(frame_width, params.field1_height * 3);  // 3x for Y, U, V planes
    result.field2 = Field(frame_width, params.field2_height * 3);  // 3x for Y, U, V planes
    result.field_number = field_number;
    
    // Allocate temporary buffers for field YUV data
    std::vector<uint16_t> field1_y(frame_width * params.field1_height);
    std::vector<uint16_t> field1_u(frame_width * params.field1_height);
    std::vector<uint16_t> field1_v(frame_width * params.field1_height);
    
    std::vector<uint16_t> field2_y(frame_width * params.field2_height);
    std::vector<uint16_t> field2_u(frame_width * params.field2_height);
    std::vector<uint16_t> field2_v(frame_width * params.field2_height);
    
    // Split even lines (0, 2, 4, ...) into field1
    int32_t field1_lines = 0;
    for (int32_t src_line = 0; src_line < frame_height && field1_lines < params.field1_height; src_line += 2) {
        const uint16_t* y_src = y_plane_src + (src_line * frame_width);
        const uint16_t* u_src = u_plane_src + (src_line * frame_width);
        const uint16_t* v_src = v_plane_src + (src_line * frame_width);
        
        uint16_t* y_dst = field1_y.data() + (field1_lines * frame_width);
        uint16_t* u_dst = field1_u.data() + (field1_lines * frame_width);
        uint16_t* v_dst = field1_v.data() + (field1_lines * frame_width);
        
        std::memcpy(y_dst, y_src, frame_width * sizeof(uint16_t));
        std::memcpy(u_dst, u_src, frame_width * sizeof(uint16_t));
        std::memcpy(v_dst, v_src, frame_width * sizeof(uint16_t));
        
        field1_lines++;
    }
    
    // Split odd lines (1, 3, 5, ...) into field2
    int32_t field2_lines = 0;
    for (int32_t src_line = 1; src_line < frame_height && field2_lines < params.field2_height; src_line += 2) {
        const uint16_t* y_src = y_plane_src + (src_line * frame_width);
        const uint16_t* u_src = u_plane_src + (src_line * frame_width);
        const uint16_t* v_src = v_plane_src + (src_line * frame_width);
        
        uint16_t* y_dst = field2_y.data() + (field2_lines * frame_width);
        uint16_t* u_dst = field2_u.data() + (field2_lines * frame_width);
        uint16_t* v_dst = field2_v.data() + (field2_lines * frame_width);
        
        std::memcpy(y_dst, y_src, frame_width * sizeof(uint16_t));
        std::memcpy(u_dst, u_src, frame_width * sizeof(uint16_t));
        std::memcpy(v_dst, v_src, frame_width * sizeof(uint16_t));
        
        field2_lines++;
    }
    
    // Copy YUV data into field structures (planar format)
    // Field data layout: Y plane, then U plane, then V plane
    uint16_t* field1_data = result.field1.data().data();
    uint16_t* field2_data = result.field2.data().data();
    
    int32_t field1_pixel_count = frame_width * params.field1_height;
    int32_t field2_pixel_count = frame_width * params.field2_height;
    
    // Copy field1 YUV planes
    std::memcpy(field1_data, field1_y.data(), field1_pixel_count * sizeof(uint16_t));
    std::memcpy(field1_data + field1_pixel_count, field1_u.data(), field1_pixel_count * sizeof(uint16_t));
    std::memcpy(field1_data + (field1_pixel_count * 2), field1_v.data(), field1_pixel_count * sizeof(uint16_t));
    
    // Copy field2 YUV planes
    std::memcpy(field2_data, field2_y.data(), field2_pixel_count * sizeof(uint16_t));
    std::memcpy(field2_data + field2_pixel_count, field2_u.data(), field2_pixel_count * sizeof(uint16_t));
    std::memcpy(field2_data + (field2_pixel_count * 2), field2_v.data(), field2_pixel_count * sizeof(uint16_t));

    // Split audio (if present) into field audio samples
    if (frame.has_audio()) {
        const auto& frame_audio = frame.audio();
        int32_t samples_per_field = get_audio_samples_per_field(params.system);
        int32_t samples_per_frame = samples_per_field * 2;  // Two fields per frame
        int32_t required_audio_values = samples_per_frame * 2;  // Stereo interleaved

        if (static_cast<int32_t>(frame_audio.size()) >= required_audio_values) {
            std::vector<int16_t> field1_audio(frame_audio.begin(), frame_audio.begin() + samples_per_field * 2);
            std::vector<int16_t> field2_audio(frame_audio.begin() + samples_per_field * 2,
                                              frame_audio.begin() + samples_per_field * 4);
            result.field1.set_audio(std::move(field1_audio));
            result.field2.set_audio(std::move(field2_audio));
        }
    }
    
    return result;
}

FrameBuffer FieldSplitter::merge_fields(const FieldPair& fields,
                                       const VideoParameters& params) {
    // Create output frame buffer
    int32_t frame_width = fields.field1.width();
    int32_t frame_height = params.field1_height + params.field2_height;
    FrameBuffer frame(frame_width, frame_height, FrameBuffer::Format::YUV444P16);
    
    // Get field data pointers
    // Note: fields have height * 3 to accommodate Y, U, V planes
    const uint16_t* field1_data = fields.field1.data().data();
    const uint16_t* field2_data = fields.field2.data().data();
    
    int32_t field1_pixel_count = frame_width * params.field1_height;
    int32_t field2_pixel_count = frame_width * params.field2_height;
    
    // Extract YUV planes from fields
    const uint16_t* field1_y = field1_data;
    const uint16_t* field1_u = field1_data + field1_pixel_count;
    const uint16_t* field1_v = field1_data + (field1_pixel_count * 2);
    
    const uint16_t* field2_y = field2_data;
    const uint16_t* field2_u = field2_data + field2_pixel_count;
    const uint16_t* field2_v = field2_data + (field2_pixel_count * 2);
    
    // Get frame YUV plane pointers
    uint16_t* frame_data = frame.data().data();
    int32_t frame_pixel_count = frame_width * frame_height;
    uint16_t* y_plane_dst = frame_data;
    uint16_t* u_plane_dst = frame_data + frame_pixel_count;
    uint16_t* v_plane_dst = frame_data + (frame_pixel_count * 2);
    
    // Interleave fields back into frame
    // Field1 lines go to even lines (0, 2, 4, ...)
    for (int32_t field_line = 0; field_line < params.field1_height; ++field_line) {
        int32_t frame_line = field_line * 2;
        if (frame_line < frame_height) {
            const uint16_t* y_src = field1_y + (field_line * frame_width);
            const uint16_t* u_src = field1_u + (field_line * frame_width);
            const uint16_t* v_src = field1_v + (field_line * frame_width);
            
            uint16_t* y_dst = y_plane_dst + (frame_line * frame_width);
            uint16_t* u_dst = u_plane_dst + (frame_line * frame_width);
            uint16_t* v_dst = v_plane_dst + (frame_line * frame_width);
            
            std::memcpy(y_dst, y_src, frame_width * sizeof(uint16_t));
            std::memcpy(u_dst, u_src, frame_width * sizeof(uint16_t));
            std::memcpy(v_dst, v_src, frame_width * sizeof(uint16_t));
        }
    }
    
    // Field2 lines go to odd lines (1, 3, 5, ...)
    for (int32_t field_line = 0; field_line < params.field2_height; ++field_line) {
        int32_t frame_line = field_line * 2 + 1;
        if (frame_line < frame_height) {
            const uint16_t* y_src = field2_y + (field_line * frame_width);
            const uint16_t* u_src = field2_u + (field_line * frame_width);
            const uint16_t* v_src = field2_v + (field_line * frame_width);
            
            uint16_t* y_dst = y_plane_dst + (frame_line * frame_width);
            uint16_t* u_dst = u_plane_dst + (frame_line * frame_width);
            uint16_t* v_dst = v_plane_dst + (frame_line * frame_width);
            
            std::memcpy(y_dst, y_src, frame_width * sizeof(uint16_t));
            std::memcpy(u_dst, u_src, frame_width * sizeof(uint16_t));
            std::memcpy(v_dst, v_src, frame_width * sizeof(uint16_t));
        }
    }
    
    return frame;
}

} // namespace encode_orc
