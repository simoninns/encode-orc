/*
 * File:        video_encoder_pipeline.cpp
 * Module:      encode-orc
 * Purpose:     Video encoder pipeline implementation (Phase 5)
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: 2026 Simon Inns
 */

#include "video_encoder_pipeline.h"
#include "pal_active_encoder.h"
#include "ntsc_active_encoder.h"
#include "metadata.h"
#include "logging.h"
#include "field_structure_generator.h"
#include <algorithm>

namespace encode_orc {

// Builder implementation
VideoEncoderPipeline::Builder& VideoEncoderPipeline::Builder::set_system(VideoSystem system) {
    system_ = system;
    return *this;
}

VideoEncoderPipeline::Builder& VideoEncoderPipeline::Builder::set_parameters(const VideoParameters& params) {
    params_ = params;
    return *this;
}

VideoEncoderPipeline::Builder& VideoEncoderPipeline::Builder::enable_chroma_filter(bool enable) {
    enable_chroma_filter_ = enable;
    return *this;
}

VideoEncoderPipeline::Builder& VideoEncoderPipeline::Builder::enable_luma_filter(bool enable) {
    enable_luma_filter_ = enable;
    return *this;
}

VideoEncoderPipeline::Builder& VideoEncoderPipeline::Builder::enable_yc_output(bool enable) {
    enable_yc_output_ = enable;
    return *this;
}

VideoEncoderPipeline::Builder& VideoEncoderPipeline::Builder::add_metadata_generator(
    std::unique_ptr<MetadataGenerator> generator) {
    generators_.push_back(std::move(generator));
    return *this;
}

VideoEncoderPipeline::Builder& VideoEncoderPipeline::Builder::set_metadata_generators(
    std::vector<std::unique_ptr<MetadataGenerator>> generators) {
    generators_ = std::move(generators);
    return *this;
}

VideoEncoderPipeline::Builder& VideoEncoderPipeline::Builder::add_field_effect(
    std::unique_ptr<FieldEffect> effect) {
    effects_.push_back(std::move(effect));
    return *this;
}

VideoEncoderPipeline::Builder& VideoEncoderPipeline::Builder::set_field_effects(
    std::vector<std::unique_ptr<FieldEffect>> effects) {
    effects_ = std::move(effects);
    return *this;
}

VideoEncoderPipeline::Builder& VideoEncoderPipeline::Builder::add_preprocessor(
    std::unique_ptr<FieldPreprocessor> preprocessor) {
    preprocessors_.push_back(std::move(preprocessor));
    return *this;
}

std::unique_ptr<VideoEncoderPipeline> VideoEncoderPipeline::Builder::build() {
    // Create appropriate active video encoder based on system
    std::unique_ptr<ActiveVideoEncoder> active_encoder;
    
    if (system_ == VideoSystem::PAL) {
        active_encoder = std::make_unique<PALActiveEncoder>(params_, enable_chroma_filter_, enable_luma_filter_);
    } else if (system_ == VideoSystem::NTSC) {
        active_encoder = std::make_unique<NTSCActiveEncoder>(params_, enable_chroma_filter_, enable_luma_filter_);
    } else {
        // Unknown video system
        return nullptr;
    }
    
    // Create pipeline with the encoder
    auto pipeline = std::unique_ptr<VideoEncoderPipeline>(
        new VideoEncoderPipeline(params_, std::move(active_encoder))
    );
    
    // Set Y/C output flag
    pipeline->enable_yc_output_ = enable_yc_output_;
    
    // Add metadata generators
    pipeline->generators_ = std::move(generators_);
    
    // Add field effects
    pipeline->effects_ = std::move(effects_);
    
    // Add preprocessors
    pipeline->preprocessors_ = std::move(preprocessors_);
    
    return pipeline;
}

// VideoEncoderPipeline implementation
VideoEncoderPipeline::VideoEncoderPipeline(const VideoParameters& params,
                                          std::unique_ptr<ActiveVideoEncoder> active_encoder)
    : params_(params),
      active_encoder_(std::move(active_encoder)),
      field_splitter_(std::make_unique<FieldSplitter>()),
      structure_gen_(std::make_unique<FieldStructureGenerator>(params)) {
}

Frame VideoEncoderPipeline::encode_frame(const FrameBuffer& frame_buffer, int32_t field_number,
                                        const VBIData* vbi_data_field1,
                                        const VBIData* vbi_data_field2,
                                        int32_t vitc_frame_offset) {
    Frame frame(params_.field_width, params_.field_height);
    
    // Split frame into fields
    auto field_pair = field_splitter_->split_frame(frame_buffer, field_number, params_);
    
    // Encode both fields
    frame.field1() = encode_field_from_yuv(field_pair.field1, field_number, true, vbi_data_field1, vitc_frame_offset);
    frame.field2() = encode_field_from_yuv(field_pair.field2, field_number + 1, false, vbi_data_field2, vitc_frame_offset);
    
    return frame;
}

Field VideoEncoderPipeline::encode_field(const FrameBuffer& frame_buffer, int32_t field_number,
                                        bool is_first_field, const VBIData* vbi_data,
                                        int32_t vitc_frame_offset) {
    // Split frame and extract appropriate field
    auto field_pair = field_splitter_->split_frame(frame_buffer, field_number, params_);
    const Field& field_yuv = is_first_field ? field_pair.field1 : field_pair.field2;
    
    // Encode the field
    return encode_field_from_yuv(field_yuv, field_number, is_first_field, vbi_data, vitc_frame_offset);
}

Field VideoEncoderPipeline::encode_field_from_yuv(const Field& field_yuv,
                                                  int32_t field_number,
                                                  bool is_first_field,
                                                  const VBIData* vbi_data,
                                                  int32_t vitc_frame_offset) {
    // Get field height (field1: 312/262, field2: 313/263)
    int32_t field_height = is_first_field ? params_.field1_height : params_.field2_height;
    VideoSystem system = active_encoder_->get_video_system();
    
    // Get active lines boundaries based on system
    int32_t active_lines_start, active_lines_end, vsync_lines;
    if (system == VideoSystem::PAL) {
        vsync_lines = 5;          // Lines 0-4
        active_lines_start = 23;  // Line 23
        active_lines_end = field_height - 3;  // 3 lines from bottom (blanking)
    } else {
        vsync_lines = 3;          // Lines 0-2
        active_lines_start = 21;  // Line 21
        active_lines_end = field_height - 2;  // 2 lines from bottom (blanking)
    }
    
    // Stage 1: Generate field structure (sync, blanking, color burst)
    StructuredField structured = structure_gen_->create_field_structure(
        Field(),  // Empty source field - we'll fill it with our data
        is_first_field,
        field_number,
        system
    );
    
    Field field = std::move(structured.field_data);

    // Preserve any audio attached to the input field
    if (field_yuv.has_audio()) {
        field.set_audio(field_yuv.audio());
    }
    
    // Get YUV plane info early (needed for field_width)
    int32_t field_width = field_yuv.width();
    int32_t output_field_width = field.width();  // Output field width (may differ from input)
    
    // Create Y and C fields if Y/C output is enabled
    Field* y_field_ptr = nullptr;
    Field* c_field_ptr = nullptr;
    if (enable_yc_output_) {
        // For Y/C output: generate separate structures
        // Y field gets sync/blanking only (no color burst)
        StructuredField y_structured = structure_gen_->create_field_structure_without_burst(
            Field(),
            is_first_field,
            field_number,
            system
        );
        field.y_field() = std::move(y_structured.field_data);
        y_field_ptr = &field.y_field();
        
        // C field initialized to mid-level (32768) and gets color burst added
        field.c_field().resize(output_field_width, field.height());
        c_field_ptr = &field.c_field();
        // Initialize C field with mid-level chroma (no signal)
        for (int32_t line = 0; line < field_height; ++line) {
            uint16_t* c_line = c_field_ptr->line_data(line);
            std::fill(c_line, c_line + output_field_width, 32768);
        }
        // Add color burst to C field centered at 32768 (mid-level)
        structure_gen_->add_color_burst_to_field(*c_field_ptr, field_number, is_first_field, system, 32768);
    }
    
    // Get YUV plane pointers (planar layout: Y, U, V)
    int32_t source_field_height = field_yuv.height() / 3;
    
    // Detect studio range input (≤1023) to preserve sub-black
    const uint16_t* yuv_data = field_yuv.data().data();
    const int32_t total_pixels = field_width * source_field_height;
    uint16_t y_max = 0;
    for (int32_t i = 0; i < total_pixels; ++i) {
        if (yuv_data[i] > y_max) y_max = yuv_data[i];
    }
    const bool studio_range_input = (y_max <= 1023);
    
    // Extract plane pointers
    const uint16_t* y_plane = yuv_data;
    const uint16_t* u_plane = yuv_data + total_pixels;
    const uint16_t* v_plane = yuv_data + (total_pixels * 2);
    
    // Process each line in the field
    for (int32_t line = 0; line < field_height; ++line) {
        uint16_t* line_buffer = field.line_data(line);
        
        // For Y/C output, also update the Y field with active video luma
        uint16_t* y_line_buffer = nullptr;
        uint16_t* c_line_buffer = nullptr;
        if (y_field_ptr) {
            y_line_buffer = y_field_ptr->line_data(line);
        }
        if (c_field_ptr) {
            c_line_buffer = c_field_ptr->line_data(line);
        }
        
        // Skip VSYNC lines (already properly generated by structure generator)
        if (line < vsync_lines) {
            continue;
        }
        
        // VBI and blanking lines
        if (line < active_lines_start) {
            // Metadata generators will be integrated in Phase 5b
            // For now, structure generator has already set sync and color burst
        }
        // Active video lines
        else if (line < active_lines_end) {
            int32_t line_in_field = line - active_lines_start;
            
            if (line_in_field < source_field_height) {
                // Get pointers to YUV line data
                const uint16_t* y_line = y_plane + (line_in_field * field_width);
                const uint16_t* u_line = u_plane + (line_in_field * field_width);
                const uint16_t* v_line = v_plane + (line_in_field * field_width);
                
                // Stage 4: Encode active video
                active_encoder_->encode_active_line(
                    line_buffer, y_line, u_line, v_line,
                    line, field_number, is_first_field,
                    field_width, studio_range_input,
                    y_line_buffer, c_line_buffer
                );
                
                // For Y/C output, ensure post-active-video region of C field stays at mid-level
                if (c_line_buffer && params_.active_video_end < output_field_width) {
                    std::fill(c_line_buffer + params_.active_video_end, c_line_buffer + output_field_width, 32768);
                }
            }
        }
        // Post-video blanking lines
        else {
            // Already generated by structure generator
        }
    }
    
    // Stage 5: Apply metadata generators (VBI, VITC, VITS, etc.)
    if (!generators_.empty()) {
        MetadataContext ctx;
        ctx.field_number = field_number;
        ctx.total_frame = field_number / 2;
        ctx.is_first_field = is_first_field;
        ctx.system = system;
        ctx.vbi_data = vbi_data;
        ctx.vitc_frame_offset = vitc_frame_offset;

        // When Y/C output is enabled, route generators to appropriate fields
        if (enable_yc_output_ && y_field_ptr && c_field_ptr) {
            for (auto& generator : generators_) {
                if (generator && generator->is_applicable(ctx)) {
                    // Color burst and other chroma-related generators go to C field
                    if (generator->applies_to_c_field_for_yc()) {
                        ctx.is_c_field_for_yc = true;
                        generator->apply(*c_field_ptr, ctx);
                        ctx.is_c_field_for_yc = false;
                    } else {
                        // VITS, VBI, VITC, etc. go to Y field
                        generator->apply(*y_field_ptr, ctx);
                    }
                }
            }
        } else {
            // For composite output, apply metadata to full field
            for (auto& generator : generators_) {
                if (generator && generator->is_applicable(ctx)) {
                    generator->apply(field, ctx);
                }
            }
        }
    }

    // Stage 7: Apply field effects (noise, dropouts, etc.)
    FieldEffectContext effect_context;
    effect_context.field_number = field_number;
    effect_context.line_number = 0;
    effect_context.is_first_field = is_first_field;
    effect_context.signal_level_white = 55000;  // Approximate white level in 16-bit scale
    effect_context.signal_level_black = 4096;   // Approximate black level

    for (auto& effect : effects_) {
        if (effect && effect->is_enabled()) {
            if (enable_yc_output_ && y_field_ptr && c_field_ptr) {
                // Apply to Y field first
                effect_context.field_type = FieldType::Y;
                effect->apply(*y_field_ptr, effect_context);
                
                // Then apply to C field (will reuse dropout decisions from Y field)
                effect_context.field_type = FieldType::C;
                effect->apply(*c_field_ptr, effect_context);
            } else {
                // Composite mode
                effect_context.field_type = FieldType::Composite;
                effect->apply(field, effect_context);
            }
        }
    }
    
    return field;
}

void VideoEncoderPipeline::add_field_effect(std::unique_ptr<FieldEffect> effect) {
    effects_.push_back(std::move(effect));
}

void VideoEncoderPipeline::clear_field_effects() {
    effects_.clear();
}

bool VideoEncoderPipeline::has_field_effects() const {
    return !effects_.empty();
}

void VideoEncoderPipeline::add_preprocessor(std::unique_ptr<FieldPreprocessor> preprocessor) {
    preprocessors_.push_back(std::move(preprocessor));
}

void VideoEncoderPipeline::clear_preprocessors() {
    preprocessors_.clear();
}

bool VideoEncoderPipeline::has_preprocessors() const {
    return !preprocessors_.empty();
}

DropoutSimulator* VideoEncoderPipeline::get_dropout_simulator() {
    for (auto& effect : effects_) {
        auto* dropout = dynamic_cast<DropoutSimulator*>(effect.get());
        if (dropout != nullptr) {
            return dropout;
        }
    }
    return nullptr;
}

const DropoutSimulator* VideoEncoderPipeline::get_dropout_simulator() const {
    for (const auto& effect : effects_) {
        const auto* dropout = dynamic_cast<const DropoutSimulator*>(effect.get());
        if (dropout != nullptr) {
            return dropout;
        }
    }
    return nullptr;
}

}  // namespace encode_orc
