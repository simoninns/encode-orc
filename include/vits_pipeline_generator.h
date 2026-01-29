/*
 * File:        vits_pipeline_generator.h
 * Module:      encode-orc
 * Purpose:     Pipeline generators for PAL and NTSC VITS test signals
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: 2026 Simon Inns
 */

#ifndef ENCODE_ORC_VITS_PIPELINE_GENERATOR_H
#define ENCODE_ORC_VITS_PIPELINE_GENERATOR_H

#include "pipeline_metadata_generator.h"
#include "pal_vits_generator.h"
#include "ntsc_vits_generator.h"
#include <memory>
#include <map>

namespace encode_orc {

/**
 * @brief Signal type for VITS generation
 */
enum class VITSSignalType {
    ITU_COMPOSITE,      ///< ITU Composite Test Signal
    UK_NATIONAL,        ///< UK PAL National Test Signal #1
    ITU_ITS,           ///< ITU Combination ITS
    MULTIBURST         ///< ITU Multiburst Test Signal
};

/**
 * @brief Configuration for a single VITS signal
 */
struct VITSSignalConfig {
    int32_t line;              ///< Line number (0-indexed)
    int32_t field;             ///< Field number (1 or 2)
    VITSSignalType signal;     ///< Type of signal to generate
};

/**
 * @brief Pipeline generator for PAL VITS test signals
 * 
 * Wraps PALVITSGenerator to work as a pipeline component.
 * Generates vertical interval test signals on configured lines.
 */
class PALVITSPipelineGenerator : public PipelineMetadataGenerator {
public:
    /**
     * @brief Configuration for PAL VITS generator
     */
    struct Config {
        std::vector<VITSSignalConfig> signals;  // List of signals to generate
    };
    
    /**
     * @brief Construct a PAL VITS pipeline generator
     * @param params Video parameters
     * @param config Configuration (which signals on which lines)
     */
    explicit PALVITSPipelineGenerator(const VideoParameters& params, const Config& config = Config());
    
    void apply(StructuredField& field, const MetadataContext& context) override;
    std::vector<int32_t> affected_lines() const override;
    std::string name() const override;
    bool is_applicable(const MetadataContext& context) const override;
    
private:
    std::unique_ptr<PALVITSGenerator> vits_gen_;
    Config config_;
};

/**
 * @brief Pipeline generator for NTSC VITS test signals
 * 
 * Wraps NTSCVITSGenerator to work as a pipeline component.
 * Generates vertical interval test signals on configured lines.
 */
class NTSCVITSPipelineGenerator : public PipelineMetadataGenerator {
public:
    /**
     * @brief Configuration for NTSC VITS generator
     */
    struct Config {
        std::vector<VITSSignalConfig> signals;  // List of signals to generate
    };
    
    /**
     * @brief Construct an NTSC VITS pipeline generator
     * @param params Video parameters
     * @param config Configuration (which signals on which lines)
     */
    explicit NTSCVITSPipelineGenerator(const VideoParameters& params, const Config& config = Config());
    
    void apply(StructuredField& field, const MetadataContext& context) override;
    std::vector<int32_t> affected_lines() const override;
    std::string name() const override;
    bool is_applicable(const MetadataContext& context) const override;
    
private:
    std::unique_ptr<NTSCVITSGenerator> vits_gen_;
    Config config_;
};

/**
 * @brief Helper function to parse VITS signal type from string
 */
bool parse_vits_signal_type(const std::string& str, VITSSignalType& type);

/**
 * @brief Helper function to convert VITS signal type to string
 */
std::string vits_signal_type_to_string(VITSSignalType type);

} // namespace encode_orc

#endif // ENCODE_ORC_VITS_PIPELINE_GENERATOR_H
