/*
 * File:        mov_loader.cpp
 * Module:      encode-orc
 * Purpose:     MOV file loading implementation using ffmpeg
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: 2026 Simon Inns
 */

#include "mov_loader.h"
#include "video_loader_base.h"
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

bool MOVLoader::open(const std::string& filename, std::string& error_message) {
    // Check if file exists
    if (!std::filesystem::exists(filename)) {
        error_message = "MOV file not found: " + filename;
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

bool MOVLoader::get_dimensions(int32_t& width, int32_t& height) const {
    if (!is_open_) {
        return false;
    }
    width = width_;
    height = height_;
    return true;
}

int32_t MOVLoader::get_frame_count() const {
    return frame_count_;
}

bool MOVLoader::probe_video_info(std::string& error_message) {
    // Use ffprobe to get video dimensions, frame count, and frame rate
    std::string cmd = "ffprobe -v error -select_streams v:0 "
                     "-show_entries stream=width,height,nb_frames,r_frame_rate,avg_frame_rate "
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
    
    // Parse result: width,height,nb_frames,r_frame_rate,avg_frame_rate
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
                frame_rate_ = std::stod(values[2]);
            }
        }
        
        // Frame count is the last non-empty value (avg_frame_rate position)
        // FFprobe returns: width,height,r_frame_rate,avg_frame_rate,nb_frames
        if (values.size() >= 5 && !values[4].empty()) {
            frame_count_ = std::stoi(values[4]);
        } else if (values.size() >= 4) {
            // Fallback: try to parse avg_frame_rate as int if it looks like a frame count
            try {
                int val = std::stoi(values[3]);
                if (val > 0 && val < 10000) {  // Reasonable frame count range
                    frame_count_ = val;
                }
            } catch (...) {
                frame_count_ = -1;
            }
        } else {
            frame_count_ = -1;
        }
        
    } catch (const std::exception& e) {
        error_message = "Failed to parse video info: " + std::string(e.what());
        return false;
    }
    
    return true;
}

