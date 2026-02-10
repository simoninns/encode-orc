cmake_minimum_required(VERSION 3.15)
project(encode-orc VERSION 0.1.0 LANGUAGES CXX)

# Allow version override for release builds (when built as part of decode-orc)
if(DEFINED PROJECT_VERSION_OVERRIDE)
    set(PROJECT_VERSION "${PROJECT_VERSION_OVERRIDE}")
    string(REPLACE "v" "" PROJECT_VERSION_CLEAN "${PROJECT_VERSION}")
    set(PROJECT_VERSION "${PROJECT_VERSION_CLEAN}")
    
    # Parse the version string (e.g., "1.0.8")
    string(REGEX MATCH "^([0-9]+)\\.([0-9]+)\\.([0-9]+)" _ "${PROJECT_VERSION}")
    if(CMAKE_MATCH_1)
        set(PROJECT_VERSION_MAJOR "${CMAKE_MATCH_1}")
        set(PROJECT_VERSION_MINOR "${CMAKE_MATCH_2}")
        set(PROJECT_VERSION_PATCH "${CMAKE_MATCH_3}")
    endif()
    
    message(STATUS "encode-orc: Using release version override: ${PROJECT_VERSION}")
endif()

# Require C++17
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Compiler flags
if(MSVC)
    add_compile_options(/W4 /WX)
else()
    add_compile_options(-Wall -Wextra -Wpedantic -Werror)
    # Additional warnings to match macOS strictness
    if(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
        # Clang-specific warnings
        add_compile_options(-Wunused-private-field)
    elseif(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
        # GCC-specific warnings
        add_compile_options(-Wunused-but-set-variable)
    endif()
endif()

# Output directories - only set when built as part of decode-orc (not standalone)
if(NOT CMAKE_PROJECT_NAME STREQUAL PROJECT_NAME)
    # Being built as a subdirectory of decode-orc
    # Match decode-orc's output layout for consistent packaging
    if(CMAKE_CONFIGURATION_TYPES)
        # Multi-config generator (MSVC, Xcode)
        set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_RELEASE ${CMAKE_BINARY_DIR}/bin/Release)
        set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_DEBUG ${CMAKE_BINARY_DIR}/bin/Debug)
        set(CMAKE_LIBRARY_OUTPUT_DIRECTORY_RELEASE ${CMAKE_BINARY_DIR}/lib/Release)
        set(CMAKE_LIBRARY_OUTPUT_DIRECTORY_DEBUG ${CMAKE_BINARY_DIR}/lib/Debug)
    else()
        # Single-config generator (Unix Makefiles, Ninja)
        set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
        set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
    endif()
endif()

# Enable all spdlog levels at compile time (runtime filtering still applies)
add_compile_definitions(SPDLOG_ACTIVE_LEVEL=SPDLOG_LEVEL_TRACE)

# Enable testing
enable_testing()

# =============================================================================
# CTest configuration for YAML project tests
# =============================================================================

set(TEST_PROJECTS_DIR "${CMAKE_CURRENT_SOURCE_DIR}/test-projects")
set(TEST_OUTPUT_DIR "${CMAKE_CURRENT_SOURCE_DIR}/test-output")

if(EXISTS "${TEST_PROJECTS_DIR}")
    file(GLOB TEST_YAML_FILES "${TEST_PROJECTS_DIR}/*.yaml")

    if(TEST_YAML_FILES)
        # Ensure output directory exists (some YAMLs write here)
        file(MAKE_DIRECTORY "${TEST_OUTPUT_DIR}")

        foreach(yaml_file IN LISTS TEST_YAML_FILES)
            get_filename_component(test_name "${yaml_file}" NAME_WE)
            string(REPLACE "-" "_" test_name "${test_name}")

            add_test(
                NAME "encode_orc_${test_name}"
                COMMAND $<TARGET_FILE:encode-orc> "${yaml_file}" --quiet
            )

            set_tests_properties(
                "encode_orc_${test_name}"
                PROPERTIES
                    WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
            )
        endforeach()
    else()
        message(STATUS "encode-orc: No YAML test files found in ${TEST_PROJECTS_DIR}")
    endif()
else()
    message(STATUS "encode-orc: Test projects directory not found at ${TEST_PROJECTS_DIR}")
endif()

# Find SQLite3
find_package(SQLite3 REQUIRED)

# Find yaml-cpp
find_package(yaml-cpp REQUIRED)

# Create an alias if yaml-cpp::yaml-cpp doesn't exist (for compatibility)
if(TARGET yaml-cpp AND NOT TARGET yaml-cpp::yaml-cpp)
    add_library(yaml-cpp::yaml-cpp ALIAS yaml-cpp)
