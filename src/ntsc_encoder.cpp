/*
 * File:        ntsc_encoder.cpp
 * Module:      encode-orc
 * Purpose:     NTSC video signal encoder implementation
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: 2026 Simon Inns
 */

#include "ntsc_encoder.h"
#include "color_burst_generator.h"
#include "biphase_encoder.h"
#include <cstring>
#include <algorithm>

namespace encode_orc {

NTSCEncoder::NTSCEncoder(const VideoParameters& params,
                        bool enable_chroma_filter,
                        bool enable_luma_filter) 
    : params_(params), vits_enabled_(false) {
    
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
        chroma_filter_ = Filters::create_ntsc_uv_filter();
    }
    if (enable_luma_filter) {
        luma_filter_ = Filters::create_ntsc_uv_filter();  // Reuse same filter for luma
    }
}

Frame NTSCEncoder::encode_frame(const FrameBuffer& frame_buffer, int32_t field_number, int32_t frame_number_for_vbi) {
    Frame frame(params_.field_width, params_.field_height);
    
    // Encode first field (even lines: 0, 2, 4, ...)
    frame.field1() = encode_field(frame_buffer, field_number, true, frame_number_for_vbi);
    
    // Encode second field (odd lines: 1, 3, 5, ...)
    frame.field2() = encode_field(frame_buffer, field_number + 1, false, frame_number_for_vbi);
    
    return frame;
}

