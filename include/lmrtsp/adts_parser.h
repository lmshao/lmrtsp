/**
 * @author SHAO Liming <lmshao@163.com>
 * @copyright Copyright (c) 2025 SHAO Liming
 * @license MIT
 *
 * SPDX-License-Identifier: MIT
 */

#ifndef LMRTSP_ADTS_PARSER_H
#define LMRTSP_ADTS_PARSER_H

#include <cstddef>
#include <cstdint>

namespace lmshao::lmrtsp {

/**
 * @brief AAC ADTS Header Structure (7 bytes)
 *
 * ADTS format is defined in ISO/IEC 13818-7 (MPEG-2 AAC) and ISO/IEC 14496-3 (MPEG-4 AAC)
 */
struct AdtsHeader {
    // Sync word (0xFFF)
    uint16_t syncword : 12;
    // MPEG Version: 0 for MPEG-4, 1 for MPEG-2
    uint8_t id : 1;
    // Layer, always 00
    uint8_t layer : 2;
    // Protection absent (1 = no CRC, 0 = CRC present)
    uint8_t protection_absent : 1;
    // Profile: 0=Main, 1=LC, 2=SSR, 3=reserved
    uint8_t profile : 2;
    // Sampling frequency index (0-12, see SAMPLING_FREQUENCIES table)
    uint8_t sampling_frequency_index : 4;
    // Private bit
    uint8_t private_bit : 1;
    // Channel configuration (0=defined in AOT, 1=1ch, 2=2ch, 3=3ch, etc.)
    uint8_t channel_configuration : 3;
    // Original/Copy
    uint8_t original_copy : 1;
    // Home
    uint8_t home : 1;
    // Copyright identification bit
    uint8_t copyright_identification_bit : 1;
    // Copyright identification start
    uint8_t copyright_identification_start : 1;
    // Frame length (including header)
    uint16_t aac_frame_length : 13;
    // Buffer fullness (0x7FF = VBR)
    uint16_t adts_buffer_fullness : 11;
    // Number of AAC frames (RDBs) in ADTS frame minus 1
    uint8_t number_of_raw_data_blocks_in_frame : 2;

    AdtsHeader()
        : syncword(0), id(0), layer(0), protection_absent(1), profile(0), sampling_frequency_index(0), private_bit(0),
          channel_configuration(0), original_copy(0), home(0), copyright_identification_bit(0),
          copyright_identification_start(0), aac_frame_length(0), adts_buffer_fullness(0x7FF),
          number_of_raw_data_blocks_in_frame(0)
    {
    }
};

/**
 * @brief ADTS Parser - utility class for parsing ADTS headers
 */
class AdtsParser {
public:
    // ADTS constants
    static constexpr size_t ADTS_HEADER_SIZE = 7;
    static constexpr size_t ADTS_HEADER_SIZE_WITH_CRC = 9;
    static constexpr uint16_t ADTS_SYNC_WORD = 0xFFF;
    static constexpr uint32_t SAMPLES_PER_AAC_FRAME = 1024; // for AAC-LC

    // Sampling frequency table (ISO/IEC 13818-7)
    // Index 0-12 are valid, 13-14 are reserved, 15 is escape value
    static constexpr uint32_t SAMPLING_FREQUENCIES[] = {96000, 88200, 64000, 48000, 44100, 32000, 24000, 22050,
                                                        16000, 12000, 11025, 8000,  7350,  0,     0,     0};

    /**
     * @brief Parse ADTS header from buffer
     * @param data Pointer to data buffer
     * @param size Size of data buffer
     * @param header Output parsed header
     * @return true if header was parsed successfully
     */
    static bool ParseHeader(const uint8_t *data, size_t size, AdtsHeader &header);

    /**
     * @brief Find next ADTS sync word in buffer
     * @param data Pointer to data buffer
     * @param size Size of data buffer
     * @param offset Starting offset to search from
     * @return Offset of sync word, or size if not found
     */
    static size_t FindSyncWord(const uint8_t *data, size_t size, size_t offset = 0);

    /**
     * @brief Get sampling frequency from index
     * @param index Sampling frequency index (0-12)
     * @return Sampling frequency in Hz, or 0 if invalid
     */
    static uint32_t GetSamplingFrequency(uint8_t index);

    /**
     * @brief Get frame duration in microseconds
     * @param sample_rate Sample rate in Hz
     * @param samples_per_frame Samples per frame (default 1024 for AAC-LC)
     * @return Frame duration in microseconds
     */
    static uint64_t GetFrameDurationUs(uint32_t sample_rate, uint32_t samples_per_frame = SAMPLES_PER_AAC_FRAME);

    /**
     * @brief Validate ADTS header
     * @param header Header to validate
     * @return true if header is valid
     */
    static bool ValidateHeader(const AdtsHeader &header);

    /**
     * @brief Get AAC profile name
     * @param profile Profile value (0-3)
     * @return Profile name string
     */
    static const char *GetProfileName(uint8_t profile);
};

} // namespace lmshao::lmrtsp

#endif // LMRTSP_ADTS_PARSER_H