endif()

# Find libpng
find_package(PNG REQUIRED)

# Find spdlog
find_package(spdlog REQUIRED)

# Find Threads (for multi-threading support)
find_package(Threads REQUIRED)

# Find FFmpeg (for MOV/MP4 video loading)
if(WIN32)
    # On Windows, try to find FFmpeg manually first, fall back to pkg-config
    # This handles Chocolatey FFmpeg installations better
    if(DEFINED ENV{FFMPEG_ROOT})
        set(FFMPEG_ROOT $ENV{FFMPEG_ROOT})
    endif()
    
    # Try to find FFmpeg headers and libraries manually
    find_path(AVCODEC_INCLUDE_DIR libavcodec/avcodec.h
        HINTS ${FFMPEG_ROOT}/include ${CMAKE_PREFIX_PATH}/include
        PATH_SUFFIXES include
    )
    
    find_library(AVCODEC_LIBRARY
        NAMES avcodec libavcodec
        HINTS ${FFMPEG_ROOT}/lib ${CMAKE_PREFIX_PATH}/lib ${FFMPEG_ROOT}/bin
        PATH_SUFFIXES lib bin
    )
    
    find_library(AVFORMAT_LIBRARY
        NAMES avformat libavformat
        HINTS ${FFMPEG_ROOT}/lib ${CMAKE_PREFIX_PATH}/lib ${FFMPEG_ROOT}/bin
        PATH_SUFFIXES lib bin
    )
    
    find_library(AVUTIL_LIBRARY
        NAMES avutil libavutil
        HINTS ${FFMPEG_ROOT}/lib ${CMAKE_PREFIX_PATH}/lib ${FFMPEG_ROOT}/bin
        PATH_SUFFIXES lib bin
    )
    
    find_library(SWSCALE_LIBRARY
        NAMES swscale libswscale
        HINTS ${FFMPEG_ROOT}/lib ${CMAKE_PREFIX_PATH}/lib ${FFMPEG_ROOT}/bin
        PATH_SUFFIXES lib bin
    )
    
    if(AVCODEC_INCLUDE_DIR AND AVCODEC_LIBRARY AND AVFORMAT_LIBRARY AND AVUTIL_LIBRARY AND SWSCALE_LIBRARY)
        # Create imported targets manually
        add_library(FFMPEG::avcodec UNKNOWN IMPORTED)
        set_target_properties(FFMPEG::avcodec PROPERTIES
            IMPORTED_LOCATION "${AVCODEC_LIBRARY}"
            INTERFACE_INCLUDE_DIRECTORIES "${AVCODEC_INCLUDE_DIR}"
        )
        
        add_library(FFMPEG::avformat UNKNOWN IMPORTED)
        set_target_properties(FFMPEG::avformat PROPERTIES
            IMPORTED_LOCATION "${AVFORMAT_LIBRARY}"
            INTERFACE_INCLUDE_DIRECTORIES "${AVCODEC_INCLUDE_DIR}"
        )
        
        add_library(FFMPEG::avutil UNKNOWN IMPORTED)
        set_target_properties(FFMPEG::avutil PROPERTIES
            IMPORTED_LOCATION "${AVUTIL_LIBRARY}"
            INTERFACE_INCLUDE_DIRECTORIES "${AVCODEC_INCLUDE_DIR}"
        )
        
        add_library(FFMPEG::swscale UNKNOWN IMPORTED)
        set_target_properties(FFMPEG::swscale PROPERTIES
            IMPORTED_LOCATION "${SWSCALE_LIBRARY}"
            INTERFACE_INCLUDE_DIRECTORIES "${AVCODEC_INCLUDE_DIR}"
        )
        
        set(FFMPEG_FOUND TRUE)
        message(STATUS "Found FFmpeg (manual discovery)")
        message(STATUS "  avcodec: ${AVCODEC_LIBRARY}")
        message(STATUS "  avformat: ${AVFORMAT_LIBRARY}")
        message(STATUS "  avutil: ${AVUTIL_LIBRARY}")
        message(STATUS "  swscale: ${SWSCALE_LIBRARY}")
    else()
        set(FFMPEG_FOUND FALSE)
    endif()
endif()

# Fall back to pkg-config if FFmpeg not found manually (or not on Windows)
if(NOT TARGET FFMPEG::avcodec)
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(FFMPEG REQUIRED IMPORTED_TARGET
        libavcodec
        libavformat
        libavutil
        libswscale
    )
endif()

