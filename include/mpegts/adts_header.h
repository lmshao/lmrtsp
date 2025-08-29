/**
 * @author SHAO Liming <lmshao@163.com>
 * @copyright Copyright (c) 2025 SHAO Liming
 * @license MIT
 *
 * SPDX-License-Identifier: MIT
 */

#ifndef LMSHAO_MPEGTS_ADTS_HEADER_H
#define LMSHAO_MPEGTS_ADTS_HEADER_H

#include <cstddef>
#include <cstdint>

namespace lmshao::mpegts {

class ADTSHeader {
public:
    ADTSHeader();
    ADTSHeader(int sample_rate, int channels, int frame_length);

    // Parses an ADTS header from the given data buffer.
    // Returns true on success, false on failure.
    bool Parse(const uint8_t *data, size_t size);

    // Generates an ADTS header and writes it to the provided buffer.
    // The buffer must have a size of at least 7 bytes.
    // Returns the number of bytes written (7).
    int Generate(uint8_t *buffer) const;

    // Getters
    int GetSampleRate() const;
    int GetChannels() const;
    int GetFrameLength() const;
    int GetProfile() const;
    bool IsProtectionAbsent() const;
    int GetHeaderSize() const;

    // Setters
    void SetSampleRate(int sample_rate);
    void SetChannels(int channels);
    void SetFrameLength(int frame_length);
    void SetProfile(int profile);
    void SetProtectionAbsent(bool protection_absent);

private:
    // ADTS header fields
    uint16_t sync_word_ = 0xFFF;
    uint8_t id_ = 0; // 0: MPEG-4, 1: MPEG-2
    uint8_t layer_ = 0;
    bool protection_absent_ = true;
    uint8_t profile_ = 1;                  // 1: LC
    uint8_t sampling_frequency_index_ = 4; // 4: 44100 Hz
    uint8_t private_bit_ = 0;
    uint8_t channel_configuration_ = 2; // 2: Stereo
    uint8_t original_copy_ = 0;
    uint8_t home_ = 0;
    uint8_t copyright_identification_bit_ = 0;
    uint8_t copyright_identification_start_ = 0;
    uint16_t aac_frame_length_ = 0;
    uint16_t adts_buffer_fullness_ = 0x7FF;
    uint8_t number_of_raw_data_blocks_in_frame_ = 0;

    // Helper functions
    static int GetSampleRateFromIndex(int index);
    static int GetIndexFromSampleRate(int sample_rate);
};

} // namespace lmshao::mpegts

#endif // LMSHAO_MPEGTS_ADTS_HEADER_H
