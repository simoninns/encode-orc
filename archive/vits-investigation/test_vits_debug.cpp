#include "pal_laserdisc_line_allocator.h"
#include <iostream>

using namespace encode_orc;

int main() {
    PALLaserDiscLineAllocator allocator;
    
    std::cout << "PAL LaserDisc Line Allocations:\n";
    std::cout << "================================\n\n";
    
    const auto& allocations = allocator.getAllocations();
    for (const auto& alloc : allocations) {
        std::cout << "Line: " << alloc.lineNumber 
                  << ", Signal Type: " << static_cast<int>(alloc.signalType)
                  << " (" << vits_signal_type_to_string(alloc.signalType) << ")"
                  << ", Field1: " << alloc.includeInField1
                  << ", Field2: " << alloc.includeInField2
                  << ", Description: " << alloc.description << "\n";
    }
    
    return 0;
}
