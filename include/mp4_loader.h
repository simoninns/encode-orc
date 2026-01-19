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
#include "video_loader_base.h"
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
 * Uses ffmpeg command-line tool to decode video frames to raw YUV420P format,
 * which is then converted to YUV444P16 for the encoder.
 * 
 * Supported formats:
 * - H.264/AVC encoded MP4 files
 * - H.265/HEVC encoded MP4 files
 * - Any other codec supported by ffmpeg that can be decoded to YUV420P
 */
class MP4Loader : public VideoLoaderBase {
public:
    /**
     * @brief Open an MP4 file and prepare for frame extraction
     * 
     * @param filename Path to MP4 file
     * @param error_message Error message if opening fails
     * @return true on success, false on error
     */
    bool open(const std::string& filename, std::string& error_message);
    
    /**
     * @brief Get video dimensions from the opened file
     * 
     * @param width Output: video width
     * @param height Output: video height
     * @return true if dimensions are available, false otherwise
     */
    bool get_dimensions(int32_t& width, int32_t& height) const;
    
    /**
     * @brief Get total number of frames in the video
     * 
     * @return Total frame count, or -1 if unknown
     */
    int32_t get_frame_count() const;
    
    /**
     * @brief Load a specific frame from the MP4 file
     * 
     * @param frame_number Frame number to load (0-indexed)
     * @param expected_width Expected width (validates against video dimensions)
     * @param expected_height Expected height (validates against video dimensions)
     * @param params Video parameters (for metadata, not conversion)
     * @param frame Output frame buffer in YUV444P16 format
     * @param error_message Error message if loading fails
     * @return true on success, false on error
     */
    bool load_frame(int32_t frame_number,
                    int32_t expected_width,
                    int32_t expected_height,
                    const VideoParameters& params,
                    FrameBuffer& frame,
                    std::string& error_message);
    
    /**
     * @brief Load multiple consecutive frames from the MP4 file
     * 
     * @param start_frame Starting frame number (0-indexed)
     * @param num_frames Number of frames to load
     * @param expected_width Expected width
     * @param expected_height Expected height
     * @param params Video parameters
     * @param frames Output vector of frame buffers
     * @param error_message Error message if loading fails
     * @return true on success, false on error
     */
    bool load_frames(int32_t start_frame,
                     int32_t num_frames,
                     int32_t expected_width,
                     int32_t expected_height,
                     const VideoParameters& params,
                     std::vector<FrameBuffer>& frames,
                     std::string& error_message);
    
    /**
     * @brief Close the MP4 file
     */
    void close();
    
    /**
     * @brief Validate MP4 format compatibility with video system
     * 
     * @param system Video system (PAL or NTSC)
     * @param error_message Error/warning message if validation fails
     * @return true if format is compatible, false otherwise
     */
    bool validate_format(VideoSystem system, std::string& error_message);
    
    /**
     * @brief Check if loader is open
     */
    bool is_open() const override { return is_open_; }

private:
    std::string filename_;

    
    /**
     * @brief Get video information using ffprobe
     * 
     * @param error_message Error message if probe fails
     * @return true on success, false on error
     */
    bool probe_video_info(std::string& error_message);
    
    /**
     * @brief Extract frames using ffmpeg to temporary YUV file
     * 
     * @param start_frame Starting frame number
     * @param num_frames Number of frames
     * @param temp_yuv_file Path to temporary YUV file
     * @param error_message Error message if extraction fails
     * @return true on success, false on error
     */
    bool extract_frames_to_yuv(int32_t start_frame,
                               int32_t num_frames,
                               const std::string& temp_yuv_file,
                               std::string& error_message);
    
    /**
     * @brief Convert YUV420P planar data to YUV444P16 frame buffer
     * 
     * @param yuv_data Raw YUV420P data
     * @param width Frame width
     * @param height Frame height
     * @param frame Output frame buffer
     */
    void convert_yuv420p_to_frame(const std::vector<uint8_t>& yuv_data,
                                  int32_t width,
                                  int32_t height,
                                  FrameBuffer& frame);
};

} // namespace encode_orc

#endif // ENCODE_ORC_MP4_LOADER_H
