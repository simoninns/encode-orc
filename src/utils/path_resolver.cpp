/*
 * File:        path_resolver.cpp
 * Module:      encode-orc
 * Purpose:     Path resolution utilities for YAML project files
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: 2026 Simon Inns
 */

#include "path_resolver.h"
#include <stdexcept>
#include <regex>
#include <cstdlib>

namespace encode_orc {

PathResolver::PathResolver(const std::string& yaml_file_path) {
    // Convert to absolute path
    yaml_file_absolute_ = std::filesystem::absolute(yaml_file_path);
    
    // Get the parent directory as the project root
    if (yaml_file_absolute_.has_parent_path()) {
        project_root_ = yaml_file_absolute_.parent_path();
    } else {
        // Edge case: YAML file is in root directory
        project_root_ = std::filesystem::path("/");
    }
    
    // Normalize paths (resolve . and .. components)
    yaml_file_absolute_ = yaml_file_absolute_.lexically_normal();
    project_root_ = project_root_.lexically_normal();
}

std::string PathResolver::resolve(const std::string& yaml_path) const {
    if (yaml_path.empty()) {
        return yaml_path;
    }
    
    std::filesystem::path path(yaml_path);
    
    // If the path is absolute, return it unchanged
    if (path.is_absolute()) {
        return path.lexically_normal().string();
    }
    
    // Handle ${ENCODE_ORC_ASSETS} variable expansion
    std::string expanded_path = yaml_path;
    const std::string assets_var = "${ENCODE_ORC_ASSETS}";

    size_t pos = expanded_path.find(assets_var);
    if (pos != std::string::npos) {
        const char* assets_env = std::getenv("ENCODE_ORC_ASSETS");
        std::string assets_root;
        if (assets_env && assets_env[0] != '\0') {
            assets_root = assets_env;
        } else {
            assets_root = (project_root_ / ".." / "assets").lexically_normal().string();
        }

        expanded_path.replace(pos, assets_var.length(), assets_root);

        // After replacement, the path might be absolute already
        path = std::filesystem::path(expanded_path);
        if (path.is_absolute()) {
            return path.lexically_normal().string();
        }
    }

    // Handle ${ENCODE_ORC_OUTPUT_ROOT} variable expansion
    const std::string output_root_var = "${ENCODE_ORC_OUTPUT_ROOT}";

    pos = expanded_path.find(output_root_var);
    if (pos != std::string::npos) {
        const char* output_env = std::getenv("ENCODE_ORC_OUTPUT_ROOT");
        std::string output_root;
        if (output_env && output_env[0] != '\0') {
            output_root = output_env;
        } else {
            std::filesystem::path parent_root = project_root_.parent_path();
            std::string project_dir = project_root_.filename().string();
            if (project_dir == "test-projects") {
                output_root = (parent_root / "test-output").lexically_normal().string();
            } else if (project_dir == "ggv-tests") {
                output_root = (parent_root / "ggv-output").lexically_normal().string();
            } else {
                output_root = (parent_root / "output").lexically_normal().string();
            }
        }

        expanded_path.replace(pos, output_root_var.length(), output_root);

        // After replacement, the path might be absolute already
        path = std::filesystem::path(expanded_path);
        if (path.is_absolute()) {
            return path.lexically_normal().string();
        }
    }

    // Handle ${PROJECT_ROOT} variable expansion
    const std::string project_root_var = "${PROJECT_ROOT}";
    
    pos = expanded_path.find(project_root_var);
    if (pos != std::string::npos) {
        // Replace ${PROJECT_ROOT} with the actual project root path
        expanded_path.replace(pos, project_root_var.length(), project_root_.string());
        
        // After replacement, the path might be absolute already
        path = std::filesystem::path(expanded_path);
        if (path.is_absolute()) {
            return path.lexically_normal().string();
        }
    }
    
    // For relative paths, resolve relative to project root
    std::filesystem::path resolved = project_root_ / expanded_path;
    
    // Normalize the path (resolve . and .. components)
    return resolved.lexically_normal().string();
}

} // namespace encode_orc
