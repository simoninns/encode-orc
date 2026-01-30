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

#include "writer.h"
#include "field.h"
#include <string>
#include <fstream>
#include <cstdint>

namespace encode_orc {

/**
 * @brief Writer for TBC (Time Base Corrected) video files
 * 
 * TBC files contain raw field-based composite video data as 16-bit
 * unsigned samples in little-endian format. All fields are padded to
 * the same size to meet TBC format requirements.
 * 
 * Can be used for combined Y+C composite output, or separate Y and C files.
 */
class TBCWriter : public Writer {
public:
    /**
     * @brief Construct a TBC writer
     */
    TBCWriter() = default;
    
    /**
     * @brief Destructor - ensures file is closed
     */
    ~TBCWriter() override {
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
     * @brief Close the TBC file
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
     * @brief Set padding parameters for field1
     * 
     * When set, field1 will be padded with blanking samples to match field2 height.
     * This is called before encoding starts to configure the padding behavior.
     * 
     * @param field_width Number of samples per line
     * @param blanking_value 16-bit value to use for blanking samples
     * @param field_height_diff Difference in height between field2 and field1
     */
    void set_field1_padding(int32_t field_width, uint16_t blanking_value, int32_t field_height_diff) {
        padding_field_width_ = field_width;
        padding_blanking_value_ = blanking_value;
        padding_field_height_diff_ = field_height_diff;
        next_is_field1_ = true;
    }
    
    /**
     * @brief Write a field to the TBC file, applying padding to field1 if configured
     * 
     * Automatically applies padding to field1 if set_field1_padding was called.
     * Padding is applied by inserting blanking samples after field1 to match field2 height.
     * 
     * @param field Field data to write
     * @return true on success, false on failure
     */
    bool write_field(const Field& field) override {
        if (!file_.is_open()) {
            return false;
        }
        
        // Write the field data
        const auto& data = field.data();
        for (uint16_t sample : data) {
            // Write low byte then high byte (little-endian)
            uint8_t low = sample & 0xFF;
            uint8_t high = (sample >> 8) & 0xFF;
            file_.write(reinterpret_cast<const char*>(&low), 1);
            file_.write(reinterpret_cast<const char*>(&high), 1);
        }
        
        // Apply padding to field1 if configured
        if (next_is_field1_ && padding_field_height_diff_ > 0) {
            int32_t padding_samples = padding_field_height_diff_ * padding_field_width_;
            for (int32_t i = 0; i < padding_samples; ++i) {
                uint8_t low = padding_blanking_value_ & 0xFF;
                uint8_t high = (padding_blanking_value_ >> 8) & 0xFF;
                file_.write(reinterpret_cast<const char*>(&low), 1);
                file_.write(reinterpret_cast<const char*>(&high), 1);
            }
        }
        
        // Toggle field indicator
        next_is_field1_ = !next_is_field1_;
        
        return file_.good();
    }
    
    /**
     * @brief Write a field to the TBC file with optional blanking line insertion
     * 
     * For first fields that are shorter than expected (e.g., PAL field1=312 vs field2=313),
     * this method can insert a blanking line at the end of the field to match ld-decode format.
     * 
     * @param field Field data to write
     * @param field_width Width of each line in samples
     * @param blanking_value Value to use for blanking line samples
     * @param insert_blanking_line Whether to insert a blanking line at the end
     * @return true on success, false on failure
     */
    bool write_field_with_blanking(const Field& field, int32_t field_width, 
                                    uint16_t blanking_value, bool insert_blanking_line) {
        if (!file_.is_open()) {
            return false;
        }
        
        // First write the field normally
        if (!write_field(field)) {
            return false;
        }
        
        // If requested, append a blanking line
        if (insert_blanking_line) {
            for (int32_t i = 0; i < field_width; ++i) {
                uint8_t low = blanking_value & 0xFF;
                uint8_t high = (blanking_value >> 8) & 0xFF;
                file_.write(reinterpret_cast<const char*>(&low), 1);
                file_.write(reinterpret_cast<const char*>(&high), 1);
            }
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
    
    // Padding configuration for field1
    int32_t padding_field_width_ = 0;
    uint16_t padding_blanking_value_ = 0;
    int32_t padding_field_height_diff_ = 0;
    bool next_is_field1_ = true;
};

} // namespace encode_orc

#endif // ENCODE_ORC_TBC_WRITER_H
