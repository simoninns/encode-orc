/*
 * File:        mp4_loader.h
 * Module:      encode-orc
 * Purpose:     MP4 file loading utility using ffmpeg/libav
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: 2026 Simon Inns
 */

#ifndef ENCODE_ORC_MP4_LOADER_H
#define ENCODE_ORC_MP4_LOADER_H

#include "frame_buffer.h"
#include "video_parameters.h"
#include "video_loader.h"
#include <string>
#include <vector>
#include <memory>

namespace encode_orc {

/**
 * @brief MP4 file loader using ffmpeg
 * 
 * Loads frames from MP4 files in normal color space (typically Rec.709 or similar)
 * and converts them to YUV444P16 frame buffers for video encoding.
 * 
 * Phase 1 Refactoring: Now implements the unified VideoLoader interface.
 * 
 * Uses ffmpeg command-line tool to decode video frames to raw YUV420P format,
 * which is then converted to YUV444P16 for the encoder.
 * 
 * Supported formats:
 * - H.264/AVC encoded MP4 files
 * - H.265/HEVC encoded MP4 files
 * - Any other codec supported by ffmpeg that can be decoded to YUV420P
 */
class MP4Loader : public VideoLoader {
public:
    // VideoLoader interface implementation
    bool open(const std::string& path, std::string& error) override;
    VideoMetadata get_metadata() const override;
    bool load_frame(int32_t frame_index, FrameBuffer& output, std::string& error) override;
    bool load_frames(int32_t start, int32_t count, 
                     std::vector<FrameBuffer>& output, std::string& error) override;
    void close() override;
    bool is_open() const override;
    
    /**
     * @brief Load a frame with validation (legacy method)
     */
    bool load_frame(int32_t frame_number,
                    int32_t expected_width,
                    int32_t expected_height,
                    const VideoParameters& params,
                    FrameBuffer& frame,
                    std::string& error_message);
    
    /**
     * @brief Load multiple frames with validation (legacy method)
     */
    bool load_frames(int32_t start_frame,
                     int32_t num_frames,
                     int32_t expected_width,
                     int32_t expected_height,
                     const VideoParameters& params,
                     std::vector<FrameBuffer>& frames,
                     std::string& error_message);

private:
    std::string filename_;
    int32_t width_ = 0;
    int32_t height_ = 0;
    int32_t frame_count_ = 0;
    double frame_rate_ = 0.0;
    bool is_open_ = false;
    
    /**
     * @brief Get video information using ffprobe
     */
    bool probe_video_info(std::string& error_message);
    
    /**
     * @brief Extract frames using ffmpeg to temporary YUV file
     */
    bool extract_frames_to_yuv(int32_t start_frame,
                               int32_t num_frames,
                               const std::string& temp_yuv_file,
                               std::string& error_message);
    
    /**
     * @brief Convert YUV420P planar data to YUV444P16 frame buffer
     */
    void convert_yuv420p_to_frame(const std::vector<uint8_t>& yuv_data,
                                  int32_t width,
                                  int32_t height,
                                  FrameBuffer& frame);
};

} // namespace encode_orc

#endif // ENCODE_ORC_MP4_LOADER_H