Field NTSCEncoder::encode_field(const FrameBuffer& frame_buffer, 
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
    
    // Detect if source is in studio code space (≤1023) to preserve sub-black
    const uint16_t* y_plane_all = frame_buffer.data().data();
    const int32_t total_pixels = frame_buffer.width() * frame_buffer.height();
    uint16_t y_max = 0;
    for (int32_t i = 0; i < total_pixels; ++i) {
        if (y_plane_all[i] > y_max) y_max = y_plane_all[i];
    }
    const bool studio_range_input = (y_max <= 1023);
    
    // Get frame dimensions
    int32_t frame_width = frame_buffer.width();
    int32_t frame_height = frame_buffer.height();
    
    // NTSC has 263 lines per field
    for (int32_t line = 0; line < LINES_PER_FIELD; ++line) {
        uint16_t* line_buffer = field.line_data(line);
        
        // Lines 1-3: Vertical sync (field sync)
        if (line < VSYNC_LINES) {
            generate_vsync_line(line_buffer, line);
        }
        // Lines 4-20: VBI (Vertical Blanking Interval)
        else if (line < ACTIVE_LINES_START) {
            // Lines 15, 16, 17 (0-indexed) = field lines 16, 17, 18 contain biphase frame numbers
            if (frame_number_for_vbi >= 0 && (line == 15 || line == 16 || line == 17)) {
                generate_biphase_vbi_line(line_buffer, line, field_number, frame_number_for_vbi);
            }
            // VITS lines (if enabled)
            else if (is_vits_enabled()) {
                // First field VITS lines (0-indexed in field)
                if (is_first_field && line == 18) {  // Line 19 in NTSC frame (first field)
                    generate_blanking_line(line_buffer);
                    generate_sync_pulse(line_buffer, line);
                    generate_color_burst(line_buffer, line, field_number);
                    vits_generator_->generate_vir_line19(line_buffer, field_number);
                }
                else if (is_first_field && line == 19) {  // Line 20 in NTSC frame (first field) - NTC-7 Composite
                    generate_blanking_line(line_buffer);
                    generate_sync_pulse(line_buffer, line);
                    generate_color_burst(line_buffer, line, field_number);
                    vits_generator_->generate_ntc7_composite_line17(line_buffer, field_number);
                }
                // Second field VITS lines (0-indexed in field)
                else if (!is_first_field && line == 18) {  // Line 282 in NTSC frame (second field)
                    generate_blanking_line(line_buffer);
                    generate_sync_pulse(line_buffer, line);
                    generate_color_burst(line_buffer, line, field_number);
                    vits_generator_->generate_vir_line19(line_buffer, field_number);
                }
                else if (!is_first_field && line == 19) {  // Line 283 in NTSC frame (second field) - NTC-7 Combination
                    generate_blanking_line(line_buffer);
                    generate_sync_pulse(line_buffer, line);
                    generate_color_burst(line_buffer, line, field_number);
                    vits_generator_->generate_ntc7_combination_line20(line_buffer, field_number);
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
        // Lines 21-263: Active video
        else if (line < ACTIVE_LINES_END) {
            // Calculate which line in the source frame to use
            int32_t line_in_field = line - ACTIVE_LINES_START;
            int32_t line_in_frame = is_first_field ? (line_in_field * 2) : (line_in_field * 2 + 1);
            
            // Check if line is within source frame
            if (line_in_frame < frame_height) {
                // Get pointers to YIQ data
                const uint16_t* frame_data = frame_buffer.data().data();
                int32_t pixel_count = frame_width * frame_height;
                const uint16_t* y_plane = frame_data;
                const uint16_t* i_plane = frame_data + pixel_count;
                const uint16_t* q_plane = frame_data + pixel_count * 2;
                
                const uint16_t* y_line = y_plane + (line_in_frame * frame_width);
                const uint16_t* i_line = i_plane + (line_in_frame * frame_width);
                const uint16_t* q_line = q_plane + (line_in_frame * frame_width);
                
                // Initialize line with blanking level, then add sync and color burst
                generate_blanking_line(line_buffer);
                generate_sync_pulse(line_buffer, line);
                generate_color_burst(line_buffer, line, field_number);
                
                // Encode active video portion
                encode_active_line(line_buffer, y_line, i_line, q_line, 
                                 line, field_number, frame_width, studio_range_input);
            } else {
                // Beyond source frame - use blanking
                generate_blanking_line(line_buffer);
                generate_sync_pulse(line_buffer, line);
                generate_color_burst(line_buffer, line, field_number);
            }
        }
        // Lines beyond active video: Post-video blanking
        else {
            generate_blanking_line(line_buffer);
            generate_sync_pulse(line_buffer, line);
            generate_color_burst(line_buffer, line, field_number);
        }
    }
    
    return field;
}

void NTSCEncoder::generate_sync_pulse(uint16_t* line_buffer, int32_t /* line_number */) {
    // NTSC horizontal sync pulse
    // Duration: 4.7 µs (approximately 67 samples at 14.3 MHz)
    // Front porch: 1.5 µs (approximately 21 samples)
    // Back porch: variable, followed by color burst
    
    double sample_duration = 1.0 / sample_rate_;  // seconds per sample
    int32_t sync_duration = static_cast<int32_t>(4.7e-6 / sample_duration);  // 4.7 µs
    
    // Sync pulse at beginning of line
    for (int32_t i = 0; i < sync_duration; ++i) {
        line_buffer[i] = static_cast<uint16_t>(sync_level_);
    }
}

void NTSCEncoder::generate_color_burst(uint16_t* line_buffer, int32_t line_number, int32_t field_number) {
    // Delegate to shared color burst generator
    ColorBurstGenerator burst_gen(params_);
    int32_t luma_range = white_level_ - blanking_level_;
    int32_t burst_amplitude = static_cast<int32_t>((20.0 / 100.0) * luma_range);
    burst_gen.generate_ntsc_burst(line_buffer, line_number, field_number, blanking_level_, burst_amplitude);
}

void NTSCEncoder::generate_color_burst_chroma(uint16_t* line_buffer, int32_t line_number, int32_t field_number) {
    // Generate color burst on chroma channel (centered at 32768)
    ColorBurstGenerator burst_gen(params_);
    
    // Calculate burst amplitude: ±20 IRE (same as composite mode)
    int32_t luma_range = white_level_ - blanking_level_;
    int32_t burst_amplitude = static_cast<int32_t>((20.0 / 100.0) * luma_range);
    
    // Generate burst centered at 32768 (16-bit midpoint for separate-yc chroma)
    burst_gen.generate_ntsc_burst(line_buffer, line_number, field_number, 32768, burst_amplitude);
}

void NTSCEncoder::generate_color_burst_chroma_line(uint16_t* line_buffer, int32_t line_number, 
                                                    int32_t field_number, int32_t /* burst_end */) {
    // Generate color burst on chroma for the portion before active video
    ColorBurstGenerator burst_gen(params_);
    
    // Calculate burst amplitude: ±20 IRE (same as composite mode)
    int32_t luma_range = white_level_ - blanking_level_;
    int32_t burst_amplitude = static_cast<int32_t>((20.0 / 100.0) * luma_range);
    
    // Generate burst centered at 32768
    burst_gen.generate_ntsc_burst(line_buffer, line_number, field_number, 32768, burst_amplitude);
}

void NTSCEncoder::generate_vsync_line(uint16_t* line_buffer, int32_t line_number) {
    // NTSC vertical sync pattern
    // Lines 1-3: broad pulses followed by narrow pulses
    
    // For simplicity, fill with sync level during vsync
    std::fill_n(line_buffer, params_.field_width, static_cast<uint16_t>(sync_level_));
    
    // Add equalizing pulses (simplified pattern)
    int32_t half_line = params_.field_width / 2;
    
    if (line_number < 2) {
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

void NTSCEncoder::generate_blanking_line(uint16_t* line_buffer) {
    // Fill line with blanking level
    std::fill_n(line_buffer, params_.field_width, static_cast<uint16_t>(blanking_level_));
}

void NTSCEncoder::encode_active_line(uint16_t* line_buffer,
                                    const uint16_t* y_line,
                                    const uint16_t* i_line,
                                    const uint16_t* q_line,
                                    int32_t line_number,
                                    int32_t field_number,
                                    int32_t width,
                                    bool studio_range_input) {
    // Apply filters if enabled
    // Note: We need to copy the line data first to apply filtering
    std::vector<uint16_t> y_filtered(y_line, y_line + width);
    std::vector<uint16_t> i_filtered(i_line, i_line + width);
    std::vector<uint16_t> q_filtered(q_line, q_line + width);
    
    // Apply filters if configured
    if (luma_filter_) {
        luma_filter_->apply(y_filtered);
    }
    if (chroma_filter_) {
        chroma_filter_->apply(i_filtered);
        chroma_filter_->apply(q_filtered);
    }
    
    // Use filtered data for encoding
    const uint16_t* y_data = y_filtered.data();
    const uint16_t* i_data = i_filtered.data();
    const uint16_t* q_data = q_filtered.data();
    
    // Encode the active video portion of the line
    int32_t active_start = params_.active_video_start;
    int32_t active_end = params_.active_video_end;
    int32_t active_width = active_end - active_start;
    
    // Calculate absolute line number in NTSC sequence
    // Keep local variables minimal; phase calculation now uses absolute lines
    // with half-line offset modeled via 262.5 lines per field.
    
    // Note: previous integer-based calculation used 263 and integer division,
    // which discarded the half-line offset and prevented proper 4-field color framing.
    
    // NTSC subcarrier phase calculation
    // Use 262.5 lines per field to preserve half-line offset between fields,
    // which yields the correct 4-field color framing sequence.
    const double lines_per_field = 262.5;
    const double cycles_per_line = 227.5;

    double absolute_lines = static_cast<double>(field_number) * lines_per_field + static_cast<double>(line_number);
    double prev_cycles = absolute_lines * cycles_per_line;
    
    for (int32_t sample = active_start; sample < active_end; ++sample) {
        // Map sample position to source pixel
        double pixel_pos = static_cast<double>(sample - active_start) * width / active_width;
        int32_t pixel_x = static_cast<int32_t>(pixel_pos);
        
        // Clamp to valid range
        if (pixel_x >= width) pixel_x = width - 1;
        
        // Get YIQ values from filtered source
        uint16_t y = y_data[pixel_x];
        uint16_t i = i_data[pixel_x];
        uint16_t q = q_data[pixel_x];
        
        // Calculate subcarrier phase for this sample position
        double t = static_cast<double>(sample) / sample_rate_;
        double phase = 2.0 * PI * (subcarrier_freq_ * t + prev_cycles);
        
        // Convert YIQ to composite signal
        // NTSC: Composite = Y + I*sin(ωt) + Q*cos(ωt)
        // (No V-switch like PAL, so simpler calculation)
        
        // Handle studio vs full-range input
        int32_t luma_range = white_level_ - black_level_;
        int32_t luma_scaled;
        
        if (studio_range_input) {
            // Studio codes: 64→black_level, 940→white_level, preserve sub-black
            int32_t y_signal = black_level_ + ((static_cast<int32_t>(y) - 64) * luma_range) / 876;
            luma_scaled = clamp_signal(y_signal);
        } else {
            // Full-range: 0-65535 → normalized to black-white range
            double y_norm = static_cast<double>(y) / 65535.0;
            luma_scaled = black_level_ + static_cast<int32_t>(y_norm * luma_range);
        }
        
        // Scale I/Q back to their actual ranges
        // NTSC IQ values typically range differently than PAL UV
        const double I_MAX = 0.5957;
        const double Q_MAX = 0.5226;
        double i_norm, q_norm;
        if (studio_range_input) {
            // Studio chroma: 0-896 range (64-960 studio codes)
            i_norm = ((static_cast<double>(i) / 896.0) - 0.5) * 2.0 * I_MAX;
            q_norm = ((static_cast<double>(q) / 896.0) - 0.5) * 2.0 * Q_MAX;
        } else {
            // Full-range: 0-65535
            i_norm = ((static_cast<double>(i) / 65535.0) - 0.5) * 2.0 * I_MAX;
            q_norm = ((static_cast<double>(q) / 65535.0) - 0.5) * 2.0 * Q_MAX;
        }
        
        // NTSC chroma encoding
        // Composite = Y + I*sin(ωt) + Q*cos(ωt)
        double chroma = (i_norm * std::sin(phase)) + (q_norm * std::cos(phase));
        int32_t chroma_scaled = static_cast<int32_t>(chroma * luma_range);
        
        // Combine luma and chroma
        int32_t composite = luma_scaled + chroma_scaled;
        
        line_buffer[sample] = clamp_signal(composite);
    }
}

uint16_t NTSCEncoder::clamp_signal(double value) const {
    int32_t int_value = static_cast<int32_t>(value);
    if (int_value < 0) return 0;
    if (int_value > 65535) return 65535;
    return static_cast<uint16_t>(int_value);
}

void NTSCEncoder::enable_vits() {
    if (!vits_generator_) {
        vits_generator_ = std::make_unique<NTSCVITSGenerator>(params_);
    }
    vits_enabled_ = true;
}

void NTSCEncoder::disable_vits() {
    vits_enabled_ = false;
}

bool NTSCEncoder::is_vits_enabled() const {
    return vits_enabled_;
}

void NTSCEncoder::generate_biphase_vbi_line(uint16_t* line_buffer, int32_t line_number, 
                                           int32_t field_number, int32_t frame_number) {
    // Start with a standard blanking line with sync and color burst
    generate_blanking_line(line_buffer);
    generate_sync_pulse(line_buffer, line_number);
    generate_color_burst(line_buffer, line_number, field_number);
    
    // Calculate line period (H) for NTSC: approximately 63.56 µs
    double line_period_h = 63.556e-6;  // ~63.56 µs for NTSC
    
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

void NTSCEncoder::encode_frame_yc(const FrameBuffer& frame_buffer, int32_t field_number,
                                  int32_t frame_number_for_vbi,
                                  Field& y_field1, Field& c_field1,
                                  Field& y_field2, Field& c_field2) {
    // Initialize fields
    y_field1.resize(params_.field_width, params_.field_height);
    c_field1.resize(params_.field_width, params_.field_height);
    y_field2.resize(params_.field_width, params_.field_height);
    c_field2.resize(params_.field_width, params_.field_height);
    
    // For separate Y/C output, we encode Y and C directly from source YIQ data:
    // Y field: luma component with sync + blanking (no chroma modulation)
    // C field: chroma-only signal (modulated subcarrier centered at blanking level)
    //
    // This is NOT created by decomposing composite - we build Y and C separately
    // from the source YIQ to avoid any quality loss.
    
    // Get frame dimensions
    int32_t frame_width = frame_buffer.width();
    int32_t frame_height = frame_buffer.height();
    
    // Get pointers to YIQ planes (stored as YUV in FrameBuffer)
    const uint16_t* frame_data = frame_buffer.data().data();
    int32_t pixel_count = frame_width * frame_height;
    const uint16_t* y_plane = frame_data;
    const uint16_t* i_plane = frame_data + pixel_count;
    const uint16_t* q_plane = frame_data + pixel_count * 2;

    // Detect studio-range input (≤1023) to preserve sub-black
    uint16_t y_max = 0;
    for (int32_t i = 0; i < pixel_count; ++i) {
        if (y_plane[i] > y_max) y_max = y_plane[i];
    }
    const bool studio_range_input = (y_max <= 1023);
    
    // Process field 1 (even lines from source)
    for (int32_t line = 0; line < params_.field_height; ++line) {
        uint16_t* y_line = y_field1.line_data(line);
        uint16_t* c_line = c_field1.line_data(line);
        
        // For sync, blanking, and VBI lines
        if (line < ACTIVE_LINES_START) {
            // Initialize Y field with blanking level
            generate_blanking_line(y_line);
            
            // Generate sync and blanking for Y field (no color burst)
            generate_sync_pulse(y_line, line);
            
            // C field gets color burst (modulated at blanking level, centered at 32768)
            generate_color_burst_chroma(c_line, line, field_number);
            
            // Handle VBI lines if enabled
            if (frame_number_for_vbi >= 0 && (line == 14 || line == 15 || line == 16)) {
                generate_biphase_vbi_line(y_line, line, field_number, frame_number_for_vbi);
            }
            // VITS lines for field 1 (if enabled)
            else if (vits_enabled_ && vits_generator_) {
                // First field VITS lines (0-indexed in field)
                if (line == 18) {  // Line 19 in NTSC frame (first field)
                    vits_generator_->generate_vir_line19(y_line, field_number);
                }
                else if (line == 19) {  // Line 20 in NTSC frame (first field) - NTC-7 Composite
                    vits_generator_->generate_ntc7_composite_line17(y_line, field_number);
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
            
            // Calculate NTSC subcarrier phase offset for this line
            // Use 262.5 lines per field to preserve half-line offset between fields
            const double lines_per_field = 262.5;
            const double cycles_per_line = 227.5;
            double absolute_lines = static_cast<double>(field_number) * lines_per_field + static_cast<double>(line);
            double prev_cycles = absolute_lines * cycles_per_line;
            
            for (int32_t sample = active_start; sample < active_end; ++sample) {
                // Map sample position to source pixel
                double pixel_pos = static_cast<double>(sample - active_start) * frame_width / active_width;
                int32_t pixel_x = static_cast<int32_t>(pixel_pos);
                if (pixel_x >= frame_width) pixel_x = frame_width - 1;
                
                // Get filtered YIQ values from source
                uint16_t y_val = y_plane[source_line * frame_width + pixel_x];
                uint16_t i_val = i_plane[source_line * frame_width + pixel_x];
                uint16_t q_val = q_plane[source_line * frame_width + pixel_x];
                
                // Convert Y to luma signal level
                int32_t luma_range = white_level_ - black_level_;
                int32_t y_signal;
                if (studio_range_input) {
                    // Studio codes: 64→black_level, 940→white_level, preserve sub-black
                    int32_t y_delta = static_cast<int32_t>(y_val) - 64;
                    y_signal = black_level_ + (y_delta * luma_range) / 876; // 940-64=876
                } else {
                    // Full-range input
                    double y_norm = static_cast<double>(y_val) / 65535.0;
                    y_signal = black_level_ + static_cast<int32_t>(y_norm * luma_range);
                }
                
                // Convert I/Q to chroma signal
                const double I_MAX = 0.5959;
                const double Q_MAX = 0.5229;
                double i_norm, q_norm;
                if (studio_range_input) {
                    // Studio chroma: 0-896 range
                    i_norm = ((static_cast<double>(i_val) / 896.0) - 0.5) * 2.0 * I_MAX;
                    q_norm = ((static_cast<double>(q_val) / 896.0) - 0.5) * 2.0 * Q_MAX;
                } else {
                    // Full-range chroma
                    i_norm = ((static_cast<double>(i_val) / 65535.0) - 0.5) * 2.0 * I_MAX;
                    q_norm = ((static_cast<double>(q_val) / 65535.0) - 0.5) * 2.0 * Q_MAX;
                }
                
                // Calculate subcarrier phase (include line phase offset for correct NTSC phase)
                double t = static_cast<double>(sample) / sample_rate_;
                double phase = 2.0 * PI * (subcarrier_freq_ * t + prev_cycles);
                
                // Modulate chroma onto subcarrier (NTSC encoding: C = I*sin(ωt) + Q*cos(ωt))
                double chroma = (i_norm * std::sin(phase)) + (q_norm * std::cos(phase));
                int32_t chroma_signal = static_cast<int32_t>(chroma * luma_range);
                
                // Y field: luma only (no chroma)
                y_line[sample] = clamp_to_16bit(y_signal);
                
                // C field: chroma only (centered at 16-bit midpoint, no luma)
                c_line[sample] = clamp_to_16bit(32768 + chroma_signal);
            }
            
            // Fill remaining samples with blanking
            for (int32_t sample = active_end; sample < params_.field_width; ++sample) {
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
            
            // Generate sync and blanking for Y field (no color burst)
            generate_sync_pulse(y_line, line);
            generate_color_burst_chroma(c_line, line, field_number + 1);
            
            if (frame_number_for_vbi >= 0 && (line == 14 || line == 15 || line == 16)) {
                generate_biphase_vbi_line(y_line, line, field_number + 1, frame_number_for_vbi);
            }
            // VITS lines for field 2 (if enabled)
            else if (vits_enabled_ && vits_generator_) {
                // Second field VITS lines (0-indexed in field)
                if (line == 18) {  // Line 282 in NTSC frame (second field)
                    vits_generator_->generate_vir_line19(y_line, field_number + 1);
                }
                else if (line == 19) {  // Line 283 in NTSC frame (second field) - NTC-7 Combination
                    vits_generator_->generate_ntc7_combination_line20(y_line, field_number + 1);
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
            
            // Calculate NTSC subcarrier phase offset for this line
            // Use 262.5 lines per field to preserve half-line offset between fields
            const double lines_per_field = 262.5;
            const double cycles_per_line = 227.5;
            double absolute_lines = static_cast<double>(field_number + 1) * lines_per_field + static_cast<double>(line);
            double prev_cycles = absolute_lines * cycles_per_line;
            
            for (int32_t sample = active_start; sample < active_end; ++sample) {
                double pixel_pos = static_cast<double>(sample - active_start) * frame_width / active_width;
                int32_t pixel_x = static_cast<int32_t>(pixel_pos);
                if (pixel_x >= frame_width) pixel_x = frame_width - 1;
                
                uint16_t y_val = y_plane[source_line * frame_width + pixel_x];
                uint16_t i_val = i_plane[source_line * frame_width + pixel_x];
                uint16_t q_val = q_plane[source_line * frame_width + pixel_x];

                int32_t luma_range = white_level_ - black_level_;
                int32_t y_signal;
                if (studio_range_input) {
                    // Studio codes: 64→black_level, 940→white_level, preserve sub-black
                    int32_t y_delta = static_cast<int32_t>(y_val) - 64;
                    y_signal = black_level_ + (y_delta * luma_range) / 876; // 940-64=876
                } else {
                    // Full-range input
                    double y_norm = static_cast<double>(y_val) / 65535.0;
                    y_signal = black_level_ + static_cast<int32_t>(y_norm * luma_range);
                }
                
                const double I_MAX = 0.5959;
                const double Q_MAX = 0.5229;
                double i_norm, q_norm;
                if (studio_range_input) {
                    // Studio chroma: 0-896 range
                    i_norm = ((static_cast<double>(i_val) / 896.0) - 0.5) * 2.0 * I_MAX;
                    q_norm = ((static_cast<double>(q_val) / 896.0) - 0.5) * 2.0 * Q_MAX;
                } else {
                    // Full-range chroma
                    i_norm = ((static_cast<double>(i_val) / 65535.0) - 0.5) * 2.0 * I_MAX;
                    q_norm = ((static_cast<double>(q_val) / 65535.0) - 0.5) * 2.0 * Q_MAX;
                }
                
                // Calculate subcarrier phase (include line phase offset for correct NTSC phase)
                double t = static_cast<double>(sample) / sample_rate_;
                double phase = 2.0 * PI * (subcarrier_freq_ * t + prev_cycles);
                
                // Modulate chroma onto subcarrier (NTSC encoding: C = I*sin(ωt) + Q*cos(ωt))
                double chroma = (i_norm * std::sin(phase)) + (q_norm * std::cos(phase));
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


