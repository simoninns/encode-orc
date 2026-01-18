/*
 * File:        mp4_loader.cpp
 * Module:      encode-orc
 * Purpose:     MP4 file loading implementation using ffmpeg
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: 2026 Simon Inns
 */

#include "mp4_loader.h"
#include <fstream>
#include <sstream>
#include <iostream>
#include <cstring>
#include <algorithm>
#include <cstdio>
#include <array>
#include <memory>
#include <filesystem>
#include <unistd.h>

namespace encode_orc {

bool MP4Loader::open(const std::string& filename, std::string& error_message) {
    // Check if file exists
    if (!std::filesystem::exists(filename)) {
        error_message = "MP4 file not found: " + filename;
        return false;
    }
    
    filename_ = filename;
    
    // Probe video info using ffprobe
    if (!probe_video_info(error_message)) {
        return false;
    }
    
    is_open_ = true;
    return true;
}

bool MP4Loader::get_dimensions(int32_t& width, int32_t& height) const {
    if (!is_open_) {
        return false;
    }
    width = width_;
    height = height_;
    return true;
}

int32_t MP4Loader::get_frame_count() const {
    return frame_count_;
}

bool MP4Loader::probe_video_info(std::string& error_message) {
    // Use ffprobe to count frames directly using count_packets
    // This is more reliable than nb_frames which is often not set in MP4 files
    // Note: ffprobe outputs in alphabetical order: height, nb_read_packets, r_frame_rate, width
    std::string cmd = "ffprobe -v error -select_streams v:0 "
                     "-count_packets -show_entries stream=width,height,nb_read_packets,r_frame_rate "
                     "-of csv=p=0 \"" + filename_ + "\" 2>&1";
    
    std::array<char, 128> buffer;
    std::string result;
    
    // Open pipe to ffprobe command
    FILE* pipe = popen(cmd.c_str(), "r");
    if (!pipe) {
        error_message = "Failed to run ffprobe command";
        return false;
    }
    
    // Read output
    while (fgets(buffer.data(), buffer.size(), pipe) != nullptr) {
        result += buffer.data();
    }
    
    pclose(pipe);
    
    // Parse result in alphabetical order: width,height,nb_read_packets,r_frame_rate
    // Actually testing shows order is: width,height,r_frame_rate,nb_read_packets
    std::istringstream iss(result);
    std::string token;
    std::vector<std::string> values;
    
    while (std::getline(iss, token, ',')) {
        // Trim whitespace and newlines
        token.erase(0, token.find_first_not_of(" \t\r\n"));
        token.erase(token.find_last_not_of(" \t\r\n") + 1);
        if (!token.empty() && token != "N/A") {
            values.push_back(token);
        }
    }
    
    if (values.size() < 2) {
        error_message = "Failed to get video dimensions from ffprobe. Output: " + result;
        return false;
    }
    
    try {
        // Parse based on actual output order: width,height,r_frame_rate,nb_read_packets
        width_ = std::stoi(values[0]);
        height_ = std::stoi(values[1]);
        
        // Parse frame rate (r_frame_rate is in the format "num/den" or "25/1")
        if (values.size() >= 3 && !values[2].empty()) {
            size_t slash_pos = values[2].find('/');
            if (slash_pos != std::string::npos) {
                double num = std::stod(values[2].substr(0, slash_pos));
                double den = std::stod(values[2].substr(slash_pos + 1));
                frame_rate_ = (den > 0) ? (num / den) : 0.0;
            } else {
                // Might be the packet count, check if it looks like a frame rate
                try {
                    int val = std::stoi(values[2]);
                    if (val > 100) {
                        // This is probably packet count, not frame rate
                        frame_count_ = val;
                        frame_rate_ = 0.0;
                    } else {
                        frame_rate_ = std::stod(values[2]);
                    }
                } catch (...) {
                    frame_rate_ = 0.0;
                }
            }
        }
        
        // Get frame count from nb_read_packets (last value)
        if (values.size() >= 4 && !values[3].empty()) {
            try {
                frame_count_ = std::stoi(values[3]);
            } catch (...) {
                frame_count_ = -1;
            }
        }
        
    } catch (const std::exception& e) {
        error_message = "Failed to parse video info: " + std::string(e.what());
        return false;
    }
    
    return true;
}

bool MP4Loader::extract_frames_to_yuv(int32_t start_frame,
                                      int32_t num_frames,
                                      const std::string& temp_yuv_file,
                                      std::string& error_message) {
    // Build ffmpeg command to extract frames at native resolution
    // For MP4 files, we typically don't need to deinterlace as they're usually progressive
    
    std::ostringstream cmd;
    cmd << "ffmpeg -v error ";
    
    // Input file
    cmd << "-i \"" << filename_ << "\" ";
    
    // Frame selection - use different approach based on whether we're selecting a range
    if (start_frame > 0 || num_frames < frame_count_) {
        // Select specific frame range
        cmd << "-vf \"select='between(n\\," << start_frame << "\\," << (start_frame + num_frames - 1) << ")',setpts=PTS-STARTPTS\" ";
        cmd << "-vsync 0 ";  // Don't drop or duplicate frames
    }
    
    // Limit output frames
    cmd << "-frames:v " << num_frames << " ";
    
    // Explicitly convert to full range first, then to TV range to normalize
    // This ensures we get proper studio range output
    cmd << "-vf \"scale=in_range=auto:out_range=tv\" ";
    
    // Output format: YUV420P 8-bit
    cmd << "-pix_fmt yuv420p ";
    cmd << "-f rawvideo ";
    cmd << "-an ";
    cmd << "-y ";  // Overwrite output file
    cmd << "\"" << temp_yuv_file << "\" 2>&1";
    
    std::string cmd_str = cmd.str();
    if (false) {  // Debug: set to true to see the command
        std::cerr << "FFmpeg command: " << cmd_str << "\n";
    }
    
    // Execute ffmpeg command
    std::array<char, 256> buffer;
    std::string ffmpeg_output;
    
    FILE* pipe = popen(cmd_str.c_str(), "r");
    if (!pipe) {
        error_message = "Failed to run ffmpeg command";
        return false;
    }
    
    // Capture any error output
    while (fgets(buffer.data(), buffer.size(), pipe) != nullptr) {
        ffmpeg_output += buffer.data();
    }
    
    int return_code = pclose(pipe);
    
    if (return_code != 0) {
        error_message = "ffmpeg extraction failed with code " + std::to_string(return_code);
        if (!ffmpeg_output.empty()) {
            error_message += ": " + ffmpeg_output;
        }
        return false;
    }
    
    // Verify output file exists
    if (!std::filesystem::exists(temp_yuv_file)) {
        error_message = "ffmpeg did not create output file: " + temp_yuv_file;
        return false;
    }
    
    return true;
}

void MP4Loader::convert_yuv420p_to_frame(const std::vector<uint8_t>& yuv_data,
                                         int32_t width,
                                         int32_t height,
                                         FrameBuffer& frame) {
    // YUV420P is PLANAR format:
    // - Y plane: width * height samples (8-bit)
    // - U plane: (width/2) * (height/2) samples (8-bit)
    // - V plane: (width/2) * (height/2) samples (8-bit)
    // 
    // ffmpeg outputs studio range YUV:
    // - Y: 16-235 (8-bit) 
    // - U/V: 16-240 (8-bit), with 128 as neutral
    // 
    // Convert to normalized form matching the encoder expectations:
    // - Y: keep as 10-bit studio range (16*4 to 235*4 = 64-940)
    // - U/V: convert to normalized form (0-896)
    //   Studio chroma 16-240 (8-bit) -> (value - 16) * 4 = 0-896 (10-bit)
    
    int32_t actual_width = width;
    
    // Pad to 720 pixels if needed (common target for PAL)
    int32_t target_width = (actual_width < 720) ? 720 : actual_width;
    
    frame.resize(target_width, height, FrameBuffer::Format::YUV444P16);
    
    const uint8_t* y_plane_src = yuv_data.data();
    const uint8_t* u_plane_src = y_plane_src + (actual_width * height);
    const uint8_t* v_plane_src = u_plane_src + ((actual_width / 2) * (height / 2));
    
    // Get output plane pointers
    uint16_t* y_plane = frame.data().data();
    uint16_t* u_plane = y_plane + (target_width * height);
    uint16_t* v_plane = u_plane + (target_width * height);
    
    // Y: convert from 8-bit studio range to 10-bit studio range (multiply by 4)
    auto to_luma = [](uint8_t value) {
        return static_cast<uint16_t>(static_cast<uint16_t>(value) << 2);
    };
    
    // Chroma: convert from 8-bit studio range to 10-bit normalized form
    // Studio 8-bit: 16-240 with 128 as neutral
    // Normalized 10-bit: 0-896 with 448 as neutral
    auto to_chroma = [](uint8_t studio_value) {
        // Clamp to studio range
        uint8_t clamped = std::clamp(studio_value, static_cast<uint8_t>(16), static_cast<uint8_t>(240));
        // Subtract offset and scale: (value - 16) * 4
        int32_t delta = clamped - 16;
        int32_t scaled = delta * 4;
        return static_cast<uint16_t>(std::min(896, scaled));
    };
    
    // Calculate padding (distribute evenly on left and right)
    int32_t pixels_to_add = target_width - actual_width;
    int32_t left_pad = pixels_to_add / 2;
    int32_t right_pad = pixels_to_add - left_pad;
    
    // Neutral values for padding
    const uint16_t neutral_y = to_luma(16);      // Black in studio range
    const uint16_t neutral_u = to_chroma(128);   // Neutral chroma in normalized form
    const uint16_t neutral_v = to_chroma(128);   // Neutral chroma in normalized form
    
    // Convert planar YUV420 to YUV444 with padding
    for (int32_t row = 0; row < height; ++row) {
        int32_t dst_row_offset = row * target_width;
        
        // Left padding
        for (int32_t col = 0; col < left_pad; ++col) {
            y_plane[dst_row_offset + col] = neutral_y;
            u_plane[dst_row_offset + col] = neutral_u;
            v_plane[dst_row_offset + col] = neutral_v;
        }
        
        // Convert actual data with nearest-neighbor sampling for chroma
        for (int32_t col = 0; col < actual_width; ++col) {
            int32_t dst_idx = dst_row_offset + left_pad + col;
            int32_t src_idx = row * actual_width + col;
            
            // Y is full resolution
            y_plane[dst_idx] = to_luma(y_plane_src[src_idx]);
            
            // U and V are quarter-resolution (4:2:0) - upsample with nearest neighbor
            int32_t chroma_row = row / 2;
            int32_t chroma_col = col / 2;
            int32_t uv_idx = chroma_row * (actual_width / 2) + chroma_col;
            
            u_plane[dst_idx] = to_chroma(u_plane_src[uv_idx]);
            v_plane[dst_idx] = to_chroma(v_plane_src[uv_idx]);
        }
        
        // Right padding
        for (int32_t col = 0; col < right_pad; ++col) {
            int32_t dst_idx = dst_row_offset + left_pad + actual_width + col;
            y_plane[dst_idx] = neutral_y;
            u_plane[dst_idx] = neutral_u;
            v_plane[dst_idx] = neutral_v;
        }
    }
}

bool MP4Loader::load_frame(int32_t frame_number,
                           int32_t expected_width,
                           int32_t expected_height,
                           const VideoParameters& params,
                           FrameBuffer& frame,
                           std::string& error_message) {
    std::vector<FrameBuffer> frames;
    if (!load_frames(frame_number, 1, expected_width, expected_height, params, frames, error_message)) {
        return false;
    }
    
    if (frames.empty()) {
        error_message = "No frame was loaded";
        return false;
    }
    
    frame = std::move(frames[0]);
    return true;
}

bool MP4Loader::load_frames(int32_t start_frame,
                            int32_t num_frames,
                            int32_t expected_width,
                            int32_t expected_height,
                            const VideoParameters& params,
                            std::vector<FrameBuffer>& frames,
                            std::string& error_message) {
    if (!is_open_) {
        error_message = "MP4 file is not open";
        return false;
    }
    
    // Validate dimensions
    if (width_ != expected_width || height_ != expected_height) {
        error_message = "MP4 dimension mismatch: expected " + 
                       std::to_string(expected_width) + "x" + std::to_string(expected_height) +
                       ", got " + std::to_string(width_) + "x" + std::to_string(height_);
        return false;
    }
    
    // Validate frame rate matches video system
    std::string format_error;
    if (!validate_format(params.system, format_error)) {
        error_message = format_error;
        return false;
    }
    
    // Validate frame range
    if (frame_count_ > 0 && start_frame + num_frames > frame_count_) {
        error_message = "Requested frame range exceeds video length: " +
                       std::to_string(start_frame) + "-" + std::to_string(start_frame + num_frames - 1) +
                       " (video has " + std::to_string(frame_count_) + " frames)";
        return false;
    }
    
    // Create temporary file for YUV data
    std::string temp_yuv_file = "/tmp/encode_orc_mp4_" + std::to_string(getpid()) + ".yuv";
    
    // Extract frames to temporary YUV file
    if (!extract_frames_to_yuv(start_frame, num_frames, temp_yuv_file, error_message)) {
        // Clean up temp file if it exists
        std::filesystem::remove(temp_yuv_file);
        return false;
    }
    
    // Read YUV data
    std::ifstream file(temp_yuv_file, std::ios::binary);
    if (!file) {
        error_message = "Cannot open temporary YUV file: " + temp_yuv_file;
        std::filesystem::remove(temp_yuv_file);
        return false;
    }
    
    // Get file size
    file.seekg(0, std::ios::end);
    size_t file_size = file.tellg();
    file.seekg(0, std::ios::beg);
    
    // For YUV420P per frame:
    //   Y plane: width * height bytes
    //   U plane: (width/2) * (height/2) bytes
    //   V plane: (width/2) * (height/2) bytes
    // Total = width*height + width*height/4 + width*height/4 = width*height*1.5
    size_t frame_size = width_ * height_ * 3 / 2;
    
    // Check if we have at least one complete frame
    if (file_size < frame_size) {
        error_message = "YUV file too small: expected at least " + 
                       std::to_string(frame_size) + " bytes (one frame), got " + 
                       std::to_string(file_size) + " bytes";
        file.close();
        std::filesystem::remove(temp_yuv_file);
        return false;
    }
    
    // Calculate actual number of frames available
    int32_t actual_num_frames = static_cast<int32_t>(file_size / frame_size);
    if (actual_num_frames < num_frames) {
        // ffmpeg may extract fewer frames than requested - adjust
        int32_t adjusted_num_frames = actual_num_frames;
        if (adjusted_num_frames <= 0) {
            error_message = "No complete frames extracted from MP4 file";
            file.close();
            std::filesystem::remove(temp_yuv_file);
            return false;
        }
        // Silently use what we got (common with frame extraction)
        num_frames = adjusted_num_frames;
    }
    
    // Read all frames
    frames.clear();
    frames.reserve(num_frames);
    
    std::vector<uint8_t> frame_data(frame_size);
    
    for (int32_t i = 0; i < num_frames; ++i) {
        file.read(reinterpret_cast<char*>(frame_data.data()), frame_size);
        
        if (!file) {
            error_message = "Failed to read frame " + std::to_string(i) + " from YUV file";
            file.close();
            std::filesystem::remove(temp_yuv_file);
            return false;
        }
        
        FrameBuffer frame;
        convert_yuv420p_to_frame(frame_data, width_, height_, frame);
        frames.push_back(std::move(frame));
    }
    
    file.close();
    
    // Clean up temporary file
    std::filesystem::remove(temp_yuv_file);
    
    return true;
}

void MP4Loader::close() {
    is_open_ = false;
    filename_.clear();
    width_ = 0;
    height_ = 0;
    frame_count_ = -1;
    frame_rate_ = 0.0;
}

bool MP4Loader::validate_format(VideoSystem system, std::string& error_message) {
    // Expected frame rates for each system
    const double PAL_FRAME_RATE = 25.0;
    const double NTSC_FRAME_RATE = 29.97;  // 30000/1001
    const double TOLERANCE = 0.1;  // Allow ±0.1 fps tolerance
    
    if (frame_rate_ <= 0.0) {
        error_message = "Warning: MP4 file frame rate could not be determined";
        return true;  // Not a hard error, just a warning
    }
    
    double expected_fps = (system == VideoSystem::PAL) ? PAL_FRAME_RATE : NTSC_FRAME_RATE;
    double fps_diff = std::abs(frame_rate_ - expected_fps);
    
    if (fps_diff > TOLERANCE) {
        error_message = "MP4 frame rate mismatch: expected " + 
                       std::to_string(expected_fps) + " fps for " +
                       (system == VideoSystem::PAL ? "PAL" : "NTSC") +
                       ", got " + std::to_string(frame_rate_) + " fps";
        return false;
    }
    
    return true;
}

} // namespace encode_orc
