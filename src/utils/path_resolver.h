/*
 * File:        path_resolver.h
 * Module:      encode-orc
 * Purpose:     Path resolution utilities for YAML project files
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: 2026 Simon Inns
 */

#ifndef ENCODE_ORC_PATH_RESOLVER_H
#define ENCODE_ORC_PATH_RESOLVER_H

#include <string>
#include <filesystem>

namespace encode_orc {

/**
 * @brief Path resolver for YAML project files
 * 
 * Resolves relative paths relative to the YAML file's directory (project root),
 * not the current working directory. This ensures project portability.
 */
class PathResolver {
public:
    /**
     * @brief Construct a path resolver
     * 
     * @param yaml_file_path Path to the YAML project file (can be relative or absolute)
     */
    explicit PathResolver(const std::string& yaml_file_path);

    /**
     * @brief Resolve a path from the YAML file
     * 
     * Resolution rules:
     * - If path is absolute, return it unchanged
     * - If path contains ${ENCODE_ORC_ASSETS}, expand it to the assets directory
     * - If path contains ${ENCODE_ORC_OUTPUT_ROOT}, expand it to the output root directory
     * - If path contains ${PROJECT_ROOT}, expand it to the project root directory
     * - If path is relative, resolve it relative to the project root
     * 
     * @param yaml_path Path as specified in the YAML file
     * @return Absolute, normalized path
     */
    std::string resolve(const std::string& yaml_path) const;

    /**
     * @brief Get the project root directory (directory containing the YAML file)
     * 
     * @return Absolute path to the project root directory
     */
    const std::filesystem::path& get_project_root() const { return project_root_; }

    /**
     * @brief Get the absolute path to the YAML file
     * 
     * @return Absolute path to the YAML file
     */
    const std::filesystem::path& get_yaml_file_path() const { return yaml_file_absolute_; }

private:
    std::filesystem::path yaml_file_absolute_;  // Absolute path to YAML file
    std::filesystem::path project_root_;        // Directory containing the YAML file
};

} // namespace encode_orc

#endif // ENCODE_ORC_PATH_RESOLVER_H
