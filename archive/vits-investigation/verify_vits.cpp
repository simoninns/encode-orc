/*
 * File:        verify_vits.cpp
 * Module:      encode-orc
 * Purpose:     Verify VITS signals in TBC file
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: 2026 Simon Inns
 */

#include <iostream>
#include <fstream>
#include <cstdint>
#include <vector>
#include <cmath>

int main(int argc, char* argv[]) {
    if (argc < 2) {
        std::cerr << "Usage: " << argv[0] << " <tbc-file>\n";
        return 1;
    }
    
    const char* filename = argv[1];
    
    // PAL parameters
    constexpr int32_t FIELD_WIDTH = 1135;
    constexpr int32_t FIELD_HEIGHT = 313;
    constexpr int32_t FIELD_SIZE = FIELD_WIDTH * FIELD_HEIGHT;
    constexpr int32_t VITS_START_LINE = 6;
    constexpr int32_t VITS_END_LINE = 22;
    
    // Open TBC file
    std::ifstream file(filename, std::ios::binary);
    if (!file) {
        std::cerr << "Error: Cannot open file " << filename << "\n";
        return 1;
    }
    
    // Read first field
    std::vector<uint16_t> field(FIELD_SIZE);
    file.read(reinterpret_cast<char*>(field.data()), FIELD_SIZE * 2);
    
    if (!file) {
        std::cerr << "Error: Failed to read field data\n";
        return 1;
    }
    
    // Read second field for complete VITS analysis
    std::vector<uint16_t> field2(FIELD_SIZE);
    file.read(reinterpret_cast<char*>(field2.data()), FIELD_SIZE * 2);
    
    bool has_field2 = file.good();
    
    file.close();
    
    std::cout << "VITS Verification for: " << filename << "\n";
    std::cout << "=========================================\n\n";
    
    std::cout << "FIELD 1 (First/Odd Field):\n";
    std::cout << "-------------------------\n";
    
    // Check VITS lines (6-22)
    for (int32_t line = VITS_START_LINE; line <= VITS_END_LINE; ++line) {
        const uint16_t* line_data = &field[line * FIELD_WIDTH];
        
        // Analyze active region (samples 185-1107)
        constexpr int32_t ACTIVE_START = 185;
        constexpr int32_t ACTIVE_END = 1107;
        
        // Also analyze burst region for color burst detection
        constexpr int32_t BURST_START = 98;
        constexpr int32_t BURST_END = 138;
        
        // Check burst region first (color burst lines should have modulated signal here)
        uint32_t burst_sum = 0;
        uint16_t burst_min_val = 0xFFFF;
        uint16_t burst_max_val = 0x0000;
        
        for (int32_t i = BURST_START; i < BURST_END; ++i) {
            uint16_t val = line_data[i];
            burst_sum += val;
            if (val < burst_min_val) burst_min_val = val;
            if (val > burst_max_val) burst_max_val = val;
        }
        
        float burst_avg = static_cast<float>(burst_sum) / (BURST_END - BURST_START);
        
        // Calculate statistics for active region
        uint32_t sum = 0;
        uint16_t min_val = 0xFFFF;
        uint16_t max_val = 0x0000;
        
        for (int32_t i = ACTIVE_START; i < ACTIVE_END; ++i) {
            uint16_t val = line_data[i];
            sum += val;
            if (val < min_val) min_val = val;
            if (val > max_val) max_val = val;
        }
        
        float avg = static_cast<float>(sum) / (ACTIVE_END - ACTIVE_START);
        
        // Calculate variance for signal detection (active region)
        float variance = 0.0f;
        for (int32_t i = ACTIVE_START; i < ACTIVE_END; ++i) {
            float diff = line_data[i] - avg;
            variance += diff * diff;
        }
        variance /= (ACTIVE_END - ACTIVE_START);
        float std_dev = std::sqrt(variance);
        
        // Also calculate burst variance
        float burst_variance = 0.0f;
        for (int32_t i = BURST_START; i < BURST_END; ++i) {
            float diff = line_data[i] - burst_avg;
            burst_variance += diff * diff;
        }
        burst_variance /= (BURST_END - BURST_START);
        float burst_std_dev = std::sqrt(burst_variance);
        
        std::cout << "Line " << line << ": ";
        std::cout << "Avg=0x" << std::hex << static_cast<uint32_t>(avg) << std::dec;
        std::cout << " Min=0x" << std::hex << min_val << std::dec;
        std::cout << " Max=0x" << std::hex << max_val << std::dec;
        std::cout << " StdDev=" << static_cast<uint32_t>(std_dev);
        
        // Check burst region separately
        if (burst_max_val - burst_min_val > 1000) {
            std::cout << " [BURST SIGNAL: StdDev=" << static_cast<uint32_t>(burst_std_dev) << "]";
        }
        // Identify signal type based on characteristics
        else if (std_dev > 5000) {
            std::cout << " [MODULATED SIGNAL]";
        } else if (max_val - min_val < 100) {
            std::cout << " [FLAT/BLANKING]";
        } else if (max_val - min_val < 1000) {
            std::cout << " [LOW VARIATION]";
        } else {
            std::cout << " [SIGNAL]";
        }
        
        std::cout << "\n";
    }
    
    // Analyze second field if available
    if (has_field2) {
        std::cout << "\nFIELD 2 (Second/Even Field):\n";
        std::cout << "----------------------------\n";
        
        for (int32_t line = VITS_START_LINE; line <= VITS_END_LINE; ++line) {
            const uint16_t* line_data = &field2[line * FIELD_WIDTH];
            
            // Analyze active region (samples 185-1107)
            constexpr int32_t ACTIVE_START = 185;
            constexpr int32_t ACTIVE_END = 1107;
            
            // Also analyze burst region for color burst detection
            constexpr int32_t BURST_START = 98;
            constexpr int32_t BURST_END = 138;
            
            // Check burst region first
            uint32_t burst_sum = 0;
            uint16_t burst_min_val = 0xFFFF;
            uint16_t burst_max_val = 0x0000;
            
            for (int32_t i = BURST_START; i < BURST_END; ++i) {
                uint16_t val = line_data[i];
                burst_sum += val;
                if (val < burst_min_val) burst_min_val = val;
                if (val > burst_max_val) burst_max_val = val;
            }
            
            float burst_avg = static_cast<float>(burst_sum) / (BURST_END - BURST_START);
            
            // Calculate statistics for active region
            uint32_t sum = 0;
            uint16_t min_val = 0xFFFF;
            uint16_t max_val = 0x0000;
            
            for (int32_t i = ACTIVE_START; i < ACTIVE_END; ++i) {
                uint16_t val = line_data[i];
                sum += val;
                if (val < min_val) min_val = val;
                if (val > max_val) max_val = val;
            }
            
            float avg = static_cast<float>(sum) / (ACTIVE_END - ACTIVE_START);
            
            // Calculate variance
            float variance = 0.0f;
            for (int32_t i = ACTIVE_START; i < ACTIVE_END; ++i) {
                float diff = line_data[i] - avg;
                variance += diff * diff;
            }
            variance /= (ACTIVE_END - ACTIVE_START);
            float std_dev = std::sqrt(variance);
            
            // Calculate burst variance
            float burst_variance = 0.0f;
            for (int32_t i = BURST_START; i < BURST_END; ++i) {
                float diff = line_data[i] - burst_avg;
                burst_variance += diff * diff;
            }
            burst_variance /= (BURST_END - BURST_START);
            float burst_std_dev = std::sqrt(burst_variance);
            
            std::cout << "Line " << line << ": ";
            std::cout << "Avg=0x" << std::hex << static_cast<uint32_t>(avg) << std::dec;
            std::cout << " Min=0x" << std::hex << min_val << std::dec;
            std::cout << " Max=0x" << std::hex << max_val << std::dec;
            std::cout << " StdDev=" << static_cast<uint32_t>(std_dev);
            
            if (burst_max_val - burst_min_val > 1000) {
                std::cout << " [BURST SIGNAL: StdDev=" << static_cast<uint32_t>(burst_std_dev) << "]";
            }
            else if (std_dev > 5000) {
                std::cout << " [MODULATED SIGNAL]";
            } else if (max_val - min_val < 100) {
                std::cout << " [FLAT/BLANKING]";
            } else if (max_val - min_val < 1000) {
                std::cout << " [LOW VARIATION]";
            } else {
                std::cout << " [SIGNAL]";
            }
            
            std::cout << "\n";
        }
    }
    
    std::cout << "\nIEC 60856-1986 VITS Summary:\n";
    std::cout << "============================\n";
    std::cout << "4 VITS signals on 2 field line positions:\n";
    std::cout << "  Signal 1 (Frame line 19):  Field 1, Line 19 = Luminance tests\n";
    std::cout << "  Signal 2 (Frame line 20):  Field 1, Line 20 = Multiburst\n";
    std::cout << "  Signal 3 (Frame line 332): Field 2, Line 19 = Differential gain/phase\n";
    std::cout << "  Signal 4 (Frame line 333): Field 2, Line 20 = Chrominance\n";
    std::cout << "\nLegend:\n";
    std::cout << "  MODULATED SIGNAL: High variation (color burst, multiburst, etc.)\n";
    std::cout << "  FLAT/BLANKING: Very low variation (blanking level)\n";
    std::cout << "  SIGNAL: Moderate variation (test signal present)\n";
    
    return 0;
}
