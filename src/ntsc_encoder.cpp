/*
 * File:        ntsc_encoder.cpp
 * Module:      encode-orc
 * Purpose:     NTSC video signal encoder implementation
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: 2026 Simon Inns
 */

#include "ntsc_encoder.h"
#include "biphase_encoder.h"
#include <cstring>
#include <algorithm>

namespace encode_orc {

NTSCEncoder::NTSCEncoder(const VideoParameters& params) 
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
                                 line, field_number, frame_width);
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
    // NTSC color burst
    // Position: Starts approximately 5.3 µs after sync (back porch)
    // Duration: 9 cycles of subcarrier (approximately 2.5 µs at 3.58 MHz)
    // Amplitude: 40 IRE peak-to-peak (±20 IRE about blanking level)
    // Phase: fixed at +180° (opposite of PAL's swinging burst)
    
    int32_t burst_start = params_.colour_burst_start;
    int32_t burst_end = params_.colour_burst_end;
    
    // Calculate absolute line number
    // NTSC uses similar calculations but with 525-line frame and 263-line fields
    bool is_first_field = (field_number % 2) == 0;
    int32_t frame_line = is_first_field ? (line_number * 2 + 1) : (line_number * 2 + 2);
    
    // For NTSC, we use a 2-field sequence (not 8-field like PAL)
    int32_t field_id = field_number % 2;
    int32_t prev_lines = (field_id * 263) + (frame_line / 2);
    
    // Calculate phase advance from previous lines
    // NTSC: 227.5 subcarrier cycles per line (approximately)
    double prev_cycles = prev_lines * 227.5;
    
    // NTSC burst phase is fixed at 180° (π radians)
    // Unlike PAL which has a swinging burst
    double burst_phase_offset = PI;
    
    // Calculate burst amplitude once (constant for all samples in burst)
    // NTSC burst amplitude: 40 IRE peak-to-peak (±20 IRE about blanking level)
    int32_t luma_range = white_level_ - blanking_level_;
    double burst_amplitude_ire = 40.0;  // 40 IRE peak-to-peak
    double ire_per_16bit = (static_cast<double>(luma_range) / 100.0);
    int32_t burst_amplitude = static_cast<int32_t>((burst_amplitude_ire / 2.0) * ire_per_16bit);
    
    // Generate color burst
    for (int32_t sample = burst_start; sample < burst_end; ++sample) {
        // Color burst phase-locked with active video chroma
        double time_phase = 2.0 * PI * subcarrier_freq_ * sample / sample_rate_;
        double phase = 2.0 * PI * prev_cycles + time_phase + burst_phase_offset;
        
        double burst_signal = std::sin(phase);
        
        int32_t sample_value = blanking_level_ + 
                              static_cast<int32_t>(burst_amplitude * burst_signal);
        line_buffer[sample] = clamp_signal(sample_value);
    }
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
                                    int32_t width) {
    // Encode the active video portion of the line
    int32_t active_start = params_.active_video_start;
    int32_t active_end = params_.active_video_end;
    int32_t active_width = active_end - active_start;
    
    // Calculate absolute line number in NTSC sequence
    bool is_first_field = (field_number % 2) == 0;
    int32_t frame_line = is_first_field ? (line_number * 2 + 1) : (line_number * 2 + 2);
    
    int32_t field_id = field_number % 2;
    int32_t prev_lines = (field_id * 263) + (frame_line / 2);
    
    // NTSC subcarrier phase calculation
    // prevCycles accumulates by 227.5 per line (approximately)
    double prev_cycles = prev_lines * 227.5;
    
    for (int32_t sample = active_start; sample < active_end; ++sample) {
        // Map sample position to source pixel
        double pixel_pos = static_cast<double>(sample - active_start) * width / active_width;
        int32_t pixel_x = static_cast<int32_t>(pixel_pos);
        
        // Clamp to valid range
        if (pixel_x >= width) pixel_x = width - 1;
        
        // Get YIQ values from source
        uint16_t y = y_line[pixel_x];
        uint16_t i = i_line[pixel_x];
        uint16_t q = q_line[pixel_x];
        
        // Calculate subcarrier phase for this sample position
        double t = static_cast<double>(sample) / sample_rate_;
        double phase = 2.0 * PI * (subcarrier_freq_ * t + prev_cycles);
        
        // Convert YIQ to composite signal
        // NTSC: Composite = Y + I*sin(ωt) + Q*cos(ωt)
        // (No V-switch like PAL, so simpler calculation)
        
        // Normalize YIQ values to 0.0-1.0 range
        double y_norm = static_cast<double>(y) / 65535.0;
        
        // Scale I/Q back to their actual ranges
        // NTSC IQ values typically range differently than PAL UV
        const double I_MAX = 0.5957;
        const double Q_MAX = 0.5226;
        double i_norm = ((static_cast<double>(i) / 65535.0) - 0.5) * 2.0 * I_MAX;
        double q_norm = ((static_cast<double>(q) / 65535.0) - 0.5) * 2.0 * Q_MAX;
        
        // Scale luma to video range
        int32_t luma_range = white_level_ - black_level_;
        int32_t luma_scaled = black_level_ + 
                             static_cast<int32_t>(y_norm * luma_range);
        
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

} // namespace encode_orc
