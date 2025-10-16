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
#include <cstring>
#include <vector>

// Platform-specific lmnet headers
#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
// Windows doesn't define these byte order macros
#ifndef __BYTE_ORDER
#define __LITTLE_ENDIAN 1234
#define __BIG_ENDIAN 4321
#define __BYTE_ORDER __LITTLE_ENDIAN
#endif
#else
#include <arpa/inet.h>
#include <endian.h>
#endif

namespace lmshao::lmrtsp {
// Represents the fixed-size RTP header.
// See RFC 3550 for details.
struct RtpHeader {
#if __BYTE_ORDER == __LITTLE_ENDIAN
    uint8_t csrc_count : 4;
    uint8_t extension : 1;
    uint8_t padding : 1;
    uint8_t version : 2;
    uint8_t payload_type : 7;
    uint8_t marker : 1;
#elif __BYTE_ORDER == __BIG_ENDIAN
    uint8_t version : 2;
    uint8_t padding : 1;
    uint8_t extension : 1;
    uint8_t csrc_count : 4;
    uint8_t marker : 1;
    uint8_t payload_type : 7;
#else
#error "Unsupported byte order"
#endif
    uint16_t sequence_number;
    uint32_t timestamp;
    uint32_t ssrc;
};

// Represents a full RTP packet, including header and payload.
class RtpPacket {
public:
    RtpHeader header;
    std::vector<uint32_t> csrc_list;
    std::vector<uint8_t> payload;

    RtpPacket()
    {
        header.version = 2;
        header.padding = 0;
        header.extension = 0;
        header.csrc_count = 0;
        header.marker = 0;
    }

    // Serialize the packet into a byte buffer for lmnet transmission.
    std::vector<uint8_t> serialize() const
    {
        std::vector<uint8_t> buffer(sizeof(RtpHeader));
        // Copy header and convert to lmnet byte order
        RtpHeader net_header = header;
        net_header.sequence_number = htons(header.sequence_number);
        net_header.timestamp = htonl(header.timestamp);
        net_header.ssrc = htonl(header.ssrc);

        memcpy(buffer.data(), &net_header, sizeof(RtpHeader));

        // Append payload
        buffer.insert(buffer.end(), payload.begin(), payload.end());

        return buffer;
    }

    // Parse a byte buffer from the lmnet into an RtpPacket.
    bool parse(const std::vector<uint8_t> &buffer)
    {
        if (buffer.size() < sizeof(RtpHeader)) {
            return false;
        }

        memcpy(&header, buffer.data(), sizeof(RtpHeader));

        // Convert from lmnet byte order
        header.sequence_number = ntohs(header.sequence_number);
        header.timestamp = ntohl(header.timestamp);
        header.ssrc = ntohl(header.ssrc);

        if (header.version != 2) {
            return false;
        }

        size_t header_size = sizeof(RtpHeader) + header.csrc_count * sizeof(uint32_t);
        if (buffer.size() < header_size) {
            return false;
        }

        // For simplicity, we are not parsing CSRC list and extension header for now.

        payload.assign(buffer.begin() + header_size, buffer.end());

        return true;
    }
};

} // namespace lmshao::lmrtsp

#endif // LMSHAO_LMRTSP_RTP_PACKET_H