/*
 * File:        audio_writer.h
 * Module:      encode-orc
 * Purpose:     PCM/WAV audio writer for analogue sound
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: 2026 Simon Inns
 */

#ifndef ENCODE_ORC_AUDIO_WRITER_H
#define ENCODE_ORC_AUDIO_WRITER_H

#include <cstdint>
#include <fstream>
#include <string>
#include <vector>

namespace encode_orc {

class AudioWriter {
public:
    enum class Format {
        PCM,
        WAV
    };

    AudioWriter() = default;

    bool open(const std::string& filename, Format format,
              int32_t sample_rate = 44100, int32_t channels = 2, int32_t bits_per_sample = 16);
    void close();
    bool is_open() const { return file_.is_open(); }

    bool write_samples(const std::vector<int16_t>& samples);

private:
    void write_wav_header_placeholder();
    void finalize_wav_header();

    std::ofstream file_;
    Format format_ = Format::PCM;
    int32_t sample_rate_ = 44100;
    int32_t channels_ = 2;
    int32_t bits_per_sample_ = 16;
    int64_t data_bytes_written_ = 0;
};

} // namespace encode_orc

#endif // ENCODE_ORC_AUDIO_WRITER_H