bool MOVLoader::extract_frames_to_yuv(int32_t start_frame,
                                      int32_t num_frames,
                                      const std::string& temp_yuv_file,
                                      std::string& error_message) {
    // Build ffmpeg command to extract frames at native resolution
    // We will pad to target width later to preserve original video quality
    // Strategy: extract starting frame with proper frame counting
    
    std::ostringstream cmd;
    cmd << "ffmpeg -v error ";
    
    // Input file
    cmd << "-i \"" << filename_ << "\" ";
    
    // Frame selection with deinterlacing
    // Important: apply deinterlacing BEFORE select filter
    // yadif doubles the frame count, so we need to account for that
    // Strategy: deinterlace first, then select frames from the deinterlaced output
    cmd << "-vf \"yadif=0:-1:0,select='between(n\\," << start_frame << "\\," << (start_frame + num_frames - 1) << ")',setpts=PTS-STARTPTS\" ";
    cmd << "-frames:v " << num_frames << " ";  // Explicitly limit output frames
    
    cmd << "-pix_fmt yuv422p10le ";
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

void MOVLoader::convert_yuv422p10le_to_frame(const std::vector<uint8_t>& yuv_data,
                                             int32_t width,
                                             int32_t height,
                                             FrameBuffer& frame) {
    // YUV422P10LE is PLANAR format:
    // - Y plane: width * height samples (10-bit, little-endian in 16-bit words)
    // - U plane: (width/2) * height samples (10-bit, little-endian in 16-bit words)
    // - V plane: (width/2) * height samples (10-bit, little-endian in 16-bit words)
    // 
    // Note: ffmpeg outputs YUV values in studio range:
    // - Y: 64-940 (10-bit)
    // - U/V: 64-960 (10-bit), with 512 as neutral
    // These must be converted to normalized form matching YUV422 loader expectations
    
    int32_t actual_width = width;
    int32_t target_width = (actual_width < 720) ? 720 : actual_width;
    
    const uint16_t* y_plane_src = reinterpret_cast<const uint16_t*>(yuv_data.data());
    const uint16_t* u_plane_src = y_plane_src + (actual_width * height);
    const uint16_t* v_plane_src = u_plane_src + ((actual_width / 2) * height);
    
    // Extract 10-bit values
    auto extract_10bit = [](uint16_t value) {
        return static_cast<uint16_t>(value & 0x3FF);
    };
    
    // Create temporary full-resolution plane arrays with extracted 10-bit values
    std::vector<uint16_t> y_full(actual_width * height);
    std::vector<uint16_t> u_half(actual_width / 2 * height);
    std::vector<uint16_t> v_half(actual_width / 2 * height);
    
    for (size_t i = 0; i < y_full.size(); ++i) {
        y_full[i] = extract_10bit(y_plane_src[i]);
    }
    for (size_t i = 0; i < u_half.size(); ++i) {
        u_half[i] = VideoLoaderUtils::chroma_10bit_to_normalized(extract_10bit(u_plane_src[i]));
        v_half[i] = VideoLoaderUtils::chroma_10bit_to_normalized(extract_10bit(v_plane_src[i]));
    }
    
    // Use common padding and upsampling function (with 4:2:2 chroma subsampling)
    uint16_t neutral_y = VideoLoaderUtils::NORMALIZED_LUMA_MIN_10BIT;
    uint16_t neutral_u = VideoLoaderUtils::NORMALIZED_CHROMA_NEUTRAL_10BIT;
    uint16_t neutral_v = VideoLoaderUtils::NORMALIZED_CHROMA_NEUTRAL_10BIT;
    
    VideoLoaderUtils::pad_and_upsample_yuv(target_width, actual_width, height, frame,
                                           y_full.data(), u_half.data(), v_half.data(),
                                           2, 1,  // 4:2:2 subsampling
                                           neutral_y, neutral_u, neutral_v);
}


bool MOVLoader::load_frame(int32_t frame_number,
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

bool MOVLoader::load_frames(int32_t start_frame,
                            int32_t num_frames,
                            int32_t expected_width,
                            int32_t expected_height,
                            const VideoParameters& params,
                            std::vector<FrameBuffer>& frames,
                            std::string& error_message) {
    if (!is_open_) {
        error_message = "MOV file is not open";
        return false;
    }
    
    // Validate dimensions
    std::string dim_error;
    if (!validate_dimensions(expected_width, expected_height, dim_error)) {
        error_message = dim_error;
        return false;
    }
    
    // Validate frame rate matches video system
    std::string format_error;
    if (!validate_format(params.system, format_error)) {
        error_message = format_error;
        return false;
    }
    
    // Validate frame range
    std::string range_error;
    if (!validate_frame_range(start_frame, num_frames, range_error)) {
        error_message = range_error;
        return false;
    }
    
    // Create temporary file for YUV data
    std::string temp_yuv_file = "/tmp/encode_orc_mov_" + std::to_string(getpid()) + ".yuv";
    
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
    
    // ffmpeg may apply crop metadata and extract at a different width than reported by ffprobe
    // Calculate actual width from file size
    // For YUV422P10LE per frame:
    //   Y plane: width * height * 2 bytes (10-bit in 16-bit words)
    //   U plane: (width/2) * height * 2 bytes
    //   V plane: (width/2) * height * 2 bytes
    // Total = width*height*2 + width*height*2 = width*height*4
    // file_size = num_frames * width * height * 4
    // So: width = file_size / (num_frames * height * 4)
    int32_t actual_width = static_cast<int32_t>(file_size / (num_frames * height_ * 4));
    
    if (actual_width <= 0 || actual_width > 2000) {
        error_message = "Calculated invalid actual width: " + std::to_string(actual_width);
        file.close();
        std::filesystem::remove(temp_yuv_file);
        return false;
    }
    
    // Recalculate frame size with actual extracted width
    size_t frame_size = actual_width * height_ * 2 +  // Y plane
                        (actual_width / 2) * height_ * 2 +  // U plane
                        (actual_width / 2) * height_ * 2;    // V plane
    
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
            error_message = "No complete frames extracted from MOV file";
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
        convert_yuv422p10le_to_frame(frame_data, actual_width, height_, frame);
        frames.push_back(std::move(frame));
    }
    
    file.close();
    
    // Clean up temporary file
    std::filesystem::remove(temp_yuv_file);
    
    return true;
}

void MOVLoader::close() {
    is_open_ = false;
    filename_.clear();
    width_ = 0;
    height_ = 0;
    frame_count_ = -1;
    frame_rate_ = 0.0;
}

bool MOVLoader::validate_format(VideoSystem system, std::string& error_message) {
    std::string format_error;
    if (!VideoLoaderUtils::validate_frame_rate(frame_rate_, system, 0.1)) {
        error_message = "MOV frame rate mismatch: expected " + 
                       std::to_string(VideoLoaderUtils::get_expected_frame_rate(system)) + 
                       " fps for " + (system == VideoSystem::PAL ? "PAL" : "NTSC") +
                       ", got " + std::to_string(frame_rate_) + " fps";
        return false;
    }
    return true;
}


} // namespace encode_orc
