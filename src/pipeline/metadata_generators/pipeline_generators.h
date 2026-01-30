#ifndef PIPELINE_GENERATORS_H
#define PIPELINE_GENERATORS_H

#include "metadata_generator_base.h"
#include "color_burst_generator.h"
#include "vitc_generator.h"
#include "pal_vits_generator.h"
#include "ntsc_vits_generator.h"
#include "biphase_encoder.h"
#include "vbi_metadata_generator.h"
#include "video_parameters.h"
#include <memory>

namespace encode_orc {

/**
 * @brief Pipeline-compatible color burst generator
 * 
 * Adds color burst reference signal to all active video and VBI lines
 */
class ColorBurstMetadataGenerator : public MetadataGenerator {
public:
    explicit ColorBurstMetadataGenerator(const VideoParameters& params);
    
    void apply(encode_orc::Field& field, const MetadataContext& context) override;
    std::vector<int32_t> affected_lines() const override;
    std::string name() const override { return "ColorBurst"; }
    
private:
    VideoParameters params_;
    std::unique_ptr<ColorBurstGenerator> generator_;
    VideoSystem system_;
};

/**
 * @brief Pipeline-compatible VITC generator
 * 
 * Adds VITC timecode to specified lines (default: lines 19 and 21 for PAL, 14 and 16 for NTSC)
 */
class VITCMetadataGenerator : public MetadataGenerator {
public:
    explicit VITCMetadataGenerator(const VideoParameters& params, 
                                   const std::vector<int32_t>& lines = {});
    
    void apply(encode_orc::Field& field, const MetadataContext& context) override;
    std::vector<int32_t> affected_lines() const override;
    std::string name() const override { return "VITC"; }
    
private:
    VideoParameters params_;
    std::unique_ptr<VITCGenerator> generator_;
    std::vector<int32_t> lines_;  // Which lines to encode VITC on (0-indexed)
    VideoSystem system_;
};

/**
 * @brief Pipeline-compatible VITS generator
 * 
 * Adds Vertical Interval Test Signals to specified lines
 */
class VITSMetadataGenerator : public MetadataGenerator {
public:
    explicit VITSMetadataGenerator(const VideoParameters& params);
    
    void apply(encode_orc::Field& field, const MetadataContext& context) override;
    std::vector<int32_t> affected_lines() const override;
    std::string name() const override { return "VITS"; }
    
private:
    VideoParameters params_;
    std::unique_ptr<PALVITSGenerator> pal_generator_;
    std::unique_ptr<NTSCVITSGenerator> ntsc_generator_;
    VideoSystem system_;
};

// Forward declarations for VITS signal types (defined in vits_pipeline_generator.h)
enum class VITSSignalType;
struct VITSSignalConfig;
bool parse_vits_signal_type(const std::string& str, VITSSignalType& type);

/**
 * @brief Pipeline-compatible PAL VITS generator with configurable signals
 * 
 * Adds PAL Vertical Interval Test Signals to configured lines/fields
 */
class PALVITSMetadataGenerator : public MetadataGenerator {
public:
    explicit PALVITSMetadataGenerator(const VideoParameters& params,
                                      const std::vector<VITSSignalConfig>& signals);
    
    void apply(encode_orc::Field& field, const MetadataContext& context) override;
    std::vector<int32_t> affected_lines() const override;
    std::string name() const override { return "PAL-VITS"; }
    
private:
    VideoParameters params_;
    std::unique_ptr<PALVITSGenerator> generator_;
    std::vector<VITSSignalConfig> signals_;
};

/**
 * @brief Pipeline-compatible NTSC VITS generator with configurable signals
 * 
 * Adds NTSC Vertical Interval Test Signals to configured lines/fields
 */
class NTSCVITSMetadataGenerator : public MetadataGenerator {
public:
    explicit NTSCVITSMetadataGenerator(const VideoParameters& params,
                                       const std::vector<VITSSignalConfig>& signals);
    
    void apply(encode_orc::Field& field, const MetadataContext& context) override;
    std::vector<int32_t> affected_lines() const override;
    std::string name() const override { return "NTSC-VITS"; }
    
private:
    VideoParameters params_;
    std::unique_ptr<NTSCVITSGenerator> generator_;
    std::vector<VITSSignalConfig> signals_;
};

/**
 * @brief Pipeline-compatible biphase VBI generator
 * 
 * Adds LaserDisc VBI data (biphase-encoded) to lines 16, 17, 18
 */
class BiphaseVBIMetadataGenerator : public MetadataGenerator {
public:
    explicit BiphaseVBIMetadataGenerator(const VideoParameters& params,
                                         const std::vector<int32_t>& lines = {15, 16, 17});
    
    void apply(encode_orc::Field& field, const MetadataContext& context) override;
    std::vector<int32_t> affected_lines() const override;
    std::string name() const override { return "BiphaseVBI"; }
    
private:
    VideoParameters params_;
    std::vector<int32_t> lines_;  // Which lines to encode VBI on (0-indexed)
};

} // namespace encode_orc

#endif // PIPELINE_GENERATORS_H
