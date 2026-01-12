/*
 * File:        tbc_writer.h
 * Module:      encode-orc
 * Purpose:     TBC file writer for field-based video data
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: 2026 Simon Inns
 */

#ifndef ENCODE_ORC_TBC_WRITER_H
#define ENCODE_ORC_TBC_WRITER_H

#include "field.h"
#include <string>
#include <fstream>
#include <cstdint>

namespace encode_orc {

/**
 * @brief Writer for TBC (Time Base Corrected) video files
 * 
 * TBC files contain raw field-based composite video data as 16-bit
 * unsigned samples in little-endian format.
 */
class TBCWriter {
public:
    /**
     * @brief Construct a TBC writer
     */
    TBCWriter() = default;
    
    /**
     * @brief Destructor - ensures file is closed
     */
    ~TBCWriter() {
        close();
    }
    
    // Disable copy
    TBCWriter(const TBCWriter&) = delete;
    TBCWriter& operator=(const TBCWriter&) = delete;
    
    /**
     * @brief Open a TBC file for writing
     * @param filename Path to TBC file to create
     * @return true on success, false on failure
     */
    bool open(const std::string& filename) {
        close();
        
        file_.open(filename, std::ios::binary | std::ios::trunc);
        if (!file_) {
            return false;
        }
        
        filename_ = filename;
        return true;
    }
    
    /**
     * @brief Close the TBC file
     */
    void close() {
        if (file_.is_open()) {
            file_.close();
        }
    }
    
    /**
     * @brief Check if file is open
     */
    bool is_open() const {
        return file_.is_open();
    }
    
    /**
     * @brief Get filename
     */
    const std::string& filename() const {
        return filename_;
    }
    
    /**
     * @brief Write a field to the TBC file
     * @param field Field data to write
     * @return true on success, false on failure
     */
    bool write_field(const Field& field) {
        if (!file_.is_open()) {
            return false;
        }
        
        // Write 16-bit samples in little-endian format
        const auto& data = field.data();
        for (uint16_t sample : data) {
            // Write low byte then high byte (little-endian)
            uint8_t low = sample & 0xFF;
            uint8_t high = (sample >> 8) & 0xFF;
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

} // namespace encode_orc

#endif // ENCODE_ORC_TBC_WRITER_H
