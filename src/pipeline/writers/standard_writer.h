/*
 * File:        standard_writer.h
 * Module:      encode-orc
 * Purpose:     Standard raw field writer (no padding, no metadata)
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: 2026 Simon Inns
 */

#ifndef ENCODE_ORC_STANDARD_WRITER_H
#define ENCODE_ORC_STANDARD_WRITER_H

#include "writer.h"
#include "field.h"
#include <string>
#include <fstream>
#include <cstdint>

namespace encode_orc {

/**
 * @brief Writer for standard raw field files (no padding, no metadata)
 * 
 * Outputs raw field-based video data as 16-bit signed samples in little-endian format.
 * Unsigned samples are converted to signed by subtracting 32767 (e.g., 0 → -32767).
 * Fields are written exactly as generated (asymmetric sizes), with no padding or metadata.
 * 
 * This is useful for:
 * - Direct field data export matching video standards exactly
 * - Compatibility with other video processing tools
 * - Minimizing file size when metadata is not needed
 */
class StandardWriter : public Writer {
public:
    /**
     * @brief Construct a standard writer
     */
    StandardWriter() = default;
    
    /**
     * @brief Destructor - ensures file is closed
     */
    ~StandardWriter() override {
        close();
    }
    
    // Disable copy
    StandardWriter(const StandardWriter&) = delete;
    StandardWriter& operator=(const StandardWriter&) = delete;
    
    /**
     * @brief Open a standard file for writing
     * @param filename Path to file to create
     * @return true on success, false on failure
     */
    bool open(const std::string& filename) override {
        close();
        
        file_.open(filename, std::ios::binary | std::ios::trunc);
        if (!file_) {
            return false;
        }
        
        filename_ = filename;
        return true;
    }
    
    /**
     * @brief Close the file
     */
    void close() override {
        if (file_.is_open()) {
            file_.close();
        }
    }
    
    /**
     * @brief Check if file is open
     */
    bool is_open() const override {
        return file_.is_open();
    }
    
    /**
     * @brief Get filename
     */
    const std::string& filename() const override {
        return filename_;
    }
    
    /**
     * @brief Write a field to the file
     * 
     * Writes the field exactly as-is without any padding.
     * Unsigned samples are converted to signed by subtracting 32767.
     * Fields will have asymmetric sizes (field1 shorter than field2).
     * 
     * @param field Field data to write
     * @return true on success, false on failure
     */
    bool write_field(const Field& field) override {
        if (!file_.is_open()) {
            return false;
        }
        
        // Convert unsigned samples to signed and write in little-endian format
        const auto& data = field.data();
        for (uint16_t sample : data) {
            // Convert unsigned to signed by subtracting 32767
            int16_t signed_sample = static_cast<int16_t>(sample - 32767);
            
            // Write low byte then high byte (little-endian)
            uint8_t low = signed_sample & 0xFF;
            uint8_t high = (signed_sample >> 8) & 0xFF;
            file_.write(reinterpret_cast<const char*>(&low), 1);
            file_.write(reinterpret_cast<const char*>(&high), 1);
        }
        
        return file_.good();
    }
    
    /**
     * @brief Get current file position
     */
    int64_t tell() const {
        if (!file_.is_open()) {
            return -1;
        }
        return const_cast<std::ofstream&>(file_).tellp();
    }

private:
    std::ofstream file_;
    std::string filename_;
};

}  // namespace encode_orc

#endif  // ENCODE_ORC_STANDARD_WRITER_H
