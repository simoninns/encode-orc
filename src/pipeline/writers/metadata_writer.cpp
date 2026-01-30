/*
 * File:        metadata_writer.cpp
 * Module:      encode-orc
 * Purpose:     SQLite metadata database writer implementation
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: 2026 Simon Inns
 */

#include "metadata_writer.h"
#include <sstream>
#include <cstring>
#include <iostream>

namespace encode_orc {

bool MetadataWriter::open(const std::string& filename) {
    close();
    
    int rc = sqlite3_open(filename.c_str(), &db_);
    if (rc != SQLITE_OK) {
        error_message_ = std::string("Failed to open database: ") + sqlite3_errmsg(db_);
        close();
        return false;
    }
    
    // Create the schema
    if (!create_schema()) {
        return false;
    }
    
    return true;
}

void MetadataWriter::close() {
    if (db_) {
        sqlite3_close(db_);
        db_ = nullptr;
    }
}

bool MetadataWriter::execute_sql(const char* sql) {
    char* err_msg = nullptr;
    int rc = sqlite3_exec(db_, sql, nullptr, nullptr, &err_msg);
    
    if (rc != SQLITE_OK) {
        error_message_ = std::string("SQL error: ") + (err_msg ? err_msg : "unknown");
        if (err_msg) {
            sqlite3_free(err_msg);
        }
        return false;
    }
    
    return true;
}

bool MetadataWriter::create_schema() {
    // Drop existing tables to ensure a clean start
    // This prevents UNIQUE constraint errors when overwriting existing metadata
    const char* drop_sql = R"(
        DROP TABLE IF EXISTS vbi;
        DROP TABLE IF EXISTS field_record;
        DROP TABLE IF EXISTS capture;
    )";
    
    if (!execute_sql(drop_sql)) {
        return false;
    }
    
    const char* schema_sql = R"(
        PRAGMA user_version = 1;
        
        CREATE TABLE capture (
            capture_id INTEGER PRIMARY KEY,
            system TEXT NOT NULL CHECK (system IN ('NTSC','PAL','PAL_M')),
            decoder TEXT NOT NULL,
            git_branch TEXT,
            git_commit TEXT,
            video_sample_rate REAL,
            active_video_start INTEGER,
            active_video_end INTEGER,
            field_width INTEGER,
            field_height INTEGER,
            number_of_sequential_fields INTEGER,
            colour_burst_start INTEGER,
            colour_burst_end INTEGER,
            is_mapped INTEGER CHECK (is_mapped IN (0,1)),
            is_subcarrier_locked INTEGER CHECK (is_subcarrier_locked IN (0,1)),
            is_widescreen INTEGER CHECK (is_widescreen IN (0,1)),
            white_16b_ire INTEGER,
            black_16b_ire INTEGER,
            blanking_16b_ire INTEGER,
            capture_notes TEXT
        );
        
        CREATE TABLE field_record (
            capture_id INTEGER NOT NULL REFERENCES capture(capture_id) ON DELETE CASCADE,
            field_id INTEGER NOT NULL,
            audio_samples INTEGER,
            decode_faults INTEGER,
            disk_loc REAL,
            efm_t_values INTEGER,
            field_phase_id INTEGER,
            file_loc INTEGER,
            is_first_field INTEGER CHECK (is_first_field IN (0,1)),
            median_burst_ire REAL,
            pad INTEGER CHECK (pad IN (0,1)),
            sync_conf INTEGER,
            ntsc_is_fm_code_data_valid INTEGER CHECK (ntsc_is_fm_code_data_valid IN (0,1)),
            ntsc_fm_code_data INTEGER,
            ntsc_field_flag INTEGER CHECK (ntsc_field_flag IN (0,1)),
            ntsc_is_video_id_data_valid INTEGER CHECK (ntsc_is_video_id_data_valid IN (0,1)),
            ntsc_video_id_data INTEGER,
            ntsc_white_flag INTEGER CHECK (ntsc_white_flag IN (0,1)),
            PRIMARY KEY (capture_id, field_id)
        );
        
        CREATE TABLE vbi (
            capture_id INTEGER NOT NULL REFERENCES capture(capture_id) ON DELETE CASCADE,
            field_id INTEGER NOT NULL,
            vbi0 INTEGER,
            vbi1 INTEGER,
            vbi2 INTEGER,
            PRIMARY KEY (capture_id, field_id)
        );
    )";
    
    return execute_sql(schema_sql);
}

