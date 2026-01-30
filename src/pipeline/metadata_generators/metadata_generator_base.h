/*
 * File:        metadata_generator_base.h
 * Module:      encode-orc
 * Purpose:     Abstract base class for pipeline metadata generators (Phase 4)
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: 2026 Simon Inns
 */

#ifndef ENCODE_ORC_METADATA_GENERATOR_BASE_H
#define ENCODE_ORC_METADATA_GENERATOR_BASE_H

#include "field.h"
#include "pipeline_metadata_generator.h"
#include <vector>
#include <cstdint>
#include <string>

namespace encode_orc {

// Re-use MetadataContext from pipeline_metadata_generator.h (no duplication)

/**
 * @brief Abstract base class for all pipeline metadata generators (Phase 4)
 * 
 * Metadata generators are composable pipeline stages that add specific
 * types of metadata to fields:
 * - ColorBurstMetadataGenerator: Color burst reference signal
 * - VITCMetadataGenerator: Consumer tape timecode (lines 19, 21)
 * - VITSMetadataGenerator: Vertical interval test signals
 * - BiphaseVBIMetadataGenerator: LaserDisc VBI frame numbers (lines 16-18)
 * 
 * Each generator is independent and can be enabled/disabled via YAML configuration.
 */
class MetadataGenerator {
public:
    virtual ~MetadataGenerator() = default;
    
    /**
     * @brief Apply this generator's metadata to a field
     * 
     * This method modifies the field by writing metadata to specific lines.
     * 
     * @param field Field to modify
     * @param context Metadata context (field number, VBI data, etc.)
     */
    virtual void apply(Field& field, const MetadataContext& context) = 0;
    
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

#endif // ENCODE_ORC_METADATA_GENERATOR_BASE_H
