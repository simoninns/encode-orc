/*
 * File:        pal_encoder.cpp
 * Module:      encode-orc
 * Purpose:     PAL video signal encoder implementation
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: 2026 Simon Inns
 */

#include "pal_encoder.h"
#include "color_burst_generator.h"
#include "pal_vits_generator.h"
#include "biphase_encoder.h"
#include <cstring>
#include <algorithm>

namespace encode_orc {

PALEncoder::PALEncoder(const VideoParameters& params, 
                       bool enable_chroma_filter,
                       bool enable_luma_filter) 
    : params_(params),
      vits_enabled_(false) {
    
    // Set signal levels
    sync_level_ = 0x0000;  // Sync tip at 0 IRE (0V)
    blanking_level_ = params_.blanking_16b_ire;
    black_level_ = params_.black_16b_ire;
    white_level_ = params_.white_16b_ire;
    
    // Set subcarrier parameters
    subcarrier_freq_ = params_.fSC;
    sample_rate_ = params_.sample_rate;
    samples_per_cycle_ = sample_rate_ / subcarrier_freq_;
    
    // Initialize filters if requested
    if (enable_chroma_filter) {
        chroma_filter_ = Filters::create_pal_uv_filter();
    }
    if (enable_luma_filter) {
        luma_filter_ = Filters::create_pal_uv_filter();  // Reuse same filter for luma
    }
}

void PALEncoder::enable_vits() {
    if (!vits_generator_) {
        vits_generator_ = std::make_unique<PALVITSGenerator>(params_);
    }
    vits_enabled_ = true;
}

void PALEncoder::disable_vits() {
    vits_enabled_ = false;
}

bool PALEncoder::is_vits_enabled() const {
    return vits_enabled_ && vits_generator_ != nullptr;
}

Frame PALEncoder::encode_frame(const FrameBuffer& frame_buffer, int32_t field_number, int32_t frame_number_for_vbi) {
    Frame frame(params_.field_width, params_.field_height);
    
    // Encode first field (even lines: 0, 2, 4, ...)
    frame.field1() = encode_field(frame_buffer, field_number, true, frame_number_for_vbi);
    
    // Encode second field (odd lines: 1, 3, 5, ...)
    frame.field2() = encode_field(frame_buffer, field_number + 1, false, frame_number_for_vbi);
    
    return frame;
}

Field PALEncoder::encode_field(const FrameBuffer& frame_buffer, 
                               int32_t field_number, 
                               bool is_first_field,
                               int32_t frame_number_for_vbi) {
    Field field(params_.field_width, params_.field_height);
    
    // Verify input format
    if (frame_buffer.format() != FrameBuffer::Format::YUV444P16) {
        // For now, just create a blanking field if wrong format
        field.fill(static_cast<uint16_t>(blanking_level_));
        return field;
    }
    
    // Get frame dimensions
    int32_t frame_width = frame_buffer.width();
    int32_t frame_height = frame_buffer.height();
    
    // PAL has 313 lines per field
    for (int32_t line = 0; line < LINES_PER_FIELD; ++line) {
        uint16_t* line_buffer = field.line_data(line);
        
        // Lines 1-5: Vertical sync (field sync)
        if (line < VSYNC_LINES) {
            generate_vsync_line(line_buffer, line);
        }
        // Lines 6-22: VBI (Vertical Blanking Interval)
        else if (line < ACTIVE_LINES_START) {
            // Lines 15, 16, 17 (0-indexed) = field lines 16, 17, 18 contain biphase frame numbers
            if (frame_number_for_vbi >= 0 && (line == 15 || line == 16 || line == 17)) {
                generate_biphase_vbi_line(line_buffer, line, field_number, frame_number_for_vbi);
            }
            // VITS lines (if enabled)
            else if (is_vits_enabled()) {
                // First field VITS lines (0-indexed in field)
                if (is_first_field && line == 18) {  // Line 332 in PAL frame (odd-first field parity)
                    vits_generator_->generate_uk_national_line332(line_buffer, field_number);
                }
                else if (is_first_field && line == 19) {  // Line 333 in PAL frame
                    vits_generator_->generate_multiburst_line333(line_buffer, field_number);
                }
                // Second field VITS lines (0-indexed in field)
                else if (!is_first_field && line == 18) {  // Line 19 in PAL frame
                    vits_generator_->generate_itu_composite_line19(line_buffer, field_number);
                }
                else if (!is_first_field && line == 19) {  // Line 20 in PAL frame
                    vits_generator_->generate_itu_its_line20(line_buffer, field_number);
                }
                else {
                    generate_blanking_line(line_buffer);
                    generate_sync_pulse(line_buffer, line);
                    generate_color_burst(line_buffer, line, field_number);
                }
            }
            else {
                generate_blanking_line(line_buffer);
                generate_sync_pulse(line_buffer, line);
                generate_color_burst(line_buffer, line, field_number);
            }
        }
        // Lines 23-310: Active video
        else if (line < ACTIVE_LINES_END) {
            // Calculate which line in the source frame to use
            int32_t line_in_field = line - ACTIVE_LINES_START;
            int32_t line_in_frame = is_first_field ? (line_in_field * 2) : (line_in_field * 2 + 1);
            
            // Initialize line with blanking level
            generate_blanking_line(line_buffer);
            
            // Check if line is within source frame
            if (line_in_frame < frame_height) {
                // Get pointers to YUV data
                const uint16_t* frame_data = frame_buffer.data().data();
                int32_t pixel_count = frame_width * frame_height;
                const uint16_t* y_plane = frame_data;
                const uint16_t* u_plane = frame_data + pixel_count;
                const uint16_t* v_plane = frame_data + pixel_count * 2;
                
                const uint16_t* y_line = y_plane + (line_in_frame * frame_width);
                const uint16_t* u_line = u_plane + (line_in_frame * frame_width);
                const uint16_t* v_line = v_plane + (line_in_frame * frame_width);
                
                // Generate horizontal sync and color burst
                generate_sync_pulse(line_buffer, line);
                generate_color_burst(line_buffer, line, field_number);
                
                // Encode active video portion
                encode_active_line(line_buffer, y_line, u_line, v_line, 
                                 line, field_number, frame_width);
            } else {
                // Beyond source frame - use blanking with sync and burst
                generate_sync_pulse(line_buffer, line);
                generate_color_burst(line_buffer, line, field_number);
            }
        }
        // Lines 311-313: Post-video blanking
        else {
            generate_blanking_line(line_buffer);
            generate_sync_pulse(line_buffer, line);
            generate_color_burst(line_buffer, line, field_number);
        }
    }
    
    return field;
}

void PALEncoder::generate_sync_pulse(uint16_t* line_buffer, int32_t /* line_number */) {
    // PAL horizontal sync pulse
    // Duration: 4.7 µs (approximately 83 samples at 17.7 MHz)
    // Front porch: 1.65 µs (approximately 29 samples)
    // Back porch starts after color burst
    
    double sample_duration = 1.0 / sample_rate_;  // seconds per sample
    int32_t sync_duration = static_cast<int32_t>(4.7e-6 / sample_duration);  // 4.7 µs
    
    // Sync pulse at beginning of line
    for (int32_t i = 0; i < sync_duration; ++i) {
        line_buffer[i] = static_cast<uint16_t>(sync_level_);
    }
}

void PALEncoder::generate_color_burst(uint16_t* line_buffer, int32_t line_number, int32_t field_number) {
    // Delegate to shared color burst generator
    ColorBurstGenerator burst_gen(params_);
    burst_gen.generate_pal_burst(line_buffer, line_number, field_number);
}

void PALEncoder::generate_color_burst_chroma(uint16_t* line_buffer, int32_t line_number, int32_t field_number) {
    // Generate color burst on chroma channel (centered at 32768)
    // The burst is a reference signal for the color subcarrier
    // For chroma-only output, we generate the modulated burst centered at 16-bit midpoint
    
    std::fill_n(line_buffer, params_.field_width, static_cast<uint16_t>(32768));
    
    // Generate burst signal modulated at the PAL color subcarrier frequency
    // Use the proper burst window from video parameters
    const int32_t burst_start = params_.colour_burst_start;
    const int32_t burst_end = params_.colour_burst_end;
    
    // Calculate burst amplitude: 3/14 of luma range (same as composite mode)
    int32_t luma_range = white_level_ - blanking_level_;
    int32_t burst_amplitude = static_cast<int32_t>((3.0 / 14.0) * luma_range);
    
    // Calculate field-specific phase for PAL
    bool is_first_field = (field_number % 2) == 0;
    int32_t frame_line = is_first_field ? (line_number * 2 + 1) : (line_number * 2 + 2);
    int32_t field_id = field_number % 8;
    int32_t prev_lines = ((field_id / 2) * 625) + ((field_id % 2) * 313) + (frame_line / 2);
    int32_t v_switch = (prev_lines % 2 == 0) ? 1 : -1;
    double burst_phase_offset = v_switch * (135.0 * PI / 180.0);
    double prev_cycles = prev_lines * 283.7516;
    
    for (int32_t sample = burst_start; sample < burst_end; ++sample) {
        if (sample >= 0 && sample < params_.field_width) {
            // Standard PAL burst phase with V-switch
            double t = static_cast<double>(sample) / sample_rate_;
            double phase = 2.0 * PI * (subcarrier_freq_ * t + prev_cycles) + burst_phase_offset;
            
            // PAL burst uses sine with V-switch modulation
            double burst_signal = std::sin(phase);
            
            int32_t sample_value = 32768 + static_cast<int32_t>(burst_amplitude * burst_signal);
            line_buffer[sample] = clamp_to_16bit(sample_value);
        }
    }
}

void PALEncoder::generate_color_burst_chroma_line(uint16_t* line_buffer, int32_t line_number, 
                                                   int32_t field_number, int32_t burst_end) {
    // Generate color burst on chroma for the portion before active video
    // Chroma should be centered at 32768 with NO sync pulse
    // Use the proper burst window from video parameters
    
    // First, fill entire line with blanking level (32768 - centered, no sync)
    std::fill_n(line_buffer, params_.field_width, static_cast<uint16_t>(32768));
    
    // Color burst position from video parameters
    const int32_t burst_start = params_.colour_burst_start;
    
    // Calculate burst amplitude: 3/14 of luma range (same as composite mode)
    int32_t luma_range = white_level_ - blanking_level_;
    int32_t burst_amplitude = static_cast<int32_t>((3.0 / 14.0) * luma_range);
    
    // Calculate field-specific phase for PAL
    bool is_first_field = (field_number % 2) == 0;
    int32_t frame_line = is_first_field ? (line_number * 2 + 1) : (line_number * 2 + 2);
    int32_t field_id = field_number % 8;
    int32_t prev_lines = ((field_id / 2) * 625) + ((field_id % 2) * 313) + (frame_line / 2);
    int32_t v_switch = (prev_lines % 2 == 0) ? 1 : -1;
    double burst_phase_offset = v_switch * (135.0 * PI / 180.0);
    double prev_cycles = prev_lines * 283.7516;
    
    // Generate color burst using the proper window
    const int32_t actual_burst_end = std::min(burst_end, params_.colour_burst_end);
    
    for (int32_t sample = burst_start; sample < actual_burst_end; ++sample) {
        if (sample < params_.field_width) {
            // Generate burst during back porch period
            double t = static_cast<double>(sample) / sample_rate_;
            double phase = 2.0 * PI * (subcarrier_freq_ * t + prev_cycles) + burst_phase_offset;
            
            // PAL burst reference signal with V-switch modulation
            double burst_signal = std::sin(phase);
            
            int32_t sample_value = 32768 + static_cast<int32_t>(burst_amplitude * burst_signal);
            line_buffer[sample] = clamp_to_16bit(sample_value);
        }
    }
}

void PALEncoder::generate_vsync_line(uint16_t* line_buffer, int32_t line_number) {
    // PAL vertical sync pattern
    // Lines 1-2.5: broad pulses (inverted)
    // Lines 2.5-5: narrow sync pulses
    
    // For simplicity, fill with sync level during vsync
    std::fill_n(line_buffer, params_.field_width, static_cast<uint16_t>(sync_level_));
    
    // Add equalizing pulses (simplified pattern)
    // In a full implementation, this would have the proper broad/narrow pulse pattern
    int32_t half_line = params_.field_width / 2;
    
    if (line_number < 3) {
        // Broad pulses (mostly sync level with short blanking pulses)
        for (int32_t i = 0; i < params_.field_width; i += half_line) {
            for (int32_t j = 0; j < 50 && (i + j) < params_.field_width; ++j) {
                line_buffer[i + j] = static_cast<uint16_t>(blanking_level_);
            }
        }
    } else {
        // Narrow pulses (mostly blanking with short sync pulses)
        std::fill_n(line_buffer, params_.field_width, static_cast<uint16_t>(blanking_level_));
        for (int32_t i = 0; i < params_.field_width; i += half_line) {
            for (int32_t j = 0; j < 50 && (i + j) < params_.field_width; ++j) {
                line_buffer[i + j] = static_cast<uint16_t>(sync_level_);
            }
        }
    }
}

void PALEncoder::generate_blanking_line(uint16_t* line_buffer) {
    // Fill line with blanking level
    std::fill_n(line_buffer, params_.field_width, static_cast<uint16_t>(blanking_level_));
}

void PALEncoder::encode_active_line(uint16_t* line_buffer,
                                   const uint16_t* y_line,
                                   const uint16_t* u_line,
                                   const uint16_t* v_line,
                                   int32_t line_number,
                                   int32_t field_number,
                                   int32_t width) {
    // Apply filters if enabled
    // Note: We need to copy the line data first to apply filtering
    std::vector<uint16_t> y_filtered(y_line, y_line + width);
    std::vector<uint16_t> u_filtered(u_line, u_line + width);
    std::vector<uint16_t> v_filtered(v_line, v_line + width);
    
    // Apply filters if configured
    if (luma_filter_) {
        luma_filter_->apply(y_filtered);
    }
    if (chroma_filter_) {
        chroma_filter_->apply(u_filtered);
        chroma_filter_->apply(v_filtered);
    }
    
    // Use filtered data for encoding
    const uint16_t* y_data = y_filtered.data();
    const uint16_t* u_data = u_filtered.data();
    const uint16_t* v_data = v_filtered.data();
    
    // Encode the active video portion of the line
    int32_t active_start = params_.active_video_start;
    int32_t active_end = params_.active_video_end;
    int32_t active_width = active_end - active_start;
    
    // Calculate absolute line number in PAL 8-field sequence
    // Following ld-chroma-encoder's calculation [palencoder.cpp:186-188]
    // First, convert field line number to frame line number (1-625)
    bool is_first_field = (field_number % 2) == 0;
    int32_t frame_line = is_first_field ? (line_number * 2 + 1) : (line_number * 2 + 2);
    
    // Now calculate prevLines using frame_line
    int32_t field_id = field_number % 8;
    int32_t prev_lines = ((field_id / 2) * 625) + ((field_id % 2) * 313) + (frame_line / 2);
    
    // Get V-switch for PAL alternation (based on absolute line number)
    // PAL V-switch alternates every line
    int32_t v_switch = (prev_lines % 2 == 0) ? 1 : -1;
    
    // PAL subcarrier phase calculation (following ld-chroma-encoder):
    // prevCycles = number of complete cycles of subcarrier since sequence start
    // phase = 2π * (fSC * t + prevCycles)
    // where prevCycles accumulates by 283.7516 per line
    double prev_cycles = prev_lines * 283.7516;  // Cycles of phase advance
    
    for (int32_t sample = active_start; sample < active_end; ++sample) {
        // Map sample position to source pixel
        double pixel_pos = static_cast<double>(sample - active_start) * width / active_width;
        int32_t pixel_x = static_cast<int32_t>(pixel_pos);
        
        // Clamp to valid range
        if (pixel_x >= width) pixel_x = width - 1;
        
        // Get YUV values from filtered source
        uint16_t y = y_data[pixel_x];
        uint16_t u = u_data[pixel_x];
        uint16_t v = v_data[pixel_x];
        
        // Calculate subcarrier phase for this sample position
        // phase = 2π * (fSC * t + prevCycles)
        double t = static_cast<double>(sample) / sample_rate_;
        double phase = 2.0 * PI * (subcarrier_freq_ * t + prev_cycles);
        
        // Convert YUV to composite signal
        uint16_t composite = yuv_to_composite(y, u, v, phase, v_switch);
        line_buffer[sample] = composite;
    }
}

uint16_t PALEncoder::yuv_to_composite(uint16_t y, uint16_t u, uint16_t v,
                                      double phase, int32_t v_switch) {
    // Convert 16-bit YUV to normalized values
    // Y: 0-65535 → 0.0-1.0
    // U, V: 0-65535 → ±0.436/±0.615 (scaled by actual max values)
    double y_norm = static_cast<double>(y) / 65535.0;
    
    // Scale U/V back to their actual ranges
    // Max values for PAL: U_MAX=0.436010, V_MAX=0.614975
    const double U_MAX = 0.436010;
    const double V_MAX = 0.614975;
    double u_norm = ((static_cast<double>(u) / 65535.0) - 0.5) * 2.0 * U_MAX;  // ±U_MAX range
    double v_norm = ((static_cast<double>(v) / 65535.0) - 0.5) * 2.0 * V_MAX;  // ±V_MAX range
    
    // Scale luma to video range
    // Map 0.0-1.0 to black_level-white_level
    int32_t luma_range = white_level_ - black_level_;
    int32_t luma_scaled = black_level_ + 
                         static_cast<int32_t>(y_norm * luma_range);
    
    // PAL chroma encoding - following ld-chroma-encoder exactly
    // Composite = Y + U*sin(ωt) + V*cos(ωt)*Vsw  (from Poynton p338)
    // The V-switch alternates the V component sign every line
    // 
    // U and V are in their natural ±U_MAX/±V_MAX ranges
    // They modulate the chroma signal directly as per ld-chroma-encoder
    
    // Calculate chroma modulation (using U/V directly as per ld-chroma-encoder)
    double chroma = (u_norm * std::sin(phase)) + (v_norm * v_switch * std::cos(phase));
    
    // Scale chroma to signal amplitude (matching ld-chroma-encoder)
    // The chroma modulates directly, scaled by luma_range
    int32_t chroma_scaled = static_cast<int32_t>(chroma * luma_range);
    
    // Combine luma and chroma
    int32_t composite = luma_scaled + chroma_scaled;
    
    return clamp_to_16bit(composite);
}

void PALEncoder::generate_biphase_vbi_line(uint16_t* line_buffer, int32_t line_number, 
                                           int32_t field_number, int32_t frame_number) {
    // Start with a standard blanking line with sync and color burst
    generate_blanking_line(line_buffer);
    generate_sync_pulse(line_buffer, line_number);
    generate_color_burst(line_buffer, line_number, field_number);
    
    // Calculate line period (H) for PAL: 64 µs
    double line_period_h = 64.0e-6;  // 64 µs for PAL
    
    // Get biphase signal position
    int32_t biphase_start = BiphaseEncoder::get_signal_start_position(sample_rate_, line_period_h);
    
    // Encode frame number as LaserDisc CAV picture number (3 bytes in BCD format)
    uint8_t vbi_byte0, vbi_byte1, vbi_byte2;
    BiphaseEncoder::encode_cav_picture_number(frame_number, vbi_byte0, vbi_byte1, vbi_byte2);
    
    // Generate biphase signal from the three bytes
    // High level: white level, Low level: black level
    std::vector<uint16_t> biphase_signal = BiphaseEncoder::encode(
        vbi_byte0, vbi_byte1, vbi_byte2,
        sample_rate_,
        static_cast<uint16_t>(white_level_),
        static_cast<uint16_t>(black_level_)
    );
    
    // Insert biphase signal into the line
    int32_t signal_end = biphase_start + static_cast<int32_t>(biphase_signal.size());
    if (signal_end > params_.field_width) {
        signal_end = params_.field_width;
    }
    
    for (int32_t i = biphase_start; i < signal_end; ++i) {
        int32_t signal_index = i - biphase_start;
        if (signal_index < static_cast<int32_t>(biphase_signal.size())) {
            line_buffer[i] = biphase_signal[signal_index];
        }
    }
}

void PALEncoder::encode_frame_yc(const FrameBuffer& frame_buffer, int32_t field_number,
                                 int32_t frame_number_for_vbi,
                                 Field& y_field1, Field& c_field1,
                                 Field& y_field2, Field& c_field2) {
    // Initialize fields
    y_field1.resize(params_.field_width, params_.field_height);
    c_field1.resize(params_.field_width, params_.field_height);
    y_field2.resize(params_.field_width, params_.field_height);
    c_field2.resize(params_.field_width, params_.field_height);
    
    // For separate Y/C output, we encode Y and C directly from source YUV data:
    // Y field: luma component with sync + blanking (no chroma modulation)
    // C field: chroma-only signal (modulated subcarrier centered at blanking level)
    //
    // This is NOT created by decomposing composite - we build Y and C separately
    // from the source YUV to avoid any quality loss.
    
    // Get frame dimensions
    int32_t frame_width = frame_buffer.width();
    int32_t frame_height = frame_buffer.height();
    
    // Get pointers to YUV planes
    const uint16_t* frame_data = frame_buffer.data().data();
    int32_t pixel_count = frame_width * frame_height;
    const uint16_t* y_plane = frame_data;
    const uint16_t* u_plane = frame_data + pixel_count;
    const uint16_t* v_plane = frame_data + pixel_count * 2;
    
    // For separate Y/C encoding, we use the source data directly
    // (filters are applied during composite encoding, but for Y/C we skip filtering
    // to avoid complexity with line-by-line processing)
    
    // Process field 1 (even lines from source)
    for (int32_t line = 0; line < params_.field_height; ++line) {
        uint16_t* y_line = y_field1.line_data(line);
        uint16_t* c_line = c_field1.line_data(line);
        
        // For sync, blanking, and VBI lines
        if (line < ACTIVE_LINES_START) {
            // Initialize Y field with blanking level
            generate_blanking_line(y_line);
            
            // Generate sync for Y field (no color burst)
            generate_sync_pulse(y_line, line);
            
            // C field gets color burst (modulated at blanking level, centered at 32768)
            generate_color_burst_chroma(c_line, line, field_number);
            
            // Handle VBI lines if enabled
            if (frame_number_for_vbi >= 0 && (line == 15 || line == 16 || line == 17)) {
                generate_biphase_vbi_line(y_line, line, field_number, frame_number_for_vbi);
            }
            // VITS lines for field 1 (if enabled)
            else if (vits_enabled_ && vits_generator_) {
                // First field VITS lines (0-indexed in field)
                if (line == 18) {  // Line 332 in PAL frame (odd-first field parity)
                    vits_generator_->generate_uk_national_line332(y_line, field_number);
                }
                else if (line == 19) {  // Line 333 in PAL frame
                    vits_generator_->generate_multiburst_line333(y_line, field_number);
                }
                else {
                    // Other VBI lines - regular blanking
                }
                
                // For C field during VITS lines, set to neutral (no chroma modulation)
                std::fill_n(c_line, params_.field_width, static_cast<uint16_t>(32768));
            }

            // Ensure no color burst appears in luma during VITS/VBI lines
            const int32_t burst_start = params_.colour_burst_start;
            const int32_t burst_end = params_.colour_burst_end;
            for (int32_t s = burst_start; s < burst_end && s < params_.field_width; ++s) {
                y_line[s] = static_cast<uint16_t>(blanking_level_);
            }
        } else if (line >= ACTIVE_LINES_END) {
            // Post-active blanking
            generate_blanking_line(y_line);
            generate_color_burst_chroma(c_line, line, field_number);
        } else {
            // Active video line - encode Y and C separately from source
            int32_t source_line = (line - ACTIVE_LINES_START) * 2;  // Even lines for field 1
            if (source_line >= frame_height) source_line = frame_height - 1;
            
            // Initialize Y field with blanking (same as composite)
            generate_blanking_line(y_line);
            
            // Generate sync for Y field (no color burst in Y)
            generate_sync_pulse(y_line, line);
            
            // C field gets color burst during sync/burst period
            generate_color_burst_chroma_line(c_line, line, field_number, params_.active_video_start);
            
            // Encode active video portion
            int32_t active_start = params_.active_video_start;
            int32_t active_end = params_.active_video_end;
            int32_t active_width = active_end - active_start;
            
            // Calculate PAL phase parameters for this line
            bool is_first_field = (field_number % 2) == 0;
            int32_t frame_line = is_first_field ? (line * 2 + 1) : (line * 2 + 2);
            int32_t field_id = field_number % 8;
            int32_t prev_lines = ((field_id / 2) * 625) + ((field_id % 2) * 313) + (frame_line / 2);
            int32_t v_switch = (prev_lines % 2 == 0) ? 1 : -1;
            double prev_cycles = prev_lines * 283.7516;
            
            for (int32_t sample = active_start; sample < active_end; ++sample) {
                // Map sample position to source pixel
                double pixel_pos = static_cast<double>(sample - active_start) * frame_width / active_width;
                int32_t pixel_x = static_cast<int32_t>(pixel_pos);
                if (pixel_x >= frame_width) pixel_x = frame_width - 1;
                
                // Get filtered YUV values from source
                uint16_t y_val = y_plane[source_line * frame_width + pixel_x];
                uint16_t u_val = u_plane[source_line * frame_width + pixel_x];
                uint16_t v_val = v_plane[source_line * frame_width + pixel_x];
                
                // Convert Y to luma signal level
                double y_norm = static_cast<double>(y_val) / 65535.0;
                int32_t luma_range = white_level_ - black_level_;
                int32_t y_signal = black_level_ + static_cast<int32_t>(y_norm * luma_range);
                
                // Convert U/V to chroma signal
                const double U_MAX = 0.436010;
                const double V_MAX = 0.614975;
                double u_norm = ((static_cast<double>(u_val) / 65535.0) - 0.5) * 2.0 * U_MAX;
                double v_norm = ((static_cast<double>(v_val) / 65535.0) - 0.5) * 2.0 * V_MAX;
                
                // Calculate subcarrier phase
                double t = static_cast<double>(sample) / sample_rate_;
                double phase = 2.0 * PI * (subcarrier_freq_ * t + prev_cycles);
                
                // Modulate chroma onto subcarrier (PAL encoding)
                double chroma = (u_norm * std::sin(phase)) + (v_norm * v_switch * std::cos(phase));
                int32_t chroma_signal = static_cast<int32_t>(chroma * luma_range);
                
                // Y field: luma only (no chroma)
                y_line[sample] = clamp_to_16bit(y_signal);
                
                // C field: chroma only (centered at 16-bit midpoint, no luma)
                c_line[sample] = clamp_to_16bit(32768 + chroma_signal);
            }
            
            // Fill remaining samples with blanking
            for (int32_t sample = active_end; sample < params_.field_width; ++sample) {
                // Y keeps its blanking level, C gets 16-bit center
                c_line[sample] = static_cast<uint16_t>(32768);
            }
        }
    }
    
    // Process field 2 (odd lines from source)
    for (int32_t line = 0; line < params_.field_height; ++line) {
        uint16_t* y_line = y_field2.line_data(line);
        uint16_t* c_line = c_field2.line_data(line);
        
        if (line < ACTIVE_LINES_START) {
            // Initialize Y field with blanking level
            generate_blanking_line(y_line);
            
            // Generate sync for Y field (no color burst)
            generate_sync_pulse(y_line, line);
            
            // C field gets color burst (centered at 32768)
            generate_color_burst_chroma(c_line, line, field_number + 1);
            
            if (frame_number_for_vbi >= 0 && (line == 15 || line == 16 || line == 17)) {
                generate_biphase_vbi_line(y_line, line, field_number + 1, frame_number_for_vbi);
            }
            // VITS lines for field 2 (if enabled)
            else if (vits_enabled_ && vits_generator_) {
                // Second field VITS lines (0-indexed in field)
                if (line == 18) {  // Line 19 in PAL frame
                    vits_generator_->generate_itu_composite_line19(y_line, field_number + 1);
                }
                else if (line == 19) {  // Line 20 in PAL frame
                    vits_generator_->generate_itu_its_line20(y_line, field_number + 1);
                }
                else {
                    // Other VBI lines - regular blanking
                }
                
                // For C field during VITS lines, set to neutral (no chroma modulation)
                std::fill_n(c_line, params_.field_width, static_cast<uint16_t>(32768));
            }

            // Ensure no color burst appears in luma during VITS/VBI lines
            const int32_t burst_start = params_.colour_burst_start;
            const int32_t burst_end = params_.colour_burst_end;
            for (int32_t s = burst_start; s < burst_end && s < params_.field_width; ++s) {
                y_line[s] = static_cast<uint16_t>(blanking_level_);
            }
        } else if (line >= ACTIVE_LINES_END) {
            generate_blanking_line(y_line);
            generate_color_burst_chroma(c_line, line, field_number + 1);
        } else {
            int32_t source_line = (line - ACTIVE_LINES_START) * 2 + 1;  // Odd lines for field 2
            if (source_line >= frame_height) source_line = frame_height - 1;
            
            // Initialize Y field with blanking (same as composite)
            generate_blanking_line(y_line);
            
            // Generate sync for Y field (no color burst in Y)
            generate_sync_pulse(y_line, line);
            
            // C field gets color burst during sync/burst period
            generate_color_burst_chroma_line(c_line, line, field_number + 1, params_.active_video_start);
            
            int32_t active_start = params_.active_video_start;
            int32_t active_end = params_.active_video_end;
            int32_t active_width = active_end - active_start;
            
            bool is_first_field = ((field_number + 1) % 2) == 0;
            int32_t frame_line = is_first_field ? (line * 2 + 1) : (line * 2 + 2);
            int32_t field_id = (field_number + 1) % 8;
            int32_t prev_lines = ((field_id / 2) * 625) + ((field_id % 2) * 313) + (frame_line / 2);
            int32_t v_switch = (prev_lines % 2 == 0) ? 1 : -1;
            double prev_cycles = prev_lines * 283.7516;
            
            for (int32_t sample = active_start; sample < active_end; ++sample) {
                double pixel_pos = static_cast<double>(sample - active_start) * frame_width / active_width;
                int32_t pixel_x = static_cast<int32_t>(pixel_pos);
                if (pixel_x >= frame_width) pixel_x = frame_width - 1;
                
uint16_t y_val = y_plane[source_line * frame_width + pixel_x];
                    uint16_t u_val = u_plane[source_line * frame_width + pixel_x];
                    uint16_t v_val = v_plane[source_line * frame_width + pixel_x];
                
                double y_norm = static_cast<double>(y_val) / 65535.0;
                int32_t luma_range = white_level_ - black_level_;
                int32_t y_signal = black_level_ + static_cast<int32_t>(y_norm * luma_range);
                
                const double U_MAX = 0.436010;
                const double V_MAX = 0.614975;
                double u_norm = ((static_cast<double>(u_val) / 65535.0) - 0.5) * 2.0 * U_MAX;
                double v_norm = ((static_cast<double>(v_val) / 65535.0) - 0.5) * 2.0 * V_MAX;
                
                double t = static_cast<double>(sample) / sample_rate_;
                double phase = 2.0 * PI * (subcarrier_freq_ * t + prev_cycles);
                
                double chroma = (u_norm * std::sin(phase)) + (v_norm * v_switch * std::cos(phase));
                int32_t chroma_signal = static_cast<int32_t>(chroma * luma_range);
                
                y_line[sample] = clamp_to_16bit(y_signal);
                c_line[sample] = clamp_to_16bit(32768 + chroma_signal);
            }
            
            for (int32_t sample = active_end; sample < params_.field_width; ++sample) {
                c_line[sample] = static_cast<uint16_t>(32768);
            }
        }
    }
}

} // namespace encode_orc