bool MetadataWriter::write_capture(const CaptureMetadata& metadata) {
    std::ostringstream sql;
    sql.precision(17);  // Use full double precision for sample rate
    sql << "INSERT INTO capture ("
        << "capture_id, system, decoder, git_branch, git_commit, "
        << "video_sample_rate, active_video_start, active_video_end, "
        << "field_width, field_height, number_of_sequential_fields, "
        << "colour_burst_start, colour_burst_end, "
        << "is_mapped, is_subcarrier_locked, is_widescreen, "
        << "white_16b_ire, black_16b_ire, blanking_16b_ire, capture_notes"
        << ") VALUES ("
        << metadata.capture_id << ", "
        << "'" << video_system_to_string(metadata.video_params.system) << "', "
        << "'" << metadata.video_params.decoder << "', "
        << "'" << metadata.git_branch << "', "
        << "'" << metadata.git_commit << "', "
        << metadata.video_params.sample_rate << ", "
        << metadata.video_params.active_video_start << ", "
        << metadata.video_params.active_video_end << ", "
        << metadata.video_params.field_width << ", "
        << metadata.video_params.field_height << ", "
        << metadata.video_params.number_of_sequential_fields << ", "
        << metadata.video_params.colour_burst_start << ", "
        << metadata.video_params.colour_burst_end << ", "
        << (metadata.video_params.is_mapped ? 1 : 0) << ", "
        << (metadata.video_params.is_subcarrier_locked ? 1 : 0) << ", "
        << (metadata.video_params.is_widescreen ? 1 : 0) << ", "
        << metadata.video_params.white_16b_ire << ", "
        << metadata.video_params.black_16b_ire << ", "
        << metadata.video_params.blanking_16b_ire << ", "
        << "'" << metadata.capture_notes << "'"
        << ");";
    
    return execute_sql(sql.str().c_str());
}

bool MetadataWriter::write_fields(const CaptureMetadata& metadata) {
    // Use a transaction for better performance
    if (!execute_sql("BEGIN TRANSACTION;")) {
        return false;
    }
    
    for (const auto& field : metadata.fields) {
        std::ostringstream sql;
        sql << "INSERT INTO field_record ("
            << "capture_id, field_id, audio_samples, decode_faults, disk_loc, "
            << "efm_t_values, field_phase_id, file_loc, is_first_field, "
            << "median_burst_ire, pad, sync_conf";
        
        // Add NTSC-specific fields if present
        bool has_ntsc = field.ntsc_field_flag.has_value();
        if (has_ntsc) {
            sql << ", ntsc_is_fm_code_data_valid, ntsc_fm_code_data, "
                << "ntsc_field_flag, ntsc_is_video_id_data_valid, "
                << "ntsc_video_id_data, ntsc_white_flag";
        }
        
        sql << ") VALUES ("
            << metadata.capture_id << ", "
            << field.field_id << ", "
            << field.audio_samples << ", "
            << field.decode_faults << ", "
            << field.disk_loc << ", "
            << field.efm_t_values << ", "
            << field.field_phase_id << ", "
            << field.file_loc << ", "
            << (field.is_first_field ? 1 : 0) << ", "
            << field.median_burst_ire << ", "
            << (field.pad ? 1 : 0) << ", "
            << field.sync_conf;
        
        if (has_ntsc) {
            sql << ", "
                << (field.ntsc_is_fm_code_data_valid.value() ? 1 : 0) << ", "
                << field.ntsc_fm_code_data.value_or(0) << ", "
                << (field.ntsc_field_flag.value() ? 1 : 0) << ", "
                << (field.ntsc_is_video_id_data_valid.value() ? 1 : 0) << ", "
                << field.ntsc_video_id_data.value_or(0) << ", "
                << (field.ntsc_white_flag.value() ? 1 : 0);
        }
        
        sql << ");";
        
        if (!execute_sql(sql.str().c_str())) {
            execute_sql("ROLLBACK;");
            return false;
        }
    }
    
    return execute_sql("COMMIT;");
}

bool MetadataWriter::write_vbi(const CaptureMetadata& metadata) {
    // Only write VBI data if it exists
    if (metadata.vbi_data.empty()) {
        return true;  // No VBI data, but not an error
    }
    
    // Use a transaction for better performance
    if (!execute_sql("BEGIN TRANSACTION;")) {
        return false;
    }
    
    for (size_t field_id = 0; field_id < metadata.vbi_data.size(); ++field_id) {
        const auto& vbi = metadata.vbi_data[field_id];
        
        // Skip fields without VBI data
        if (!vbi.has_value()) {
            continue;
        }
        
        std::ostringstream sql;
        sql << "INSERT INTO vbi ("
            << "capture_id, field_id, vbi0, vbi1, vbi2"
            << ") VALUES ("
            << metadata.capture_id << ", "
            << field_id << ", "
            << vbi->vbi0 << ", "
            << vbi->vbi1 << ", "
            << vbi->vbi2
            << ");";
        
        if (!execute_sql(sql.str().c_str())) {
            execute_sql("ROLLBACK;");
            return false;
        }
    }
    
    return execute_sql("COMMIT;");
}

bool MetadataWriter::write_metadata(const CaptureMetadata& metadata) {
    if (!db_) {
        error_message_ = "Database not open";
        return false;
    }
    
    // Write capture record
    if (!write_capture(metadata)) {
        return false;
    }
    
    // Write field records
    if (!write_fields(metadata)) {
        return false;
    }
    
    // Write VBI records if present
    if (!write_vbi(metadata)) {
        return false;
    }
    
    return true;
}

} // namespace encode_orc
