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

# Enable testing
enable_testing()

# Find SQLite3
find_package(SQLite3 REQUIRED)

# Include directories
include_directories(${PROJECT_SOURCE_DIR}/include)

# Source files
set(SOURCES
    src/main.cpp
    src/cli_parser.cpp
    src/metadata_writer.cpp
    src/blanking_encoder.cpp
    src/pal_encoder.cpp
    src/color_conversion.cpp
    src/test_card_generator.cpp
    src/video_encoder.cpp
)

# Create executable
add_executable(encode-orc ${SOURCES})

# Link libraries
target_link_libraries(encode-orc PRIVATE SQLite::SQLite3)

# Install target
install(TARGETS encode-orc DESTINATION bin)

# Add subdirectories
add_subdirectory(tests)
