# Essential color conversion tests for Phase 2 work

# Color value test - validates YUV/YIQ conversion
add_executable(test_color_values
    test_color_values.cpp
    ../src/color_conversion.cpp
)
target_link_libraries(test_color_values PRIVATE SQLite::SQLite3)
add_test(NAME test_color_values COMMAND test_color_values)

# Signal analyzer - for debugging signal generation
add_executable(analyze_signal
    analyze_signal.cpp
    ../src/pal_encoder.cpp
    ../src/color_conversion.cpp
)
target_link_libraries(analyze_signal PRIVATE SQLite::SQLite3)

# Chroma analyzer - for color/signal debugging
add_executable(analyze_chroma
    analyze_chroma.cpp
)