# Get git commit hash
execute_process(
    COMMAND git rev-parse --short HEAD
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    OUTPUT_VARIABLE GIT_COMMIT_HASH
    OUTPUT_STRIP_TRAILING_WHITESPACE
    ERROR_QUIET
)

# Check if git repo is dirty (has uncommitted changes)
execute_process(
    COMMAND git diff-index --quiet HEAD --
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
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
    ${CMAKE_CURRENT_SOURCE_DIR}/src/config/version.h.in
    ${CMAKE_CURRENT_BINARY_DIR}/generated/version.h
    @ONLY
)

# Include directories
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/src)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/src/config)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/src/utils)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/src/pipeline/loaders)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/src/pipeline/field_splitting)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/src/pipeline/preprocessing)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/src/pipeline/structure_generation)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/src/pipeline/metadata_generators)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/src/pipeline/metadata_generators/encoders)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/src/pipeline/active_encoding)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/src/pipeline/effects)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/src/pipeline/writers)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/src/pipeline/orchestrator)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/src/pipeline/common)
include_directories(${CMAKE_CURRENT_BINARY_DIR}/generated)

# Source files
set(SOURCES
    src/main.cpp
    src/utils/logging.cpp
    src/utils/thread_pool.cpp
    src/utils/audio_generator.cpp
    src/utils/path_resolver.cpp
    src/config/yaml_config.cpp
    src/pipeline/writers/metadata_writer.cpp
    src/pipeline/writers/audio_writer.cpp
    src/pipeline/metadata_generators/metadata_generator.cpp
    src/pipeline/metadata_generators/color_burst_generator.cpp
    src/pipeline/field_splitting/field_splitter.cpp
    src/pipeline/structure_generation/field_structure_generator.cpp
    src/pipeline/metadata_generators/pal_vits_generator.cpp
    src/pipeline/metadata_generators/ntsc_vits_generator.cpp
    src/pipeline/metadata_generators/encoders/biphase_encoder.cpp
    src/pipeline/metadata_generators/encoders/manchester_encoder.cpp
    src/pipeline/metadata_generators/vitc_generator.cpp
    src/pipeline/loaders/color_conversion.cpp
    src/pipeline/loaders/video_loader_base.cpp
    src/pipeline/loaders/yuv422_loader.cpp
    src/pipeline/loaders/png_loader.cpp
    src/pipeline/loaders/mov_loader.cpp
    src/pipeline/loaders/mp4_loader.cpp
    src/pipeline/metadata_generators/vbi_metadata_generator.cpp
    src/pipeline/metadata_generators/color_burst_pipeline_generator.cpp
    src/pipeline/metadata_generators/vitc_pipeline_generator.cpp
    src/pipeline/metadata_generators/vits_pipeline_generator.cpp
    src/pipeline/metadata_generators/biphase_vbi_generator.cpp
    src/pipeline/metadata_generators/pipeline_generators.cpp
    src/pipeline/active_encoding/pal_active_encoder.cpp
    src/pipeline/active_encoding/ntsc_active_encoder.cpp
    src/pipeline/orchestrator/video_encoder_pipeline.cpp
    src/pipeline/effects/field_effect.cpp
    src/pipeline/preprocessing/field_preprocessor.cpp
)

# Create executable
add_executable(encode-orc ${SOURCES})

# Link libraries
if(WIN32 AND FFMPEG_FOUND AND TARGET FFMPEG::avcodec)
    # Using manual FFmpeg discovery on Windows
    target_link_libraries(encode-orc PRIVATE 
        SQLite::SQLite3 
        yaml-cpp::yaml-cpp 
        PNG::PNG 
        spdlog::spdlog
        FFMPEG::avcodec
        FFMPEG::avformat
        FFMPEG::avutil
        FFMPEG::swscale
        Threads::Threads
    )
else()
    # Using pkg-config FFmpeg
    target_link_libraries(encode-orc PRIVATE 
        SQLite::SQLite3 
        yaml-cpp::yaml-cpp 
        PNG::PNG 
        spdlog::spdlog
        PkgConfig::FFMPEG
        Threads::Threads
    )
endif()

# Install target
install(TARGETS encode-orc 
    RUNTIME DESTINATION bin
    COMPONENT Runtime
)

# Install testcard images
install(DIRECTORY assets/
    DESTINATION share/encode-orc/assets
    FILES_MATCHING 
        PATTERN "*.png"
        PATTERN "*.raw"
        PATTERN "*.mov"
        PATTERN "*.mp4"
        PATTERN "*.pcm"
)

# Install documentation
install(FILES README.md LICENSE
    DESTINATION share/doc/encode-orc
    COMPONENT Runtime
)
