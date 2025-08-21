/**
 * @author SHAO Liming <lmshao@163.com>
 * @copyright Copyright (c) 2025 SHAO Liming
 * @license MIT
 *
 * SPDX-License-Identifier: MIT
 */

#ifndef RTSP_TS_TYPES_H
#define RTSP_TS_TYPES_H

#include <coreutils/data_buffer.h>

#include <cstdint>
#include <memory>
#include <vector>

namespace lmshao::rtsp::mpegts {

// TS packet size constants
constexpr size_t TS_PACKET_SIZE = 188;
constexpr size_t TS_HEADER_SIZE = 4;
constexpr size_t TS_PAYLOAD_SIZE = TS_PACKET_SIZE - TS_HEADER_SIZE;

// TS packet sync byte
constexpr uint8_t TS_SYNC_BYTE = 0x47;

// PID constants
constexpr uint16_t PAT_PID = 0x0000;
constexpr uint16_t PMT_PID = 0x1000;
constexpr uint16_t NULL_PID = 0x1FFF;

// Table ID constants
constexpr uint8_t PAT_TABLE_ID = 0x00;
constexpr uint8_t PMT_TABLE_ID = 0x02;

// PAT table structure constants
constexpr size_t PAT_TABLE_HEADER_SIZE = 8;
constexpr size_t PAT_PROGRAM_ENTRY_SIZE = 4;
constexpr size_t PAT_CRC_SIZE = 4;

// PMT table structure constants
constexpr size_t PMT_TABLE_HEADER_SIZE = 12;
constexpr size_t PMT_STREAM_ENTRY_SIZE = 5;
constexpr size_t PMT_CRC_SIZE = 4;

// Stream types
enum class StreamType : uint8_t {
    MPEG1_VIDEO = 0x01,
    MPEG2_VIDEO = 0x02,
    MPEG1_AUDIO = 0x03,
    MPEG2_AUDIO = 0x04,
    AAC_AUDIO = 0x0F,
    H264_VIDEO = 0x1B,
    H265_VIDEO = 0x24,
    AC3_AUDIO = 0x81,
    EAC3_AUDIO = 0x87
};

// TS packet header structure
struct TSHeader {
    uint8_t sync_byte; // Sync byte (0x47)
    uint8_t transport_error_indicator : 1;
    uint8_t payload_unit_start_indicator : 1;
    uint8_t transport_priority : 1;
    uint16_t pid : 13; // Packet identifier
    uint8_t transport_scrambling_control : 2;
    uint8_t adaptation_field_control : 2;
    uint8_t continuity_counter : 4;

    TSHeader()
        : sync_byte(TS_SYNC_BYTE), transport_error_indicator(0), payload_unit_start_indicator(0), transport_priority(0),
          pid(0), transport_scrambling_control(0), adaptation_field_control(0), continuity_counter(0)
    {
    }

    // Static method to parse header from raw data
    static TSHeader ParseFromData(const uint8_t *data)
    {
        TSHeader header;
        header.sync_byte = data[0];
        header.transport_error_indicator = (data[1] >> 7) & 0x01;
        header.payload_unit_start_indicator = (data[1] >> 6) & 0x01;
        header.transport_priority = (data[1] >> 5) & 0x01;
        header.pid = ((data[1] & 0x1F) << 8) | data[2];
        header.transport_scrambling_control = (data[3] >> 6) & 0x03;
        header.adaptation_field_control = (data[3] >> 4) & 0x03;
        header.continuity_counter = data[3] & 0x0F;
        return header;
    }

    // Method to generate raw data from header
    void ToRawData(uint8_t *data) const
    {
        data[0] = sync_byte;
        data[1] = (transport_error_indicator << 7) | (payload_unit_start_indicator << 6) | (transport_priority << 5) |
                  ((pid >> 8) & 0x1F);
        data[2] = pid & 0xFF;
        data[3] = (transport_scrambling_control << 6) | (adaptation_field_control << 4) | (continuity_counter & 0x0F);
    }
};

// PAT table entry structure
struct PATEntry {
    uint16_t program_number;
    uint16_t program_map_pid;

    PATEntry() : program_number(0), program_map_pid(0) {}
    PATEntry(uint16_t prog_num, uint16_t pmt_pid) : program_number(prog_num), program_map_pid(pmt_pid) {}
};

// PAT table structure
struct PATTable {
    uint16_t transport_stream_id;
    uint8_t version_number;
    bool current_next_indicator;
    uint8_t section_number;
    uint8_t last_section_number;
    std::vector<PATEntry> programs;

    PATTable()
        : transport_stream_id(0), version_number(0), current_next_indicator(false), section_number(0),
          last_section_number(0)
    {
    }

    // Static method to parse PAT table from raw data
    static bool ParseFromData(const uint8_t *data, size_t size, PATTable &pat_table);
};

struct TSPacket {
    TSHeader header;
    std::shared_ptr<coreutils::DataBuffer> adaptation_field;
    std::shared_ptr<coreutils::DataBuffer> payload;

    TSPacket() : adaptation_field(nullptr), payload(std::make_shared<coreutils::DataBuffer>()) {}

    size_t GetTotalSize() const;
    bool HasPCR() const;
    uint64_t GetPCR() const;
    std::vector<uint8_t> GetRawData() const;
};

} // namespace lmshao::rtsp::mpegts

#endif // RTSP_TS_TYPES_H
