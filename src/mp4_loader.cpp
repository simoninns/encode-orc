/*
 * File:        mp4_loader.cpp
 * Module:      encode-orc
 * Purpose:     MP4 file loading implementation using ffmpeg
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: 2026 Simon Inns
 */

#include "mp4_loader.h"
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
    int32_t target_width = (actual_width < 720) ? 720 : actual_width;
    
    const uint8_t* y_plane_src = yuv_data.data();
    const uint8_t* u_plane_src = y_plane_src + (actual_width * height);
    const uint8_t* v_plane_src = u_plane_src + ((actual_width / 2) * (height / 2));
    
    // Use common padding and upsampling function (with 4:2:0 chroma subsampling)
    // Neutral values for padding (8-bit input)
    uint8_t neutral_y_src = VideoLoaderUtils::STUDIO_LUMA_MIN_8BIT;
    uint8_t neutral_u_src = VideoLoaderUtils::STUDIO_CHROMA_NEUTRAL_8BIT;
    uint8_t neutral_v_src = VideoLoaderUtils::STUDIO_CHROMA_NEUTRAL_8BIT;
    
    VideoLoaderUtils::pad_and_upsample_yuv_8bit(target_width, actual_width, height, frame,
                                                y_plane_src, u_plane_src, v_plane_src,
                                                2, 2,  // 4:2:0 subsampling
                                                neutral_y_src, neutral_u_src, neutral_v_src);
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
    std::string format_error;
    if (!VideoLoaderUtils::validate_frame_rate(frame_rate_, system, 0.1)) {
        error_message = "MP4 frame rate mismatch: expected " + 
                       std::to_string(VideoLoaderUtils::get_expected_frame_rate(system)) + 
                       " fps for " + (system == VideoSystem::PAL ? "PAL" : "NTSC") +
                       ", got " + std::to_string(frame_rate_) + " fps";
        return false;
    }
    return true;
}


} // namespace encode_orc
