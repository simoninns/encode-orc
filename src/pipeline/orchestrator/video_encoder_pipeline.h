/*
 * File:        video_encoder_pipeline.h
 * Module:      encode-orc
 * Purpose:     Video encoder pipeline with Builder pattern (Phase 5)
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: 2026 Simon Inns
 */

#ifndef ENCODE_ORC_VIDEO_ENCODER_PIPELINE_H
#define ENCODE_ORC_VIDEO_ENCODER_PIPELINE_H

#include "field.h"
#include "frame_buffer.h"
#include "video_parameters.h"
#include "active_video_encoder.h"
#include "field_splitter.h"
#include "field_structure_generator.h"
#include "metadata_generator_base.h"
#include "field_effect.h"
#include "field_preprocessor.h"
#include <cstdint>
#include <memory>
#include <vector>
#include <optional>

namespace encode_orc {

// Forward declarations
class VBIData;

/**
 * @brief Video encoder pipeline with composable stages (Phase 5)
 * 
 * This class implements the new Phase 5 architecture with clear separation of concerns:
 * 1. Field splitting (progressive → interlaced)
 * 2. Field structure generation (sync, blanking, color burst)
 * 3. Metadata generation (VITC, VITS, VBI, etc.)
 * 4. Active video encoding (YUV to composite with subcarrier modulation)
 * 
 * Uses Builder pattern for flexible configuration and YAML integration.
 */
class VideoEncoderPipeline {
public:
    /**
     * @brief Builder class for constructing VideoEncoderPipeline with fluent API
     */
    class Builder {
    public:
        /**
         * @brief Set the video system (PAL or NTSC)
         */
        Builder& set_system(VideoSystem system);
        
        /**
         * @brief Set video parameters
         */
        Builder& set_parameters(const VideoParameters& params);
        
        /**
         * @brief Enable chroma (U/V or I/Q) low-pass filtering (1.3 MHz)
         */
        Builder& enable_chroma_filter(bool enable = true);
        
        /**
         * @brief Enable luma (Y) low-pass filtering
         */
        Builder& enable_luma_filter(bool enable = true);
        
        /**
         * @brief Enable separate Y/C output generation
         */
        Builder& enable_yc_output(bool enable = true);
        
        /**
         * @brief Add a metadata generator to the pipeline
         */
        Builder& add_metadata_generator(std::unique_ptr<MetadataGenerator> generator);
        
        /**
         * @brief Set metadata generators (replaces any existing ones)
         */
        Builder& set_metadata_generators(std::vector<std::unique_ptr<MetadataGenerator>> generators);
        
        /**
         * @brief Add a field effect to the pipeline
         */
        Builder& add_field_effect(std::unique_ptr<FieldEffect> effect);
        
        /**
         * @brief Set field effects (replaces any existing ones)
         */
        Builder& set_field_effects(std::vector<std::unique_ptr<FieldEffect>> effects);
        
        /**
         * @brief Add a field preprocessor (filter)
         */
        Builder& add_preprocessor(std::unique_ptr<FieldPreprocessor> preprocessor);
        
        /**
         * @brief Build the pipeline
         */
        std::unique_ptr<VideoEncoderPipeline> build();
        
    private:
        VideoSystem system_ = VideoSystem::PAL;
        VideoParameters params_;
        bool enable_chroma_filter_ = true;
        bool enable_luma_filter_ = false;
        bool enable_yc_output_ = false;
        std::vector<std::unique_ptr<MetadataGenerator>> generators_;
        std::vector<std::unique_ptr<FieldEffect>> effects_;
        std::vector<std::unique_ptr<FieldPreprocessor>> preprocessors_;
    };
    
    /**
     * @brief Encode a progressive frame to two interlaced fields
     * @param frame_buffer Input frame in YUV444P16 format
     * @param field_number Starting field number (for timecode, V-switch calculation)
     * @param vbi_data Optional VBI data to encode in VBI lines
     * @return Frame containing two encoded composite fields
     */
    Frame encode_frame(const FrameBuffer& frame_buffer, int32_t field_number,
                      const VBIData* vbi_data = nullptr);
    
    /**
     * @brief Encode a single field
     * @param frame_buffer Input frame in YUV444P16 format
     * @param field_number Field number
     * @param is_first_field true for field1, false for field2
     * @param vbi_data Optional VBI data
     * @return Encoded composite field
     */
    Field encode_field(const FrameBuffer& frame_buffer, int32_t field_number,
                      bool is_first_field, const VBIData* vbi_data = nullptr);
    
    /**
     * @brief Get the video system
     */
    VideoSystem get_video_system() const { return active_encoder_->get_video_system(); }
    
    /**
     * @brief Get the video parameters
     */
    const VideoParameters& get_parameters() const { return params_; }
    
    /**
     * @brief Add metadata generator
     */
    void add_metadata_generator(std::unique_ptr<MetadataGenerator> generator);
    
    /**
     * @brief Clear all metadata generators
     */
    void clear_metadata_generators();
    
    /**
     * @brief Check if we have metadata generators
     */
    bool has_metadata_generators() const;
    
    /**
     * @brief Add field effect
     */
    void add_field_effect(std::unique_ptr<FieldEffect> effect);
    
    /**
     * @brief Clear all field effects
     */
    void clear_field_effects();
    
    /**
     * @brief Check if we have field effects
     */
    bool has_field_effects() const;
    
    /**
     * @brief Add field preprocessor (filter)
     */
    void add_preprocessor(std::unique_ptr<FieldPreprocessor> preprocessor);
    
    /**
     * @brief Clear all preprocessors
     */
    void clear_preprocessors();
    
    /**
     * @brief Check if we have preprocessors
     */
    bool has_preprocessors() const;
    
    /**
     * @brief Get the dropout simulator effect (if present)
     * @return Pointer to DropoutSimulator if found, nullptr otherwise
     */
    DropoutSimulator* get_dropout_simulator();
    
    /**
     * @brief Get const pointer to dropout simulator effect (if present)
     */
    const DropoutSimulator* get_dropout_simulator() const;

private:
    friend class Builder;
    
    VideoParameters params_;
    std::unique_ptr<ActiveVideoEncoder> active_encoder_;
    std::unique_ptr<FieldSplitter> field_splitter_;
    std::unique_ptr<FieldStructureGenerator> structure_gen_;
    std::vector<std::unique_ptr<MetadataGenerator>> generators_;
    std::vector<std::unique_ptr<FieldEffect>> effects_;
    std::vector<std::unique_ptr<FieldPreprocessor>> preprocessors_;
    bool enable_yc_output_ = false;
    
    /**
     * @brief Private constructor for Builder
     */
    VideoEncoderPipeline(const VideoParameters& params,
                        std::unique_ptr<ActiveVideoEncoder> active_encoder);
    
    /**
     * @brief Encode a single field from pre-split YUV data
     */
    Field encode_field_from_yuv(const Field& field_yuv,
                               int32_t field_number,
                               bool is_first_field,
                               const VBIData* vbi_data);
};

}  // namespace encode_orc

#endif  // ENCODE_ORC_VIDEO_ENCODER_PIPELINE_H
