/*
 * File:        writer.h
 * Module:      encode-orc
 * Purpose:     Abstract base class for field writers
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: 2026 Simon Inns
 */

#ifndef ENCODE_ORC_WRITER_H
#define ENCODE_ORC_WRITER_H

#include "field.h"
#include <string>
#include <memory>

namespace encode_orc {

/**
 * @brief Abstract base class for video field writers
 * 
 * Defines the interface for writing video field data to files.
 * Different implementations can handle TBC format (with padding),
 * standard format (no padding), or separate Y/C formats.
 */
class Writer {
public:
    /**
     * @brief Virtual destructor
     */
    virtual ~Writer() = default;

    /**
     * @brief Open a file for writing
     * @param filename Path to file to create
     * @return true on success, false on failure
     */
    virtual bool open(const std::string& filename) = 0;

    /**
     * @brief Close the file
     */
    virtual void close() = 0;

    /**
     * @brief Check if file is open
     * @return true if file is open and ready for writing
     */
    virtual bool is_open() const = 0;

    /**
     * @brief Write a field to the file
     * @param field Field data to write
     * @return true on success, false on failure
     */
    virtual bool write_field(const Field& field) = 0;

    /**
     * @brief Get filename
     * @return The filename associated with this writer (if applicable)
     */
    virtual const std::string& filename() const = 0;
};

} // namespace encode_orc

#endif // ENCODE_ORC_WRITER_H
