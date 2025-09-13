/**
 * @author SHAO Liming <lmshao@163.com>
 * @copyright Copyright (c) 2025 SHAO Liming
 * @license MIT
 *
 * SPDX-License-Identifier: MIT
 */

#include "lmrtp/aac_packetizer.h"

#include "internal_logger.h"

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <arpa/inet.h>
#endif

namespace lmshao::lmrtp {

AacPacketizer::AacPacketizer(uint32_t ssrc, uint16_t sequence_number, uint32_t timestamp, uint32_t mtu_size)
    : ssrc_(ssrc), sequence_number_(sequence_number), timestamp_(timestamp), mtu_size_(mtu_size)
{
    RTP_LOGD("AacPacketizer created: SSRC=0x%08X, MTU=%u", ssrc, mtu_size);
}

std::vector<RtpPacket> AacPacketizer::packetize(const MediaFrame &frame)
{
    RTP_LOGD("AacPacketizer: packetizing AAC frame, size: %zu", frame.data.size());
    std::vector<RtpPacket> packets;
    const uint8_t *frame_data = frame.data.data();
    size_t frame_size = frame.data.size();

    // Assuming the frame contains a single ADTS frame
    // ADTS header is 7 bytes, we need to skip it
    if (frame_size <= 7) {
        RTP_LOGW("AacPacketizer: frame too small (%zu bytes), skipping", frame_size);
        return packets;
    }

    const uint8_t *payload_data = frame_data + 7;
    size_t payload_size = frame_size - 7;

    if (payload_size <= mtu_size_ - 12 - 4) { // 12 for RTP header, 4 for AU header
        RtpPacket packet;
        packet.header.version = 2;
        packet.header.padding = 0;
        packet.header.extension = 0;
        packet.header.csrc_count = 0;
        packet.header.marker = 1;        // AAC frames are usually sent in a single packet
        packet.header.payload_type = 97; // Dynamic payload type for AAC
        packet.header.sequence_number = htons(sequence_number_++);
        packet.header.timestamp = htonl(timestamp_);
        packet.header.ssrc = htonl(ssrc_);

        // AU Header
        uint8_t au_header[4];
        au_header[0] = 0x00;
        au_header[1] = 0x10;
        au_header[2] = static_cast<uint8_t>((payload_size & 0x1FE0) >> 5);
        au_header[3] = static_cast<uint8_t>((payload_size & 0x1F) << 3);

        packet.payload.assign(au_header, au_header + 4);
        packet.payload.insert(packet.payload.end(), payload_data, payload_data + payload_size);
        packets.push_back(std::move(packet));
    } else {
        // Fragmentation for AAC is more complex and not implemented here
        RTP_LOGW("AacPacketizer: frame too large for single packet, fragmentation not implemented");
    }

    RTP_LOGD("AacPacketizer: generated %zu RTP packets", packets.size());
    return packets;
}

} // namespace lmshao::lmrtp