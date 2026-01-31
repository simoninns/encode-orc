/*
 * File:        audio_writer.cpp
 * Module:      encode-orc
 * Purpose:     PCM/WAV audio writer for analogue sound
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: 2026 Simon Inns
 */

#include "audio_writer.h"
#include <cstring>

namespace encode_orc {

bool AudioWriter::open(const std::string& filename, Format format,
                       int32_t sample_rate, int32_t channels, int32_t bits_per_sample) {
    close();

    format_ = format;
    sample_rate_ = sample_rate;
    channels_ = channels;
    bits_per_sample_ = bits_per_sample;
    data_bytes_written_ = 0;

    file_.open(filename, std::ios::binary);
    if (!file_.is_open()) {
        return false;
    }

    if (format_ == Format::WAV) {
        write_wav_header_placeholder();
    }

    return true;
}

void AudioWriter::close() {
    if (file_.is_open()) {
        if (format_ == Format::WAV) {
            finalize_wav_header();
        }
        file_.close();
    }
}

bool AudioWriter::write_samples(const std::vector<int16_t>& samples) {
    if (!file_.is_open() || samples.empty()) {
        return false;
    }

    const char* data_ptr = reinterpret_cast<const char*>(samples.data());
    std::streamsize bytes = static_cast<std::streamsize>(samples.size() * sizeof(int16_t));
    file_.write(data_ptr, bytes);
    if (!file_.good()) {
        return false;
    }

    data_bytes_written_ += bytes;
    return true;
}

void AudioWriter::write_wav_header_placeholder() {
    // Standard 44-byte WAV header with placeholder sizes
    char header[44];
    std::memset(header, 0, sizeof(header));

    // RIFF header
    std::memcpy(header + 0, "RIFF", 4);
    // Chunk size (placeholder)
    std::memcpy(header + 8, "WAVE", 4);

    // fmt subchunk
    std::memcpy(header + 12, "fmt ", 4);
    uint32_t subchunk1_size = 16;
    uint16_t audio_format = 1; // PCM
    uint32_t byte_rate = sample_rate_ * channels_ * bits_per_sample_ / 8;
    uint16_t block_align = static_cast<uint16_t>(channels_ * bits_per_sample_ / 8);

    std::memcpy(header + 16, &subchunk1_size, 4);
    std::memcpy(header + 20, &audio_format, 2);
    std::memcpy(header + 22, &channels_, 2);
    std::memcpy(header + 24, &sample_rate_, 4);
    std::memcpy(header + 28, &byte_rate, 4);
    std::memcpy(header + 32, &block_align, 2);
    std::memcpy(header + 34, &bits_per_sample_, 2);

    // data subchunk
    std::memcpy(header + 36, "data", 4);
    // Subchunk2 size (placeholder)

    file_.write(header, sizeof(header));
}

void AudioWriter::finalize_wav_header() {
    // Update RIFF chunk size and data chunk size
    uint32_t data_chunk_size = static_cast<uint32_t>(data_bytes_written_);
    uint32_t riff_chunk_size = 36 + data_chunk_size;

    file_.seekp(4, std::ios::beg);
    file_.write(reinterpret_cast<const char*>(&riff_chunk_size), 4);

    file_.seekp(40, std::ios::beg);
    file_.write(reinterpret_cast<const char*>(&data_chunk_size), 4);

    file_.seekp(0, std::ios::end);
}

} // namespace encode_orc
