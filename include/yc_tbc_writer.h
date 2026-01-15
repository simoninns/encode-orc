/*
 * File:        yc_tbc_writer.h
 * Module:      encode-orc
 * Purpose:     Separate Y/C TBC file writers for separate luma/chroma output
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: 2026 Simon Inns
 */

#ifndef ENCODE_ORC_YC_TBC_WRITER_H
#define ENCODE_ORC_YC_TBC_WRITER_H

#include "field.h"
#include "tbc_writer.h"
#include <string>
#include <memory>

namespace encode_orc {

/**
 * @brief Writer for separate Y and C TBC files
 * 
 * This class manages two separate TBC files:
 * - Modern mode: .tbcy for luma (Y) component, .tbcc for chroma (C) component
 * - Legacy mode: .tbc for luma, _chroma.tbc for chroma component
 */
class YCTBCWriter {
public:
    /**
     * @brief Naming convention modes for Y/C files
     */
    enum class NamingMode {
        MODERN,  // .tbcy and .tbcc extensions
        LEGACY   // .tbc and _chroma.tbc suffixes
    };
    
    /**
     * @brief Construct a Y/C TBC writer
     * @param mode Naming convention to use (default: MODERN)
     */
    YCTBCWriter(NamingMode mode = NamingMode::MODERN) : naming_mode_(mode) {}
    
    /**
     * @brief Destructor - ensures files are closed
     */
    ~YCTBCWriter() {
        close();
    }
    
    // Disable copy
    YCTBCWriter(const YCTBCWriter&) = delete;
    YCTBCWriter& operator=(const YCTBCWriter&) = delete;
    
    /**
     * @brief Open Y and C TBC files for writing
     * @param base_filename Base path without extension (e.g., "output/video")
     * @return true on success, false on failure
     */
    bool open(const std::string& base_filename) {
        close();
        
        // Determine file extensions based on naming mode
        std::string y_filename;
        std::string c_filename;
        
        if (naming_mode_ == NamingMode::LEGACY) {
            // Legacy: base.tbc and base_chroma.tbc
            y_filename = base_filename + ".tbc";
            c_filename = base_filename + "_chroma.tbc";
        } else {
            // Modern: base.tbcy and base.tbcc
            y_filename = base_filename + ".tbcy";
            c_filename = base_filename + ".tbcc";
        }
        
        y_writer_ = std::make_unique<TBCWriter>();
        c_writer_ = std::make_unique<TBCWriter>();
        
        if (!y_writer_->open(y_filename)) {
            return false;
        }
        
        if (!c_writer_->open(c_filename)) {
            y_writer_->close();
            return false;
        }
        
        base_filename_ = base_filename;
        return true;
    }
    
    /**
     * @brief Close both Y and C TBC files
     */
    void close() {
        if (y_writer_) {
            y_writer_->close();
        }
        if (c_writer_) {
            c_writer_->close();
        }
    }
    
    /**
     * @brief Check if files are open
     */
    bool is_open() const {
        return y_writer_ && y_writer_->is_open() && 
               c_writer_ && c_writer_->is_open();
    }
    
    /**
     * @brief Get base filename
     */
    const std::string& base_filename() const {
        return base_filename_;
    }
    
    /**
     * @brief Write Y field to luma TBC file
     * @param field Y field data to write
     * @return true on success, false on failure
     */
    bool write_y_field(const Field& field) {
        if (!y_writer_ || !y_writer_->is_open()) {
            return false;
        }
        return y_writer_->write_field(field);
    }
    
    /**
     * @brief Write C field to chroma TBC file
     * @param field C field data to write
     * @return true on success, false on failure
     */
    bool write_c_field(const Field& field) {
        if (!c_writer_ || !c_writer_->is_open()) {
            return false;
        }
        return c_writer_->write_field(field);
    }
    
    /**
     * @brief Get Y writer
     */
    TBCWriter* y_writer() { return y_writer_.get(); }
    
    /**
     * @brief Get C writer
     */
    TBCWriter* c_writer() { return c_writer_.get(); }

private:
    std::unique_ptr<TBCWriter> y_writer_;
    std::unique_ptr<TBCWriter> c_writer_;
    std::string base_filename_;
    NamingMode naming_mode_ = NamingMode::MODERN;
};

} // namespace encode_orc

#endif // ENCODE_ORC_YC_TBC_WRITER_H
