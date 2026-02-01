/*
 * File:        field_effect.cpp
 * Module:      encode-orc
 * Purpose:     Field effects implementation (Phase 6)
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: 2026 Simon Inns
 */

#include "field_effect.h"
#include <cmath>
#include <algorithm>
#include <random>
#include <cstring>

namespace encode_orc {

// ============================================================================
// NoiseGenerator Implementation
// ============================================================================

NoiseGenerator::NoiseGenerator(double noise_level_db)
    : noise_level_db_(noise_level_db), snr_db_(40.0), use_snr_mode_(false) {}

NoiseGenerator NoiseGenerator::from_snr(double snr_db) {
    NoiseGenerator gen;
    gen.set_snr_db(snr_db);
    return gen;
}

void NoiseGenerator::set_noise_level_db(double noise_level_db) {
    noise_level_db_ = noise_level_db;
    use_snr_mode_ = false;
}

void NoiseGenerator::set_snr_db(double snr_db) {
    snr_db_ = snr_db;
    use_snr_mode_ = true;
}

void NoiseGenerator::set_seed(uint32_t seed) {
    seed_ = seed;
}

double NoiseGenerator::calculate_noise_from_snr(double signal_rms, double snr_db) {
    // SNR_db = 20 * log10(signal_rms / noise_rms)
    // Rearranging: noise_rms = signal_rms / 10^(SNR_db / 20)
    return signal_rms / std::pow(10.0, snr_db / 20.0);
}

double NoiseGenerator::generate_gaussian_random() {
    // Box-Muller transform to generate Gaussian random numbers
    static thread_local std::mt19937 rng(seed_);
    static thread_local std::uniform_real_distribution<double> uniform(0.0, 1.0);
    
    double u1 = uniform(rng);
    double u2 = uniform(rng);
    
    // Avoid log(0)
    if (u1 < 1e-10) u1 = 1e-10;
    if (u2 < 1e-10) u2 = 1e-10;
    
    double z = std::sqrt(-2.0 * std::log(u1)) * std::cos(2.0 * M_PI * u2);
    return z;
}

void NoiseGenerator::apply(Field& field, const FieldEffectContext& context) {
    if (!enabled_) return;
    
    // Calculate noise RMS from SNR if in SNR mode
    double noise_rms = 0.0;
    
    if (use_snr_mode_) {
        // Calculate signal RMS from white and black levels
        // Assume linear signal: black at 0, white at max_value
        double signal_peak = (context.signal_level_white - context.signal_level_black) / 2.0;
        double signal_rms = signal_peak / std::sqrt(2.0);  // RMS of sine wave
        
        if (signal_rms > 0) {
            noise_rms = calculate_noise_from_snr(signal_rms, snr_db_);
        }
    } else {
        // Convert dB to linear scale
        // noise_linear = 10^(noise_db / 20)
        noise_rms = std::pow(10.0, noise_level_db_ / 20.0) * 32768.0;  // Normalized to 16-bit
    }
    
    // Seed a local RNG so each field gets reproducible noise
    std::mt19937 rng(seed_ + context.field_number);
    
    // Use std::normal_distribution which is much faster than manual Box-Muller
    std::normal_distribution<double> gaussian_dist(0.0, noise_rms);
    
    // Add Gaussian noise to all samples
    auto& data = field.data();
    for (auto& sample : data) {
        double noise = gaussian_dist(rng);
        
        // Add noise and clamp to valid range [0, 65535]
        double noisy_sample = static_cast<double>(sample) + noise;
        noisy_sample = std::max(0.0, std::min(65535.0, noisy_sample));
        
        sample = static_cast<uint16_t>(noisy_sample + 0.5);  // Round to nearest
    }
}

// ============================================================================
// DropoutSimulator Implementation
// ============================================================================

DropoutSimulator::DropoutSimulator(double density, uint32_t seed)
    : density_(density), seed_(seed), last_processed_field_(-1) {}

void DropoutSimulator::apply(Field& field, const FieldEffectContext& context) {
    if (!enabled_) return;
    if (density_ <= 0.0) return;
    
    // Lock for thread safety - entire method modifies shared state
    std::lock_guard<std::mutex> lock(dropout_mutex_);
    
    // For YC mode: if this is C field and we have cached Y dropouts, use them
    if (context.field_type == FieldType::C && 
        cached_y_field_number_ == context.field_number && 
        !cached_y_dropouts_.empty()) {
        // Apply cached dropouts from Y field to C field
        apply_cached_dropouts(field, context);
        return;
    }
    
    // Clear dropouts from previous field
    last_field_dropouts_.clear();
    
    // For YC mode: if this is Y field, clear cache to prepare for new dropouts
    if (context.field_type == FieldType::Y) {
        cached_y_dropouts_.clear();
        cached_y_field_number_ = context.field_number;
    }
    
    auto& data = field.data();
    int32_t width = field.width();
    int32_t height = field.height();

    if (width <= 0 || height <= 0) return;

    // Random generator seeded per field for reproducibility
    std::mt19937 rng(seed_ + context.field_number);
    std::uniform_real_distribution<double> uniform01(0.0, 1.0);

    // Dropout length distribution: mostly short (5-10), sometimes longer
    auto sample_dropout_length = [&](int32_t max_width) -> int32_t {
        double r = uniform01(rng);
        if (r < 0.90) {
            std::uniform_int_distribution<int32_t> short_len(5, 10);
            return std::min(short_len(rng), max_width);
        }
        if (r < 0.98) {
            std::uniform_int_distribution<int32_t> mid_len(11, 25);
            return std::min(mid_len(rng), max_width);
        }
        int32_t max_long = std::max<int32_t>(26, std::min(max_width, 120));
        std::uniform_int_distribution<int32_t> long_len(26, max_long);
        return std::min(long_len(rng), max_width);
    };

    // Model dropouts as rapid amplitude excursions with noisy edges
    std::normal_distribution<double> body_noise(0.0, 800.0);
    std::normal_distribution<double> edge_noise(0.0, 1600.0);

    auto clamp_sample = [](double value) -> uint16_t {
        value = std::max(0.0, std::min(65535.0, value));
        return static_cast<uint16_t>(value + 0.5);
    };

    auto apply_dropout = [&](int32_t line, int32_t start, int32_t length,
                             double base_excursion, double amplitude, 
                             int32_t edge_len) {
        if (line < 0 || line >= height || start < 0 || start + length > width) return;

        // Track the dropout location
        last_field_dropouts_.emplace_back(line, start, start + length);
        
        // For YC mode: if this is Y field, cache the dropout parameters
        if (context.field_type == FieldType::Y) {
            CachedDropout cached;
            cached.line = line;
            cached.start = start;
            cached.length = length;
            cached.base_excursion = base_excursion;
            cached.amplitude = amplitude;
            cached.edge_len = edge_len;
            cached_y_dropouts_.push_back(cached);
        }

        std::normal_distribution<double> modulation_step(0.0, amplitude * 0.08);
        double modulation = 0.0;
        double modulation_alpha = 0.2;
        double sign = base_excursion >= 0.0 ? 1.0 : -1.0;  // Preserve sign throughout

        for (int32_t i = 0; i < length; ++i) {
            int32_t idx = line * width + start + i;
            double original = static_cast<double>(data[idx]);

            bool is_edge = (i < edge_len) || (i >= (length - edge_len));
            double noise = is_edge ? edge_noise(rng) : body_noise(rng);

            // Update modulation but keep it same-signed as base_excursion
            double raw_modulation = (1.0 - modulation_alpha) * modulation + modulation_alpha * modulation_step(rng);
            modulation = sign * std::abs(raw_modulation);  // Constrain to same sign as base_excursion
            double excursion = base_excursion + modulation;

            double value = 0.0;
            if (i < edge_len) {
                double blend = static_cast<double>(i + 1) / static_cast<double>(edge_len);
                value = original + excursion * blend + noise;
            } else if (i >= (length - edge_len)) {
                double blend = static_cast<double>(length - i) / static_cast<double>(edge_len);
                value = original + excursion * blend + noise;
            } else {
                value = original + excursion + noise;
            }

            data[idx] = clamp_sample(value);
        }
    };

    // Generate new single-field and multi-field dropouts if this field hasn't been processed
    if (context.field_number > last_processed_field_) {
        last_processed_field_ = context.field_number;

        // Track occupied regions per line to prevent overlaps
        // Each entry is a pair of (start, end) positions
        std::vector<std::vector<std::pair<int32_t, int32_t>>> occupied_regions(height);
        
        // First, mark regions occupied by active multi-field dropouts
        for (auto& mfd : multi_field_dropouts_) {
            int32_t field_offset = context.field_number - mfd.start_field;
            if (field_offset < 0 || field_offset >= mfd.duration_fields) continue;

            double progress = static_cast<double>(field_offset) / static_cast<double>(mfd.duration_fields);
            double growth_factor = 1.0 + 0.5 * std::sin(M_PI * progress);
            int32_t current_length = static_cast<int32_t>(mfd.initial_length * growth_factor);
            
            int32_t left_extent = current_length / 2;
            int32_t start = std::max(0, mfd.center_x - left_extent);
            int32_t end = std::min(width, start + current_length);
            
            if (mfd.line_number >= 0 && mfd.line_number < height && end > start) {
                occupied_regions[mfd.line_number].emplace_back(start, end);
            }
        }

        // Lambda to check if a region overlaps with any occupied regions
        auto has_overlap = [](const std::vector<std::pair<int32_t, int32_t>>& regions, 
                             int32_t start, int32_t end) -> bool {
            for (const auto& region : regions) {
                // Check for overlap: [start, end) overlaps with [region.first, region.second)
                if (start < region.second && end > region.first) {
                    return true;
                }
            }
            return false;
        };

        // Interpret density as expected fraction of samples affected per line
        constexpr double average_dropout_length = 8.0;

        for (int32_t line = 0; line < height; ++line) {
            // Calculate available space (non-occupied)
            int32_t occupied_samples = 0;
            for (const auto& region : occupied_regions[line]) {
                occupied_samples += region.second - region.first;
            }
            int32_t available_samples = std::max(0, width - occupied_samples);
            
            if (available_samples <= 0) continue;
            
            double adjusted_density = (density_ * static_cast<double>(available_samples)) / static_cast<double>(width);
            double adjusted_events = (adjusted_density * static_cast<double>(width)) / average_dropout_length;
            
            std::poisson_distribution<int> event_count(std::max(0.0, adjusted_events));
            int events = event_count(rng);
            if (events <= 0) continue;

            for (int e = 0; e < events; ++e) {
                // Decide if this dropout should span multiple fields
                bool is_multi_field = uniform01(rng) < (multi_field_prob_ / (multi_field_prob_ + single_field_prob_));

                if (is_multi_field && multi_field_prob_ > 0.0) {
                    int32_t initial_length = sample_dropout_length(width);
                    if (initial_length <= 0) continue;

                    // Try to find a non-overlapping position (max 10 attempts)
                    bool found_position = false;
                    int32_t center_x = 0;
                    for (int attempt = 0; attempt < 10; ++attempt) {
                        center_x = std::uniform_int_distribution<int32_t>(initial_length / 2, width - 1 - initial_length / 2)(rng);
                        int32_t left_extent = initial_length / 2;
                        int32_t test_start = std::max(0, center_x - left_extent);
                        int32_t test_end = std::min(width, test_start + initial_length);
                        
                        if (!has_overlap(occupied_regions[line], test_start, test_end)) {
                            found_position = true;
                            break;
                        }
                    }
                    
                    if (!found_position) continue;  // Skip if couldn't find non-overlapping position

                    std::uniform_int_distribution<int32_t> duration_dist(5, 12);
                    int32_t duration = duration_dist(rng);

                    MultiFieldDropout mfd;
                    mfd.start_field = context.field_number;
                    mfd.duration_fields = duration;
                    mfd.line_number = line;
                    mfd.center_x = center_x;
                    mfd.initial_length = initial_length;
                    
                    double sign = (uniform01(rng) < 0.5) ? -1.0 : 1.0;
                    mfd.amplitude = (0.05 + 0.95 * uniform01(rng)) * 32768.0;
                    mfd.base_excursion = sign * mfd.amplitude;

                    multi_field_dropouts_.push_back(mfd);
                    
                    // Mark this region as occupied
                    int32_t left_extent = initial_length / 2;
                    int32_t start = std::max(0, center_x - left_extent);
                    int32_t end = std::min(width, start + initial_length);
                    occupied_regions[line].emplace_back(start, end);
                    
                } else if (!is_multi_field && single_field_prob_ > 0.0) {
                    // Create single-field dropout immediately
                    int32_t length = sample_dropout_length(width);
                    if (length <= 0) continue;

                    // Try to find a non-overlapping position (max 10 attempts)
                    bool found_position = false;
                    int32_t start = 0;
                    for (int attempt = 0; attempt < 10; ++attempt) {
                        start = std::uniform_int_distribution<int32_t>(0, width - 1)(rng);
                        int32_t end = std::min(width, start + length);
                        length = end - start;
                        
                        if (length > 0 && !has_overlap(occupied_regions[line], start, end)) {
                            found_position = true;
                            break;
                        }
                    }
                    
                    if (!found_position || length <= 0) continue;  // Skip if couldn't find non-overlapping position

                    double sign = (uniform01(rng) < 0.5) ? -1.0 : 1.0;
                    double amplitude = (0.05 + 0.95 * uniform01(rng)) * 32768.0;
                    double base_excursion = sign * amplitude;

                    int32_t min_edge = (length >= 40) ? 3 : 1;
                    int32_t max_edge = (length >= 80) ? 12 : (length >= 40 ? 8 : 4);
                    std::uniform_int_distribution<int32_t> edge_len_dist(min_edge, max_edge);
                    int32_t edge_len = std::min<int32_t>(length / 2, edge_len_dist(rng));
                    if (edge_len < 1) edge_len = 1;

                    apply_dropout(line, start, length, base_excursion, amplitude, edge_len);
                    
                    // Mark this region as occupied
                    occupied_regions[line].emplace_back(start, start + length);
                }
            }
        }
    }

    // Apply multi-field dropouts that are active for this field
    for (auto& mfd : multi_field_dropouts_) {
        int32_t field_offset = context.field_number - mfd.start_field;
        if (field_offset < 0 || field_offset >= mfd.duration_fields) continue;

        // Growth/shrinkage profile: peak at mid-point, shrinks towards edges
        double progress = static_cast<double>(field_offset) / static_cast<double>(mfd.duration_fields);
        double growth_factor = 1.0 + 0.5 * std::sin(M_PI * progress);  // Peaks at 1.5x at mid-point
        int32_t current_length = static_cast<int32_t>(mfd.initial_length * growth_factor);

        // Calculate spread left and right from center
        int32_t left_extent = current_length / 2;

        int32_t start = mfd.center_x - left_extent;
        if (start < 0) {
            start = 0;
        }
        if (start + current_length > width) {
            current_length = width - start;
        }
        if (current_length <= 0) continue;

        // Use the stored excursion characteristics (generated when dropout was created)
        int32_t min_edge = (current_length >= 40) ? 3 : 1;
        int32_t max_edge = (current_length >= 80) ? 12 : (current_length >= 40 ? 8 : 4);
        int32_t edge_len = std::min<int32_t>(current_length / 2,
                                              std::uniform_int_distribution<int32_t>(min_edge, max_edge)(rng));
        if (edge_len < 1) edge_len = 1;

        apply_dropout(mfd.line_number, start, current_length, mfd.base_excursion, mfd.amplitude, edge_len);
    }

    // Remove expired multi-field dropouts
    multi_field_dropouts_.erase(
        std::remove_if(multi_field_dropouts_.begin(), multi_field_dropouts_.end(),
                      [context](const MultiFieldDropout& mfd) {
                          return context.field_number >= mfd.start_field + mfd.duration_fields;
                      }),
        multi_field_dropouts_.end()
    );

    // Store dropouts for this field (already protected by method-level mutex)
    field_dropouts_[context.field_number] = last_field_dropouts_;
}

void DropoutSimulator::apply_cached_dropouts(Field& field, const FieldEffectContext& context) {
    // Apply the same dropouts that were applied to the Y field to the C field
    // Note: We don't track metadata here since it's already been recorded for the Y field
    // (same dropout affects both Y and C, so only one metadata record is needed)
    auto& data = field.data();
    int32_t width = field.width();
    int32_t height = field.height();

    if (width <= 0 || height <= 0) return;

    // Random generator - use same seed as Y field for consistency
    std::mt19937 rng(seed_ + context.field_number);
    std::normal_distribution<double> body_noise(0.0, 800.0);
    std::normal_distribution<double> edge_noise(0.0, 1600.0);

    auto clamp_sample = [](double value) -> uint16_t {
        value = std::max(0.0, std::min(65535.0, value));
        return static_cast<uint16_t>(value + 0.5);
    };

    // Apply each cached dropout
    for (const auto& cached : cached_y_dropouts_) {
        int32_t line = cached.line;
        int32_t start = cached.start;
        int32_t length = cached.length;
        double base_excursion = cached.base_excursion;
        double amplitude = cached.amplitude;
        int32_t edge_len = cached.edge_len;

        if (line < 0 || line >= height || start < 0 || start + length > width) continue;

        // Do NOT track dropout location here - already tracked in Y field processing
        // This ensures one metadata record per dropout (not duplicated for Y and C)

        std::normal_distribution<double> modulation_step(0.0, amplitude * 0.08);
        double modulation = 0.0;
        double modulation_alpha = 0.2;
        double sign = base_excursion >= 0.0 ? 1.0 : -1.0;

        for (int32_t i = 0; i < length; ++i) {
            int32_t idx = line * width + start + i;
            double original = static_cast<double>(data[idx]);

            bool is_edge = (i < edge_len) || (i >= (length - edge_len));
            double noise = is_edge ? edge_noise(rng) : body_noise(rng);

            // Update modulation but keep it same-signed as base_excursion
            double raw_modulation = (1.0 - modulation_alpha) * modulation + modulation_alpha * modulation_step(rng);
            modulation = sign * std::abs(raw_modulation);
            double excursion = base_excursion + modulation;

            double value = 0.0;
            if (i < edge_len) {
                double blend = static_cast<double>(i + 1) / static_cast<double>(edge_len);
                value = original + excursion * blend + noise;
            } else if (i >= (length - edge_len)) {
                double blend = static_cast<double>(length - i) / static_cast<double>(edge_len);
                value = original + excursion * blend + noise;
            } else {
                value = original + excursion + noise;
            }

            data[idx] = clamp_sample(value);
        }
    }

    // Do NOT store dropouts for C field - metadata already stored from Y field
    // This ensures consistent metadata with composite sources (one record per dropout)
}


// ============================================================================
// PhaseErrorSimulator Implementation
// ============================================================================

PhaseErrorSimulator::PhaseErrorSimulator(double phase_jitter_samples,
                                         double frequency_hz,
                                         uint32_t seed)
    : phase_jitter_samples_(phase_jitter_samples),
      frequency_hz_(frequency_hz),
      seed_(seed) {}

void PhaseErrorSimulator::set_phase_jitter(double phase_jitter_samples) {
    phase_jitter_samples_ = phase_jitter_samples;
}

void PhaseErrorSimulator::set_frequency(double frequency_hz) {
    frequency_hz_ = frequency_hz;
}

void PhaseErrorSimulator::apply(Field& field, const FieldEffectContext& context) {
    if (!enabled_) return;
    
    auto& data = field.data();
    int32_t width = field.width();
    int32_t height = field.height();
    
    // Create a seeded RNG for reproducible phase errors
    std::mt19937 rng(seed_ + context.field_number);
    std::normal_distribution<double> gaussian(0.0, phase_jitter_samples_);
    
    // Apply phase jitter to each line
    for (int32_t line = 0; line < height; ++line) {
        // Calculate phase modulation for this line
        double phase_mod = std::sin(2.0 * M_PI * frequency_hz_ * context.field_number +
                                    2.0 * M_PI * line / height);
        
        // Add random jitter
        double jitter = gaussian(rng);
        double total_shift = phase_mod * phase_jitter_samples_ + jitter;
        
        // Clamp shift to valid range
        int32_t pixel_shift = static_cast<int32_t>(total_shift);
        pixel_shift = std::max(-width + 1, std::min(width - 1, pixel_shift));
        
        if (pixel_shift == 0) continue;  // No shift needed
        
        // Apply horizontal shift to this line
        std::vector<uint16_t> line_data(data.begin() + line * width,
                                         data.begin() + (line + 1) * width);
        
        if (pixel_shift > 0) {
            // Shift right: move data right, fill left with blanking
            std::fill(data.begin() + line * width,
                     data.begin() + line * width + pixel_shift,
                     4096);  // Blanking level
            std::copy(line_data.begin(),
                     line_data.end() - pixel_shift,
                     data.begin() + line * width + pixel_shift);
        } else {
            // Shift left: move data left, fill right with blanking
            std::copy(line_data.begin() - pixel_shift,
                     line_data.end(),
                     data.begin() + line * width);
            std::fill(data.begin() + line * width + (width + pixel_shift),
                     data.begin() + (line + 1) * width,
                     4096);  // Blanking level
        }
    }
}

} // namespace encode_orc
