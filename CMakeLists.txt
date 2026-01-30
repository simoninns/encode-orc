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
    ${CMAKE_SOURCE_DIR}/src/config/version.h.in
    ${CMAKE_BINARY_DIR}/generated/version.h
    @ONLY
)

# Include directories
include_directories(${PROJECT_SOURCE_DIR}/src)
include_directories(${PROJECT_SOURCE_DIR}/src/config)
include_directories(${PROJECT_SOURCE_DIR}/src/utils)
include_directories(${PROJECT_SOURCE_DIR}/src/pipeline/loaders)
include_directories(${PROJECT_SOURCE_DIR}/src/pipeline/field_splitting)
include_directories(${PROJECT_SOURCE_DIR}/src/pipeline/preprocessing)
include_directories(${PROJECT_SOURCE_DIR}/src/pipeline/structure_generation)
include_directories(${PROJECT_SOURCE_DIR}/src/pipeline/metadata_generators)
include_directories(${PROJECT_SOURCE_DIR}/src/pipeline/metadata_generators/encoders)
include_directories(${PROJECT_SOURCE_DIR}/src/pipeline/active_encoding)
include_directories(${PROJECT_SOURCE_DIR}/src/pipeline/effects)
include_directories(${PROJECT_SOURCE_DIR}/src/pipeline/writers)
include_directories(${PROJECT_SOURCE_DIR}/src/pipeline/orchestrator)
include_directories(${PROJECT_SOURCE_DIR}/src/pipeline/common)
include_directories(${CMAKE_BINARY_DIR}/generated)

# Source files
set(SOURCES
    src/main.cpp
    src/utils/logging.cpp
    src/config/yaml_config.cpp
    src/pipeline/writers/metadata_writer.cpp
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
    
    # Phase 4: Pipeline metadata generators
    src/pipeline/metadata_generators/vbi_metadata_generator.cpp
    src/pipeline/metadata_generators/color_burst_pipeline_generator.cpp
    src/pipeline/metadata_generators/vitc_pipeline_generator.cpp
    src/pipeline/metadata_generators/vits_pipeline_generator.cpp
    src/pipeline/metadata_generators/biphase_vbi_generator.cpp
    src/pipeline/metadata_generators/pipeline_generators.cpp
    # Phase 5: Active video encoding refactoring
    src/pipeline/active_encoding/pal_active_encoder.cpp
    src/pipeline/active_encoding/ntsc_active_encoder.cpp
    src/pipeline/orchestrator/video_encoder_pipeline.cpp
    # Phase 6: Field effects and preprocessing
    src/pipeline/effects/field_effect.cpp
    src/pipeline/preprocessing/field_preprocessor.cpp
)

# Create executable
add_executable(encode-orc ${SOURCES})

# Link libraries
target_link_libraries(encode-orc PRIVATE SQLite::SQLite3 yaml-cpp PNG::PNG spdlog::spdlog)

# Install target
install(TARGETS encode-orc DESTINATION bin)
