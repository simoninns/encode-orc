/*
 * File:        fir_filter.h
 * Module:      encode-orc
 * Purpose:     FIR (Finite Impulse Response) filter for bandpass filtering of Y/U/V signals
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: 2026 Simon Inns
 *
 * Based on ld-chroma-encoder implementation from ld-decode-tools
 */

#ifndef ENCODE_ORC_FIR_FILTER_H
#define ENCODE_ORC_FIR_FILTER_H

#include <vector>
#include <array>
#include <cstdint>
#include <algorithm>
#include <cassert>

namespace encode_orc {

/**
 * @brief FIR filter with arbitrary coefficients
 * 
 * A Finite Impulse Response filter to prevent high-frequency artifacts
 * when encoding composite video. Applies low-pass filtering to remove
 * frequencies above the video bandwidth limits.
 * 
 * The number of filter taps must be odd to ensure zero phase distortion.
 */
class FIRFilter {
public:
    /**
     * @brief Construct a FIR filter with given coefficients
     * @param coefficients Filter tap coefficients (must have odd length)
     */
    explicit FIRFilter(const std::vector<double>& coefficients)
        : coeffs_(coefficients)
    {
        assert((coefficients.size() % 2) == 1);
    }

    /**
     * @brief Apply filter to a vector of samples (in-place)
     * @param samples Input/output samples to filter
     */
    void apply(std::vector<double>& samples) const {
        if (samples.empty()) return;

        thread_local std::vector<double> tmp;
        tmp.resize(samples.size());
        apply_internal(samples.data(), tmp.data(), static_cast<int>(samples.size()));
        std::copy(tmp.begin(), tmp.end(), samples.begin());
    }

    /**
     * @brief Apply filter to a vector of samples (in-place, 16-bit)
     * @param samples Input/output samples to filter
     */
    void apply(std::vector<uint16_t>& samples) const {
        if (samples.empty()) return;

        thread_local std::vector<double> tmp_double;
        tmp_double.resize(samples.size());

        for (size_t i = 0; i < samples.size(); ++i) {
            tmp_double[i] = static_cast<double>(samples[i]);
        }

        apply_internal_16bit(tmp_double.data(), samples.data(), static_cast<int>(samples.size()));
    }

    /**
     * @brief Check if filter is valid (has odd number of taps)
     */
    bool is_valid() const {
        return (coeffs_.size() % 2) == 1 && !coeffs_.empty();
    }

    /**
     * @brief Get number of filter taps
     */
    size_t num_taps() const {
        return coeffs_.size();
    }

private:
    std::vector<double> coeffs_;

    /**
     * @brief Internal filter application for double samples
     * Uses edge reflection padding to avoid amplitude distortion at boundaries
     */
    void apply_internal(const double* input_data, double* output_data, int num_samples) const {
        const int num_taps = static_cast<int>(coeffs_.size());
        const int overlap = num_taps / 2;

        // Create padded version with reflection at edges to avoid artifacts
        thread_local std::vector<double> padded;
        padded.resize(num_samples + 2 * overlap);
        
        // Reflect at left edge
        for (int i = 0; i < overlap; ++i) {
            padded[overlap - 1 - i] = input_data[i];
        }
        
        // Copy center
        for (int i = 0; i < num_samples; ++i) {
            padded[overlap + i] = input_data[i];
        }
        
        // Reflect at right edge
        for (int i = 0; i < overlap; ++i) {
            padded[overlap + num_samples + i] = input_data[num_samples - 2 - i];
        }
        
        // Apply filter to entire padded signal
        for (int i = 0; i < num_samples; ++i) {
            double v = 0.0;
            for (int j = 0; j < num_taps; ++j) {
                v += coeffs_[j] * padded[i + j];
            }
            output_data[i] = v;
        }
    }

    /**
     * @brief Internal filter application converting to/from 16-bit
     * Uses edge reflection padding to avoid amplitude distortion at boundaries
     */
    void apply_internal_16bit(const double* input_double, uint16_t* output_data, int num_samples) const {
        const int num_taps = static_cast<int>(coeffs_.size());
        const int overlap = num_taps / 2;

        // Create padded version with reflection at edges to avoid artifacts
        thread_local std::vector<double> padded;
        padded.resize(num_samples + 2 * overlap);
        
        // Reflect at left edge
        for (int i = 0; i < overlap; ++i) {
            padded[overlap - 1 - i] = input_double[i];
        }
        
        // Copy center
        for (int i = 0; i < num_samples; ++i) {
            padded[overlap + i] = input_double[i];
        }
        
        // Reflect at right edge
        for (int i = 0; i < overlap; ++i) {
            padded[overlap + num_samples + i] = input_double[num_samples - 2 - i];
        }
        
        // Apply filter to entire padded signal
        for (int i = 0; i < num_samples; ++i) {
            double v = 0.0;
            for (int j = 0; j < num_taps; ++j) {
                v += coeffs_[j] * padded[i + j];
            }
            output_data[i] = static_cast<uint16_t>(v);
        }
    }
};

/**
 * @brief Predefined filter configurations
 */
namespace Filters {

    /**
     * @brief Create a 1.3 MHz low-pass filter for PAL
     * 
     * 13-tap Gaussian window filter generated by:
     * c = scipy.signal.gaussian(13, 1.52); c / sum(c)
     * 
     * Specifications: 0 dB @ 0 Hz, >= -3 dB @ 1.3 MHz, <= -20 dB @ 4.0 MHz
     * Reference: Clarke p8, Poynton p342
     */
    inline FIRFilter create_pal_uv_filter() {
        const std::vector<double> coeffs{
            0.00010852890120228184,
            0.0011732778293138913,
            0.008227778710181127,
            0.03742748297181873,
            0.11043962430879829,
            0.21139051659718247,
            0.2624655813630064,
            0.21139051659718247,
            0.11043962430879829,
            0.03742748297181873,
            0.008227778710181127,
            0.0011732778293138913,
            0.00010852890120228184
        };
        return FIRFilter(coeffs);
    }

    /**
     * @brief Create a 1.3 MHz low-pass filter for NTSC
     * 
     * 9-tap filter for U/V (I/Q wideband) chroma components
     * Specifications: 0 dB @ 0 Hz, >= -2 dB @ 1.3 MHz, < -20 dB @ 3.6 MHz
     * Reference: Clarke p15, Poynton p342
     */
    inline FIRFilter create_ntsc_uv_filter() {
        const std::vector<double> coeffs{
            0.0021, 0.0191, 0.0903, 0.2308, 0.3153,
            0.2308, 0.0903, 0.0191, 0.0021
        };
        return FIRFilter(coeffs);
    }

    /**
     * @brief Create a 0.6 MHz low-pass filter for NTSC Q channel
     * 
     * 23-tap filter for narrowband Q (I/Q) mode only
     * Specifications: 0 dB @ 0 Hz, >= -2 dB @ 0.4 MHz, >= -6 dB @ 0.5 MHz, <= -6 dB @ 0.6 MHz
     * Reference: Clarke p15
     */
    inline FIRFilter create_ntsc_q_filter() {
        const std::vector<double> coeffs{
            0.0002, 0.0027, 0.0085, 0.0171, 0.0278, 0.0398, 0.0522, 0.0639, 0.0742, 0.0821, 0.0872,
            0.0889,
            0.0872, 0.0821, 0.0742, 0.0639, 0.0522, 0.0398, 0.0278, 0.0171, 0.0085, 0.0027, 0.0002
        };
        return FIRFilter(coeffs);
    }

} // namespace Filters

} // namespace encode_orc

#endif // ENCODE_ORC_FIR_FILTER_H
