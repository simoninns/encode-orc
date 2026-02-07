/*
 * File:        pipeline_metadata_generator.h
 * Module:      encode-orc
 * Purpose:     Abstract base class for pipeline metadata generators (VBI, VITS, VITC, burst)
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: 2026 Simon Inns
 */

#ifndef ENCODE_ORC_PIPELINE_METADATA_GENERATOR_H
#define ENCODE_ORC_PIPELINE_METADATA_GENERATOR_H

#include "field_structure_generator.h"
#include "metadata.h"
#include <vector>
#include <cstdint>
#include <string>

namespace encode_orc {

/**
 * @brief Context information passed to metadata generators
 * 
 * Contains all the information a generator might need to create
 * appropriate metadata for a specific field.
 */
struct MetadataContext {
    int32_t field_number = 0;       ///< Absolute field number (0-indexed)
    int32_t total_frame = 0;        ///< Absolute frame number (0-indexed)
    bool is_first_field = false;    ///< First or second field of the frame
    VideoSystem system = VideoSystem::PAL;  ///< Video system (PAL/NTSC)
    bool is_c_field_for_yc = false; ///< True if this is C field for Y/C output
    
    // VBI data (for LaserDisc)
    const VBIData* vbi_data = nullptr;  ///< Optional VBI frame numbers
    
    // VITC offset (for consumer tape timecode)
    int32_t vitc_frame_offset = 0;  ///< Frame offset for VITC timecode
};

/**
 * @brief Abstract base class for all pipeline metadata generators
 * 
 * Metadata generators are composable pipeline stages that add specific
 * types of metadata to structured fields:
 * - BiphaseVBIGenerator: LaserDisc VBI frame numbers (lines 16-18)
 * - VITCGenerator: Consumer tape timecode (lines 19, 21)
 * - PALVITSGenerator: PAL vertical interval test signals
 * - NTSCVITSGenerator: NTSC vertical interval test signals
 * - ColorBurstGenerator: Color burst reference signal
 * 
 * Each generator is independent and can be enabled/disabled via YAML configuration.
 */
class PipelineMetadataGenerator {
public:
    virtual ~PipelineMetadataGenerator() = default;
    
    /**
     * @brief Apply this generator's metadata to a structured field
     * 
     * This method modifies the field_data within the StructuredField,
     * typically by writing to specific lines or line ranges.
     * 
     * @param field Structured field to modify (has sync/blanking already)
     * @param context Metadata context (field number, VBI data, etc.)
     */
    virtual void apply(StructuredField& field, const MetadataContext& context) = 0;
    
    /**
     * @brief Get which lines this generator affects
     * 
     * Returns a list of line numbers (0-indexed) that this generator
     * will modify. This is useful for validation and debugging.
     * 
     * @return Vector of affected line numbers (empty if affects all/many lines)
     */
    virtual std::vector<int32_t> affected_lines() const = 0;
    
    /**
     * @brief Get a human-readable name for this generator
     */
    virtual std::string name() const = 0;
    
    /**
     * @brief Check if this generator is applicable to the given context
     * 
     * Some generators are only applicable to specific video systems,
     * source standards, or field types. This allows generators to
     * skip processing when not applicable.
     * 
     * @param context Metadata context
     * @return true if this generator should be applied
     */
    virtual bool is_applicable([[maybe_unused]] const MetadataContext& context) const {
        // Default: always applicable
        return true;
    }
};

} // namespace encode_orc

#endif // ENCODE_ORC_PIPELINE_METADATA_GENERATOR_H
