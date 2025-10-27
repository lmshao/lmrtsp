/**
 * @author SHAO Liming <lmshao@163.com>
 * @copyright Copyright (c) 2025 SHAO Liming
 * @license MIT
 *
 * SPDX-License-Identifier: MIT
 */

#ifndef LMSHAO_LMRTSP_RTP_PACKET_H
#define LMSHAO_LMRTSP_RTP_PACKET_H

#include <cstdint>
#include <memory>
#include <vector>

#include "lmcore/data_buffer.h"

namespace lmshao::lmrtsp {

/**
 * RtpPacket provides a structured representation of an RTP packet and
 * utilities to serialize/deserialize using lmcore::DataBuffer.
 * - Header layout follows RFC 3550 (V/P/X/CC, M/PT, sequence, timestamp, SSRC)
 * - CSRC list, optional extension header, and payload are supported
 */
class RtpPacket {
public:
    // Fixed header fields (not using bitfields to avoid compiler packing issues)
    uint8_t version = 2;      // RTP version (must be 2)
    uint8_t padding = 0;      // Padding flag
    uint8_t extension = 0;    // Extension flag
    uint8_t csrc_count = 0;   // Number of CSRC identifiers
    uint8_t marker = 0;       // Marker bit
    uint8_t payload_type = 0; // Payload type (7 bits)

    uint16_t sequence_number = 0; // Sequence number
    uint32_t timestamp = 0;       // Timestamp
    uint32_t ssrc = 0;            // Synchronization source

    // Optional headers
    std::vector<uint32_t> csrc_list;     // CSRC identifiers
    uint16_t extension_profile = 0;      // Extension header profile (if extension == 1)
    std::vector<uint8_t> extension_data; // Extension data (length must be a multiple of 4)

    // Payload stored in a DataBuffer
    std::shared_ptr<lmcore::DataBuffer> payload; // RTP payload

    RtpPacket() = default;

    // Compute the serialized header size (without payload)
    size_t HeaderSize() const;

    // Total packet size after serialization (header + payload)
    size_t Size() const;

    // Basic validity checks before serialization
    bool Validate() const;

    // Serialize to a DataBuffer in network byte order
    std::shared_ptr<lmcore::DataBuffer> Serialize() const;

    // Parse from a raw buffer (char* + length) in network byte order
    bool Parse(char *data, size_t length);

    // Parse from a DataBuffer in network byte order
    bool Parse(const lmcore::DataBuffer &buf);

    // Convenience: deserialize and return a shared_ptr
    static std::shared_ptr<RtpPacket> Deserialize(char *data, size_t length);
    static std::shared_ptr<RtpPacket> Deserialize(const lmcore::DataBuffer &buf);
};

} // namespace lmshao::lmrtsp

#endif // LMSHAO_LMRTSP_RTP_PACKET_H