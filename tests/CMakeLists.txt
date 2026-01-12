# Tests will be added here as the project develops

# PAL encoder test
add_executable(test_pal_encoder 
    test_pal_encoder.cpp
    ../src/pal_encoder.cpp
    ../src/color_conversion.cpp
    ../src/metadata_writer.cpp
)
target_link_libraries(test_pal_encoder PRIVATE SQLite::SQLite3)
add_test(NAME test_pal_encoder COMMAND test_pal_encoder)

# Signal analyzer
add_executable(analyze_signal
    analyze_signal.cpp
    ../src/pal_encoder.cpp
    ../src/color_conversion.cpp
)
target_link_libraries(analyze_signal PRIVATE SQLite::SQLite3)

# Color value test
add_executable(test_color_values
    test_color_values.cpp
    ../src/color_conversion.cpp
)
target_link_libraries(test_color_values PRIVATE SQLite::SQLite3)

# Chroma analyzer
add_executable(analyze_chroma
    analyze_chroma.cpp
)

# Debug PAL encoder test
add_executable(test_pal_encode_debug
    test_pal_encode_debug.cpp
    ../src/pal_encoder.cpp
    ../src/color_conversion.cpp
)
target_link_libraries(test_pal_encode_debug PRIVATE SQLite::SQLite3)

# Single color test
add_executable(test_single_color
    test_single_color.cpp
    ../src/pal_encoder.cpp
    ../src/color_conversion.cpp
    ../src/metadata_writer.cpp
)
target_link_libraries(test_single_color PRIVATE SQLite::SQLite3)



