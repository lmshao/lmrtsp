/**
 * @author SHAO Liming <lmshao@163.com>
 * @copyright Copyright (c) 2025 SHAO Liming
 * @license MIT
 *
 * SPDX-License-Identifier: MIT
 */

#include "mpegts/ts_types.h"

#include <cstring>

namespace lmshao::rtsp::mpegts {

size_t TSPacket::GetTotalSize() const
{
    return TS_HEADER_SIZE + (payload ? payload->Size() : 0);
}

bool TSPacket::HasPCR() const
{
    // Simplified implementation, actually need to parse adaptation field
    return false;
}

uint64_t TSPacket::GetPCR() const
{
    // Simplified implementation, actually need to extract PCR from adaptation field
    return 0;
}

std::vector<uint8_t> TSPacket::GetRawData() const
{
    std::vector<uint8_t> raw_data;
    raw_data.reserve(GetTotalSize());

    // Add header
    raw_data.push_back(header.sync_byte);

    // Build header bytes
    uint8_t byte1 = (header.transport_error_indicator << 7) | (header.payload_unit_start_indicator << 6) |
                    (header.transport_priority << 5) | ((header.pid >> 8) & 0x1F);
    raw_data.push_back(byte1);

    uint8_t byte2 = header.pid & 0xFF;
    raw_data.push_back(byte2);

    uint8_t byte3 = (header.transport_scrambling_control << 6) | (header.adaptation_field_control << 4) |
                    (header.continuity_counter & 0x0F);
    raw_data.push_back(byte3);

    // Add payload
    if (payload && payload->Size() > 0) {
        const uint8_t *payload_data = payload->Data();
        raw_data.insert(raw_data.end(), payload_data, payload_data + payload->Size());
    }

    return raw_data;
}

} // namespace lmshao::rtsp::mpegts
