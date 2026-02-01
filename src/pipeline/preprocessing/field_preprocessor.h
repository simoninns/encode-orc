/*
 * File:        field_preprocessor.h
 * Module:      encode-orc
 * Purpose:     Field preprocessing with optional filtering (Phase 6)
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: 2026 Simon Inns
 */

#ifndef ENCODE_ORC_FIELD_PREPROCESSOR_H
#define ENCODE_ORC_FIELD_PREPROCESSOR_H

#include "field.h"
#include "video_parameters.h"
#include <cstdint>
#include <memory>
#include <vector>

namespace encode_orc {

/**
 * @brief Abstract base class for field preprocessing filters
 * 
 * Preprocessing filters operate on YUV field data before active video encoding.
 * This is Stage 3 of the pipeline.
 */
class FieldPreprocessor {
public:
    virtual ~FieldPreprocessor() = default;
    
    /**
     * @brief Get filter type name
     */
    virtual std::string filter_type() const = 0;
    
    /**
     * @brief Apply filter to Y component data (luma)
     * @param y_data Luma data to filter (in-place)
     * @param width Width in samples
     * @param height Height in lines
     */
    virtual void apply_luma(uint16_t* y_data, int32_t width, int32_t height) = 0;
    
    /**
     * @brief Apply filter to U component data (chroma)
     * @param u_data Chroma U data to filter (in-place)
     * @param width Width in samples
     * @param height Height in lines
     */
    virtual void apply_chroma_u(uint16_t* u_data, int32_t width, int32_t height) = 0;
    
    /**
     * @brief Apply filter to V component data (chroma)
     * @param v_data Chroma V data to filter (in-place)
     * @param width Width in samples
     * @param height Height in lines
     */
    virtual void apply_chroma_v(uint16_t* v_data, int32_t width, int32_t height) = 0;
};

/**
 * @brief Chroma low-pass filter (1.3 MHz for PAL, 600 kHz for NTSC)
 * 
 * Removes high-frequency chroma artifacts before encoding.
 */
class ChromaFilter : public FieldPreprocessor {
public:
    enum FilterType {
        NONE,           ///< No filtering
        PAL_1_3MHZ,     ///< PAL standard 1.3 MHz cutoff
        NTSC_600KHZ,    ///< NTSC standard 600 kHz cutoff
        CUSTOM          ///< Custom filter coefficients
    };
    
    /**
     * @brief Construct chroma filter with specified type
     */
    explicit ChromaFilter(FilterType type = PAL_1_3MHZ);
    
    /**
     * @brief Construct with custom filter coefficients
     */
    explicit ChromaFilter(const std::vector<double>& custom_coefficients);
    
    /**
     * @brief Enable or disable the filter
     */
    void set_enabled(bool enabled) { enabled_ = enabled; }
    
    /**
     * @brief Check if filter is enabled
     */
    bool is_enabled() const { return enabled_; }
    
    /**
     * @brief Apply luma filtering (no-op for chroma filter)
     */
    void apply_luma(uint16_t* y_data, int32_t width, int32_t height) override;
    
    /**
     * @brief Apply chroma U filtering
     */
    void apply_chroma_u(uint16_t* u_data, int32_t width, int32_t height) override;
    
    /**
     * @brief Apply chroma V filtering
     */
    void apply_chroma_v(uint16_t* v_data, int32_t width, int32_t height) override;
    
    /**
     * @brief Get filter type
     */
    std::string filter_type() const override { return "chroma"; }
    
private:
    bool enabled_ = true;
    [[maybe_unused]] FilterType type_;
    std::vector<double> coefficients_;
    
    /**
     * @brief Get standard filter coefficients for PAL 1.3 MHz
     */
    static std::vector<double> get_pal_1_3mhz_coefficients();
    
    /**
     * @brief Get standard filter coefficients for NTSC 600 kHz
     */
    static std::vector<double> get_ntsc_600khz_coefficients();
    
    /**
     * @brief Apply FIR filter to a component channel
     */
    void apply_filter(uint16_t* data, int32_t width, int32_t height);
};

/**
 * @brief Luma low-pass filter (5.5 MHz for PAL, 3.6 MHz for NTSC)
 * 
 * Removes high-frequency luma artifacts before encoding.
 */
class LumaFilter : public FieldPreprocessor {
public:
    enum FilterType {
        NONE,           ///< No filtering
        PAL_5_5MHZ,     ///< PAL standard 5.5 MHz cutoff
        NTSC_3_6MHZ,    ///< NTSC standard 3.6 MHz cutoff
        CUSTOM          ///< Custom filter coefficients
    };
    
    /**
     * @brief Construct luma filter with specified type
     */
    explicit LumaFilter(FilterType type = PAL_5_5MHZ);
    
    /**
     * @brief Construct with custom filter coefficients
     */
    explicit LumaFilter(const std::vector<double>& custom_coefficients);
    
    /**
     * @brief Enable or disable the filter
     */
    void set_enabled(bool enabled) { enabled_ = enabled; }
    
    /**
     * @brief Check if filter is enabled
     */
    bool is_enabled() const { return enabled_; }
    
    /**
     * @brief Apply luma filtering
     */
    void apply_luma(uint16_t* y_data, int32_t width, int32_t height) override;
    
    /**
     * @brief Apply chroma filtering (no-op for luma filter)
     */
    void apply_chroma_u(uint16_t* u_data, int32_t width, int32_t height) override;
    
    /**
     * @brief Apply chroma V filtering (no-op for luma filter)
     */
    void apply_chroma_v(uint16_t* v_data, int32_t width, int32_t height) override;
    
    /**
     * @brief Get filter type
     */
    std::string filter_type() const override { return "luma"; }
    
private:
    bool enabled_ = false;  // Luma filtering disabled by default
    [[maybe_unused]] FilterType type_;
    std::vector<double> coefficients_;
    
    /**
     * @brief Get standard filter coefficients for PAL 5.5 MHz
     */
    static std::vector<double> get_pal_5_5mhz_coefficients();
    
    /**
     * @brief Get standard filter coefficients for NTSC 3.6 MHz
     */
    static std::vector<double> get_ntsc_3_6mhz_coefficients();
    
    /**
     * @brief Apply FIR filter to a component channel
     */
    void apply_filter(uint16_t* data, int32_t width, int32_t height);
};

} // namespace encode_orc

#endif // ENCODE_ORC_FIELD_PREPROCESSOR_H
