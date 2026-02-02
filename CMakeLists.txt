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
    # Additional warnings to match macOS strictness
    if(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
        # Clang-specific warnings
        add_compile_options(-Wunused-private-field)
    elseif(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
        # GCC-specific warnings
        add_compile_options(-Wunused-but-set-variable)
    endif()
endif()

# Enable all spdlog levels at compile time (runtime filtering still applies)
add_compile_definitions(SPDLOG_ACTIVE_LEVEL=SPDLOG_LEVEL_TRACE)

# Enable testing
enable_testing()

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

# Install example projects
install(DIRECTORY example-projects/
    DESTINATION share/encode-orc/example-projects
    FILES_MATCHING PATTERN "*.yaml"
)

# Install testcard images
install(DIRECTORY testcard-images/
    DESTINATION share/encode-orc/testcard-images
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

# On Windows, install vcpkg DLLs alongside the executable
if(WIN32)
    # Install script to copy vcpkg DLLs during installation
    install(CODE "
        # Ensure bin directory exists
        file(MAKE_DIRECTORY \"\${CMAKE_INSTALL_PREFIX}/bin\")
        
        # Copy vcpkg runtime DLLs
        set(VCPKG_BIN_DIR \"C:/vcpkg/installed/x64-windows/bin\")
        if(EXISTS \"\${VCPKG_BIN_DIR}\")
            message(STATUS \"Installing vcpkg runtime dependencies...\")
            
            # List of required DLL patterns
            set(DLL_PATTERNS
                yaml-cpp
                spdlog
                libpng16
                sqlite3
                zlib1
                avcodec
                avformat
                avutil
                swscale
                swresample
                x264
                x265
            )
            
            foreach(DLL_PATTERN \${DLL_PATTERNS})
                file(GLOB DLL_FILES \"\${VCPKG_BIN_DIR}/\${DLL_PATTERN}*.dll\")
                foreach(DLL_FILE \${DLL_FILES})
                    get_filename_component(DLL_NAME \"\${DLL_FILE}\" NAME)
                    message(STATUS \"  Installing \${DLL_NAME}\")
                    file(INSTALL \"\${DLL_FILE}\" DESTINATION \"\${CMAKE_INSTALL_PREFIX}/bin\")
                endforeach()
            endforeach()
            
            message(STATUS \"vcpkg DLLs installed successfully\")
        else()
            message(WARNING \"vcpkg bin directory not found at: \${VCPKG_BIN_DIR}\")
            message(WARNING \"DLLs may need to be copied manually\")
        endif()
    " COMPONENT Runtime)
endif()

# =============================================================================
# CPack configuration for installer generation
# =============================================================================

set(CPACK_PACKAGE_NAME "Encode Orc")
set(CPACK_PACKAGE_VENDOR "Simon Inns")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "TBC encoder for LaserDisc and tape mastering workflows")
set(CPACK_PACKAGE_VERSION "${PROJECT_VERSION}")
set(CPACK_PACKAGE_VERSION_MAJOR "${PROJECT_VERSION_MAJOR}")
set(CPACK_PACKAGE_VERSION_MINOR "${PROJECT_VERSION_MINOR}")
set(CPACK_PACKAGE_VERSION_PATCH "${PROJECT_VERSION_PATCH}")
set(CPACK_PACKAGE_INSTALL_DIRECTORY "EncodeOrc")
set(CPACK_PACKAGE_EXECUTABLES "encode-orc;Encode Orc")

# License and readme
set(CPACK_RESOURCE_FILE_LICENSE "${CMAKE_CURRENT_SOURCE_DIR}/LICENSE")
set(CPACK_RESOURCE_FILE_README "${CMAKE_CURRENT_SOURCE_DIR}/README.md")

# WiX specific settings for MSI generation on Windows
if(WIN32)
    set(CPACK_GENERATOR "WIX")
    set(CPACK_WIX_UPGRADE_GUID "A1B2C3D4-E5F6-7890-A1B2-C3D4E5F67890")
    set(CPACK_WIX_PRODUCT_GUID "*") # Generate new GUID for each version
    
    # Set license in RTF format (created by GitHub Actions workflow)
    set(CPACK_WIX_LICENSE_RTF "${CMAKE_BINARY_DIR}/LICENSE.rtf")
    
    set(CPACK_WIX_PROGRAM_MENU_FOLDER "Encode Orc")
    
    # Create start menu shortcuts
    set(CPACK_PACKAGE_EXECUTABLES "encode-orc;Encode Orc")
    
    # Add to Programs and Features
    set(CPACK_WIX_PROPERTY_ARPURLINFOABOUT "https://github.com/simoninns/encode-orc")
    set(CPACK_WIX_PROPERTY_ARPHELPLINK "https://simoninns.github.io/encode-orc/")
    
    # Generate RTF license from plain text LICENSE file
    # Note: This will be created by the GitHub Actions workflow
    # This will be done in the GitHub Actions workflow
endif()

include(CPack)
