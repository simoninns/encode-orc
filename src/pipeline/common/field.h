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
#include <memory>
#include <utility>

namespace encode_orc {

/**
 * @brief Represents a single interlaced video field
 * 
 * A field can contain:
 * - Composite representation: Y+C combined (always present)
 * - Separate Y/C representations: separate Y and C fields (optional, for Y/C output)
 * 
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
     * @brief Check if this field has audio samples attached
     */
    bool has_audio() const {
        return !audio_.empty();
    }

    /**
     * @brief Get audio samples (interleaved stereo, 16-bit PCM)
     */
    std::vector<int16_t>& audio() {
        return audio_;
    }

    /**
     * @brief Get audio samples (const)
     */
    const std::vector<int16_t>& audio() const {
        return audio_;
    }

    /**
     * @brief Set audio samples (copy)
     */
    void set_audio(const std::vector<int16_t>& audio) {
        audio_ = audio;
    }

    /**
     * @brief Set audio samples (move)
     */
    void set_audio(std::vector<int16_t>&& audio) {
        audio_ = std::move(audio);
    }

    /**
     * @brief Clear audio samples
     */
    void clear_audio() {
        audio_.clear();
    }
    
    /**
     * @brief Check if this field has separate Y/C representations
     * @return true if Y and C fields are available
     */
    bool has_separate_yc() const {
        return y_field_ != nullptr && c_field_ != nullptr;
    }
    
    /**
     * @brief Get or create separate Y representation
     * @return Reference to Y field
     */
    Field& y_field() {
        if (!y_field_) {
            y_field_ = std::make_unique<Field>(width_, height_);
        }
        return *y_field_;
    }
    
    /**
     * @brief Get Y field (const)
     * @return Const pointer to Y field if available, nullptr otherwise
     */
    const Field* y_field_const() const {
        return y_field_.get();
    }
    
    /**
     * @brief Get or create separate C representation
     * @return Reference to C field
     */
    Field& c_field() {
        if (!c_field_) {
            c_field_ = std::make_unique<Field>(width_, height_);
        }
        return *c_field_;
    }
    
    /**
     * @brief Get C field (const)
     * @return Const pointer to C field if available, nullptr otherwise
     */
    const Field* c_field_const() const {
        return c_field_.get();
    }
    
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

    // Optional interleaved stereo PCM audio samples (16-bit)
    std::vector<int16_t> audio_;
    
    // Optional separate Y and C representations (for Y/C output)
    // Using unique_ptr to avoid forward declaration issues
    std::unique_ptr<Field> y_field_;
    std::unique_ptr<Field> c_field_;
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
