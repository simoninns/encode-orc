cmake_minimum_required(VERSION 3.15)
project(encode-orc VERSION 0.1.0 LANGUAGES CXX)

# Require C++17
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Compiler flags
if(MSVC)
    add_compile_options(/W4 /WX)
else()
    add_compile_options(-Wall -Wextra -Wpedantic -Werror)
endif()

# Enable all spdlog levels at compile time (runtime filtering still applies)
add_compile_definitions(SPDLOG_ACTIVE_LEVEL=SPDLOG_LEVEL_TRACE)

# Enable testing
enable_testing()

# Find SQLite3
find_package(SQLite3 REQUIRED)

# Find yaml-cpp
find_package(yaml-cpp REQUIRED)

# Find libpng
find_package(PNG REQUIRED)

# Find spdlog
find_package(spdlog REQUIRED)

# Get git commit hash
execute_process(
    COMMAND git rev-parse --short HEAD
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    OUTPUT_VARIABLE GIT_COMMIT_HASH
    OUTPUT_STRIP_TRAILING_WHITESPACE
    ERROR_QUIET
)

# Check if git repo is dirty (has uncommitted changes)
execute_process(
    COMMAND git diff-index --quiet HEAD --
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    RESULT_VARIABLE GIT_IS_DIRTY
    ERROR_QUIET
)

# If git command fails, use "unknown"
if(NOT GIT_COMMIT_HASH)
    set(GIT_COMMIT_HASH "unknown")
else()
    # Append "-dirty" if there are uncommitted changes
    if(GIT_IS_DIRTY)
        set(GIT_COMMIT_HASH "${GIT_COMMIT_HASH}-dirty")
    endif()
endif()

# Generate version header
configure_file(
    ${CMAKE_SOURCE_DIR}/include/version.h.in
    ${CMAKE_BINARY_DIR}/generated/version.h
    @ONLY
)

# Include directories
include_directories(${PROJECT_SOURCE_DIR}/include)
include_directories(${CMAKE_BINARY_DIR}/generated)

# Source files
set(SOURCES
    src/main.cpp
    src/logging.cpp
    src/yaml_config.cpp
    src/metadata_writer.cpp
    src/metadata_generator.cpp
    src/color_burst_generator.cpp
    src/pal_encoder.cpp
    src/pal_vits_generator.cpp
    src/ntsc_encoder.cpp
    src/ntsc_vits_generator.cpp
    src/biphase_encoder.cpp
    src/manchester_encoder.cpp
    src/vitc_generator.cpp
    src/color_conversion.cpp
    src/video_loader_base.cpp
    src/yuv422_loader.cpp
    src/png_loader.cpp
    src/mov_loader.cpp
    src/mp4_loader.cpp
    src/video_encoder.cpp
)

# Create executable
add_executable(encode-orc ${SOURCES})

# Link libraries
target_link_libraries(encode-orc PRIVATE SQLite::SQLite3 yaml-cpp PNG::PNG spdlog::spdlog)

# Install target
install(TARGETS encode-orc DESTINATION bin)
