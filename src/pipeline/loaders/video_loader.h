/*
 * File:        video_loader.h
 * Module:      encode-orc
 * Purpose:     Unified interface for all video loaders
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: 2026 Simon Inns
 */

#ifndef ENCODE_ORC_VIDEO_LOADER_H
#define ENCODE_ORC_VIDEO_LOADER_H

#include "frame_buffer.h"
#include "video_parameters.h"
#include <cstdint>
#include <string>
#include <vector>

namespace encode_orc {

/**
 * @brief Video color space enumeration
 */
enum class VideoColorSpace {
    YUV422,      // 4:2:2 YUV
    YUV420,      // 4:2:0 YUV
    YUV444,      // 4:4:4 YUV
    RGB          // RGB color space
};

/**
 * @brief Video bit depth enumeration
 */
enum class VideoBitDepth {
    BIT_8,       // 8-bit per component
    BIT_10,      // 10-bit per component
    BIT_16       // 16-bit per component
};

/**
 * @brief Video metadata structure
 * 
 * Contains information about the video source that can be queried
 * before loading frames.
 */
struct VideoMetadata {
    int32_t width = 0;                     // Frame width in pixels
    int32_t height = 0;                    // Frame height in pixels
    int32_t frame_count = 0;               // Total number of frames
    double frame_rate = 0.0;               // Frame rate in Hz
    VideoColorSpace color_space = VideoColorSpace::YUV444;  // Color space
    VideoBitDepth bit_depth = VideoBitDepth::BIT_10;        // Bit depth per component
};

/**
 * @brief Unified video loader interface
 * 
 * This abstract base class provides a consistent interface for all video
 * loader implementations. All loaders must implement these methods to
 * ensure compatibility with the video encoding pipeline.
 * 
 * Phase 1 Refactoring: This replaces the inconsistent VideoLoaderBase
 * interface with a clean, standardized API.
 */
class VideoLoader {
public:
    virtual ~VideoLoader() = default;
    
    /**
     * @brief Open a video source
     * 
     * Opens the video file or source and prepares it for frame loading.
     * This method should validate the file format and extract metadata.
     * 
     * @param path Path to video file or source identifier
     * @param error Error message on failure
     * @return true on success, false on error
     */
    virtual bool open(const std::string& path, std::string& error) = 0;
    
    /**
     * @brief Get video metadata
     * 
     * Returns metadata about the opened video source. Can only be called
     * after successfully calling open().
     * 
     * @return VideoMetadata structure with source information
     */
    virtual VideoMetadata get_metadata() const = 0;
    
    /**
     * @brief Load a single frame
     * 
     * Loads a single frame from the video source and converts it to
     * YUV444P16 format for the encoder pipeline.
     * 
     * @param frame_index Frame index to load (0-indexed)
     * @param output Output frame buffer (will be allocated/resized as needed)
     * @param error Error message on failure
     * @return true on success, false on error
     */
    virtual bool load_frame(int32_t frame_index, FrameBuffer& output, std::string& error) = 0;
    
    /**
     * @brief Load multiple frames
     * 
     * Loads a range of frames from the video source. This is a batch operation
     * that can be more efficient than calling load_frame() multiple times.
     * 
     * @param start Starting frame index (0-indexed, inclusive)
     * @param count Number of frames to load
     * @param output Output vector of frame buffers
     * @param error Error message on failure
     * @return true on success, false on error
     */
    virtual bool load_frames(int32_t start, int32_t count, 
                             std::vector<FrameBuffer>& output, std::string& error) = 0;
    
    /**
     * @brief Close the video source
     * 
     * Closes the video file and releases all resources. After calling close(),
     * the loader must be reopened before loading more frames.
     */
    virtual void close() = 0;
    
    /**
     * @brief Check if loader is open and ready
     * 
     * @return true if loader is open, false otherwise
     */
    virtual bool is_open() const = 0;
};

} // namespace encode_orc

#endif // ENCODE_ORC_VIDEO_LOADER_H
