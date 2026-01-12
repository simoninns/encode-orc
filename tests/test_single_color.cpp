#include "frame_buffer.h"
#include "color_conversion.h"
#include "pal_encoder.h"
#include "tbc_writer.h"
#include "metadata_writer.h"
#include <iostream>
#include <cstdlib>

using namespace encode_orc;

int main() {
    std::cout << "Testing single solid red frame encoding...\n\n";
    
    // Create a 720x576 solid red frame
    FrameBuffer rgb_frame(720, 576, FrameBuffer::Format::RGB48);
    
    // Fill with solid red (R=0xFFFF, G=0, B=0)
    for (int32_t y = 0; y < 576; ++y) {
        for (int32_t x = 0; x < 720; ++x) {
            rgb_frame.set_rgb_pixel(x, y, 0xFFFF * 0.75, 0, 0);  // 75% red
        }
    }
    
    // Convert to YUV
    FrameBuffer yuv_frame = ColorConverter::rgb_to_yuv_pal(rgb_frame);
    
    // Create PAL encoder
    PALEncoder encoder;
    
    // Encode 10 frames
    TBCWriter writer;
    std::string output_filename = "test-output/test-solid-red.tbc";
    
    if (!writer.open(output_filename)) {
        std::cerr << "Error: Failed to open TBC file\n";
        return 1;
    }
    
    std::cout << "Encoding 10 frames of solid red...\n";
    for (int frame_num = 0; frame_num < 10; ++frame_num) {
        Field field1 = encoder.encode_field(yuv_frame, frame_num * 2, true);
        Field field2 = encoder.encode_field(yuv_frame, frame_num * 2 + 1, false);
        
        writer.write_field(field1);
        writer.write_field(field2);
        
        std::cout << "  Frame " << (frame_num + 1) << "/10\r" << std::flush;
    }
    std::cout << "\n";
    
    writer.close();
    
    // Write metadata
    std::string metadata_filename = output_filename + ".db";
    MetadataWriter metadata_writer;
    
    if (!metadata_writer.open(metadata_filename)) {
        std::cerr << "Error: Failed to open metadata file\n";
        return 1;
    }
    
    std::cout << "Writing metadata...\n";
    for (int frame_num = 0; frame_num < 10; ++frame_num) {
        metadata_writer.add_frame(frame_num, VideoParameters::PAL);
    }
    
    metadata_writer.close();
    
    std::cout << "\nSingle color test completed!\n";
    std::cout << "Output: " << output_filename << "\n";
    std::cout << "Try decoding with: ld-analyse test-solid-red.tbc\n";
    std::cout << "Expected: Entire frame should be solid 75% red\n";
    
    return 0;
}
