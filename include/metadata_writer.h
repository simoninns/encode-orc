/*
 * File:        metadata_writer.h
 * Module:      encode-orc
 * Purpose:     SQLite metadata database writer for TBC files
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: 2026 Simon Inns
 */

#ifndef ENCODE_ORC_METADATA_WRITER_H
#define ENCODE_ORC_METADATA_WRITER_H

#include "metadata.h"
#include <sqlite3.h>
#include <string>
#include <memory>

namespace encode_orc {

/**
 * @brief Writer for TBC metadata SQLite databases (.tbc.db files)
 * 
 * Implements the ld-decode metadata schema for storing field-level
 * and capture-level metadata.
 */
class MetadataWriter {
public:
    /**
     * @brief Construct a metadata writer
     */
    MetadataWriter() : db_(nullptr) {}
    
    /**
     * @brief Destructor - ensures database is closed
     */
    ~MetadataWriter() {
        close();
    }
    
    // Disable copy
    MetadataWriter(const MetadataWriter&) = delete;
    MetadataWriter& operator=(const MetadataWriter&) = delete;
    
    /**
     * @brief Open/create a metadata database file
     * @param filename Path to .tbc.db file to create
     * @return true on success, false on failure
     */
    bool open(const std::string& filename);
    
    /**
     * @brief Close the database
     */
    void close();
    
    /**
     * @brief Check if database is open
     */
    bool is_open() const {
        return db_ != nullptr;
    }
    
    /**
     * @brief Write complete capture metadata to database
     * @param metadata Capture metadata structure
     * @return true on success, false on failure
     */
    bool write_metadata(const CaptureMetadata& metadata);
    
    /**
     * @brief Get last error message
     */
    const std::string& get_error() const {
        return error_message_;
    }

private:
    /**
     * @brief Create database schema
     */
    bool create_schema();
    
    /**
     * @brief Write capture table
     */
    bool write_capture(const CaptureMetadata& metadata);
    
    /**
     * @brief Write field_record table
     */
    bool write_fields(const CaptureMetadata& metadata);
    
    /**
     * @brief Execute SQL statement
     */
    bool execute_sql(const char* sql);
    
    sqlite3* db_;
    std::string error_message_;
};

} // namespace encode_orc

#endif // ENCODE_ORC_METADATA_WRITER_H
