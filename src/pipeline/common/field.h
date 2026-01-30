/*
 * File:        field.h
 * Module:      encode-orc
 * Purpose:     Field and frame data structures
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: 2026 Simon Inns
 */

#ifndef ENCODE_ORC_FIELD_H
#define ENCODE_ORC_FIELD_H

#include <cstdint>
#include <vector>

namespace encode_orc {

/**
 * @brief Represents a single interlaced video field
 * 
 * A field contains one set of scan lines from an interlaced video frame.
 * Fields are stored as 16-bit unsigned samples.
 */
class Field {
public:
    /**
     * @brief Construct an empty field
     */
    Field() = default;
    
    /**
     * @brief Construct a field with specified dimensions
     * @param width Field width in samples
     * @param height Field height in lines
     */
    Field(int32_t width, int32_t height)
        : width_(width), height_(height), data_(width * height, 0) {}
    
    /**
     * @brief Get field width in samples
     */
    int32_t width() const { return width_; }
    
    /**
     * @brief Get field height in lines
     */
    int32_t height() const { return height_; }
    
    /**
     * @brief Get total number of samples
     */
    size_t size() const { return data_.size(); }
    
    /**
     * @brief Access sample data (mutable)
     */
    std::vector<uint16_t>& data() { return data_; }
    
    /**
     * @brief Access sample data (const)
     */
    const std::vector<uint16_t>& data() const { return data_; }
    
    /**
     * @brief Get pointer to raw data for a specific line
     * @param line Line number (0-indexed)
     */
    uint16_t* line_data(int32_t line) {
        return &data_[line * width_];
    }
    
    /**
     * @brief Get const pointer to raw data for a specific line
     * @param line Line number (0-indexed)
     */
    const uint16_t* line_data(int32_t line) const {
        return &data_[line * width_];
    }
    
    /**
     * @brief Set a sample value
     * @param x Horizontal position (0-indexed)
     * @param y Vertical position (line number, 0-indexed)
     * @param value Sample value
     */
    void set_sample(int32_t x, int32_t y, uint16_t value) {
        data_[y * width_ + x] = value;
    }
    
    /**
     * @brief Get a sample value
     * @param x Horizontal position (0-indexed)
     * @param y Vertical position (line number, 0-indexed)
     */
    uint16_t get_sample(int32_t x, int32_t y) const {
        return data_[y * width_ + x];
    }
    
    /**
     * @brief Fill entire field with a constant value
     * @param value Value to fill with
     */
    void fill(uint16_t value) {
        std::fill(data_.begin(), data_.end(), value);
    }
    
    /**
     * @brief Resize the field
     * @param width New width in samples
     * @param height New height in lines
     */
    void resize(int32_t width, int32_t height) {
        width_ = width;
        height_ = height;
        data_.resize(width * height);
    }
    
    /**
     * @brief Clear the field (set all samples to 0)
     */
    void clear() {
        std::fill(data_.begin(), data_.end(), 0);
    }

private:
    int32_t width_ = 0;
    int32_t height_ = 0;
    std::vector<uint16_t> data_;
};

/**
 * @brief Represents a complete interlaced video frame (two fields)
 */
class Frame {
public:
    /**
     * @brief Construct an empty frame
     */
    Frame() = default;
    
    /**
     * @brief Construct a frame with two fields of specified dimensions
     * @param width Field width in samples
     * @param height Field height in lines
     */
    Frame(int32_t width, int32_t height)
        : field1_(width, height), field2_(width, height) {}
    
    /**
     * @brief Get first field (mutable)
     */
    Field& field1() { return field1_; }
    
    /**
     * @brief Get first field (const)
     */
    const Field& field1() const { return field1_; }
    
    /**
     * @brief Get second field (mutable)
     */
    Field& field2() { return field2_; }
    
    /**
     * @brief Get second field (const)
     */
    const Field& field2() const { return field2_; }
    
    /**
     * @brief Resize both fields
     * @param width New width in samples
     * @param height New height in lines
     */
    void resize(int32_t width, int32_t height) {
        field1_.resize(width, height);
        field2_.resize(width, height);
    }
    
    /**
     * @brief Fill both fields with a constant value
     * @param value Value to fill with
     */
    void fill(uint16_t value) {
        field1_.fill(value);
        field2_.fill(value);
    }

private:
    Field field1_;
    Field field2_;
};

} // namespace encode_orc

#endif // ENCODE_ORC_FIELD_H
