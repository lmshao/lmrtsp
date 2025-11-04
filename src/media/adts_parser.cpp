/**
 * @author SHAO Liming <lmshao@163.com>
 * @copyright Copyright (c) 2025 SHAO Liming
 * @license MIT
 *
 * SPDX-License-Identifier: MIT
 */

#include "lmrtsp/adts_parser.h"

#include <cstddef>

namespace lmshao::lmrtsp {

constexpr size_t AdtsParser::ADTS_HEADER_SIZE;
constexpr size_t AdtsParser::ADTS_HEADER_SIZE_WITH_CRC;
constexpr uint16_t AdtsParser::ADTS_SYNC_WORD;
constexpr uint32_t AdtsParser::SAMPLES_PER_AAC_FRAME;
constexpr uint32_t AdtsParser::SAMPLING_FREQUENCIES[];

bool AdtsParser::ParseHeader(const uint8_t *data, size_t size, AdtsHeader &header)
{
    if (!data || size < ADTS_HEADER_SIZE) {
        return false;
    }

    // Parse ADTS header (bit-packed format)
    uint16_t sync = (data[0] << 4) | (data[1] >> 4);
    if (sync != ADTS_SYNC_WORD) {
        return false;
    }

    header.syncword = sync;
    header.id = (data[1] >> 3) & 0x01;
    header.layer = (data[1] >> 1) & 0x03;
    header.protection_absent = data[1] & 0x01;
    header.profile = (data[2] >> 6) & 0x03;
    header.sampling_frequency_index = (data[2] >> 2) & 0x0F;
    header.private_bit = (data[2] >> 1) & 0x01;
    header.channel_configuration = ((data[2] & 0x01) << 2) | ((data[3] >> 6) & 0x03);
    header.original_copy = (data[3] >> 5) & 0x01;
    header.home = (data[3] >> 4) & 0x01;
    header.copyright_identification_bit = (data[3] >> 3) & 0x01;
    header.copyright_identification_start = (data[3] >> 2) & 0x01;
    header.aac_frame_length = ((data[3] & 0x03) << 11) | (data[4] << 3) | ((data[5] >> 5) & 0x07);
    header.adts_buffer_fullness = ((data[5] & 0x1F) << 6) | ((data[6] >> 2) & 0x3F);
    header.number_of_raw_data_blocks_in_frame = data[6] & 0x03;

    return ValidateHeader(header);
}

size_t AdtsParser::FindSyncWord(const uint8_t *data, size_t size, size_t offset)
{
    if (!data || offset >= size) {
        return size;
    }

    for (size_t i = offset; i < size - 1; i++) {
        if (data[i] == 0xFF && (data[i + 1] & 0xF0) == 0xF0) {
            return i;
        }
    }
    return size;
}

uint32_t AdtsParser::GetSamplingFrequency(uint8_t index)
{
    if (index >= 13) {
        return 0;
    }
    return SAMPLING_FREQUENCIES[index];
}

uint64_t AdtsParser::GetFrameDurationUs(uint32_t sample_rate, uint32_t samples_per_frame)
{
    if (sample_rate == 0) {
        return 0;
    }
    // Duration in microseconds = (samples * 1,000,000) / sample_rate
    return (static_cast<uint64_t>(samples_per_frame) * 1000000ULL) / sample_rate;
}

bool AdtsParser::ValidateHeader(const AdtsHeader &header)
{
    // Check sync word
    if (header.syncword != AdtsParser::ADTS_SYNC_WORD) {
        return false;
    }

    // Check layer (should be 00)
    if (header.layer != 0) {
        return false;
    }

    // Validate frame length (minimum is header size, maximum is reasonable)
    if (header.aac_frame_length < AdtsParser::ADTS_HEADER_SIZE || header.aac_frame_length > 8192) {
        return false;
    }

    // Validate sampling frequency index
    if (header.sampling_frequency_index >= 13) {
        return false;
    }

    // Validate profile (0-3 are valid in ADTS)
    if (header.profile > 3) {
        return false;
    }

    return true;
}

const char *AdtsParser::GetProfileName(uint8_t profile)
{
    switch (profile) {
        case 0:
            return "AAC Main";
        case 1:
            return "AAC-LC";
        case 2:
            return "AAC-SSR";
        case 3:
            return "AAC-LTP";
        default:
            return "Unknown";
    }
}

} // namespace lmshao::lmrtsp
