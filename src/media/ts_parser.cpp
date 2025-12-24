/**
 * @author SHAO Liming <lmshao@163.com>
 * @copyright Copyright (c) 2025 SHAO Liming
 * @license MIT
 *
 * SPDX-License-Identifier: MIT
 */

#include "lmrtsp/ts_parser.h"

#include <cstring>

namespace lmshao::lmrtsp {

// TS packet constants
static constexpr size_t TS_PACKET_SIZE = 188;
static constexpr uint8_t TS_SYNC_BYTE = 0x47;

// TS header field offsets and masks
static constexpr uint8_t TS_HEADER_SIZE = 4;
static constexpr uint8_t TS_PID_MASK = 0x1F;
static constexpr uint8_t TS_ADAPTATION_FIELD_CONTROL_MASK = 0x30;
static constexpr uint8_t TS_ADAPTATION_FIELD_CONTROL_PAYLOAD_ONLY = 0x01;
static constexpr uint8_t TS_ADAPTATION_FIELD_CONTROL_ADAPTATION_ONLY = 0x02;
static constexpr uint8_t TS_ADAPTATION_FIELD_CONTROL_BOTH = 0x03;

// Adaptation field flags
static constexpr uint8_t TS_AF_DISCONTINUITY_MASK = 0x80;
static constexpr uint8_t TS_AF_RANDOM_ACCESS_MASK = 0x40;
static constexpr uint8_t TS_AF_PCR_FLAG_MASK = 0x10;

bool TSParser::ParsePacket(const uint8_t *packet_data, TSPacketInfo &info)
{
    // Initialize output
    info = TSPacketInfo{};

    if (!packet_data) {
        return false;
    }

    // Verify sync byte
    if (packet_data[0] != TS_SYNC_BYTE) {
        return false;
    }

    // Extract PID (13 bits, bytes 1-2)
    info.pid = ((packet_data[1] & TS_PID_MASK) << 8) | packet_data[2];

    // Extract adaptation field control (2 bits, byte 3)
    uint8_t adaptation_field_control = (packet_data[3] & TS_ADAPTATION_FIELD_CONTROL_MASK) >> 4;

    // Check if adaptation field is present
    if (adaptation_field_control == TS_ADAPTATION_FIELD_CONTROL_ADAPTATION_ONLY ||
        adaptation_field_control == TS_ADAPTATION_FIELD_CONTROL_BOTH) {
        info.has_adaptation_field = true;

        // Adaptation field starts at byte 4 (after 4-byte header)
        // First byte of adaptation field is the length
        if (TS_HEADER_SIZE >= TS_PACKET_SIZE) {
            return false; // Invalid packet
        }

        uint8_t adaptation_field_length = packet_data[TS_HEADER_SIZE];
        if (adaptation_field_length == 0) {
            // Adaptation field exists but is empty
            return true;
        }

        if (TS_HEADER_SIZE + adaptation_field_length >= TS_PACKET_SIZE) {
            return false; // Invalid adaptation field length
        }

        // Extract flags from second byte of adaptation field
        const uint8_t *adaptation_field = packet_data + TS_HEADER_SIZE;
        uint8_t flags = adaptation_field[1];

        // Check discontinuity indicator
        info.discontinuity = (flags & TS_AF_DISCONTINUITY_MASK) != 0;

        // Check random access indicator
        info.random_access = (flags & TS_AF_RANDOM_ACCESS_MASK) != 0;

        // Extract PCR if present
        if (flags & TS_AF_PCR_FLAG_MASK) {
            if (ExtractPCR(adaptation_field, adaptation_field_length, info.pcr)) {
                info.has_pcr = true;
            }
        }
    }

    return true;
}

bool TSParser::ExtractPCR(const uint8_t *adaptation_field_data, uint8_t adaptation_field_length, uint64_t &pcr)
{
    if (!adaptation_field_data || adaptation_field_length < 8) {
        // Need at least 8 bytes: length(1) + flags(1) + PCR(6)
        return false;
    }

    // PCR is stored in 6 bytes starting at offset 2 (after length and flags)
    // Format: 33 bits base + 6 bits extension + 1 reserved bit
    // PCR_base (33 bits): bytes 2-5 (first 33 bits)
    // PCR_ext (6 bits): byte 6 (bits 0-5)
    // Reserved (1 bit): byte 6 (bit 6)
    // Total PCR = (PCR_base * 300) + PCR_ext

    uint64_t pcr_base = 0;
    pcr_base |= static_cast<uint64_t>(adaptation_field_data[2]) << 25;
    pcr_base |= static_cast<uint64_t>(adaptation_field_data[3]) << 17;
    pcr_base |= static_cast<uint64_t>(adaptation_field_data[4]) << 9;
    pcr_base |= static_cast<uint64_t>(adaptation_field_data[5]) << 1;
    pcr_base |= (adaptation_field_data[6] >> 7) & 0x01;

    uint8_t pcr_ext = adaptation_field_data[6] & 0x3F; // Lower 6 bits

    // PCR in 27MHz ticks = (PCR_base * 300) + PCR_ext
    pcr = (pcr_base * 300) + pcr_ext;

    return true;
}

uint32_t TSParser::PCRToRTPTimestamp(uint64_t pcr)
{
    // PCR is in 27MHz clock units
    // RTP timestamp is in 90kHz clock units
    // Conversion: rtp_ts = (pcr * 90) / 27000 = pcr / 300
    return static_cast<uint32_t>(pcr / 300);
}

uint32_t TSParser::CalculateRTPIncrementFromPCR(uint64_t pcr1, uint64_t pcr2, uint32_t packet_count)
{
    if (packet_count == 0) {
        return 0;
    }

    // Handle PCR wrap-around (PCR is 33 bits, max value is 2^33 - 1)
    uint64_t pcr_diff;
    const uint64_t PCR_MAX = (1ULL << 33) - 1;

    if (pcr2 >= pcr1) {
        pcr_diff = pcr2 - pcr1;
    } else {
        // Wrap-around occurred
        pcr_diff = (PCR_MAX - pcr1) + pcr2 + 1;
    }

    // Convert PCR difference to RTP timestamp difference
    uint64_t rtp_diff = pcr_diff / 300;

    // Calculate increment per packet
    return static_cast<uint32_t>(rtp_diff / packet_count);
}

bool TSParser::IsPCRDiscontinuous(uint64_t prev_pcr, uint64_t curr_pcr, uint64_t max_interval)
{
    // Handle PCR wrap-around
    const uint64_t PCR_MAX = (1ULL << 33) - 1;
    uint64_t pcr_diff;

    if (curr_pcr >= prev_pcr) {
        pcr_diff = curr_pcr - prev_pcr;
    } else {
        // Wrap-around occurred
        pcr_diff = (PCR_MAX - prev_pcr) + curr_pcr + 1;
    }

    // Check if difference exceeds maximum expected interval
    // max_interval default is 0.1s = 2700000 ticks (27MHz)
    return pcr_diff > max_interval;
}

} // namespace lmshao::lmrtsp
