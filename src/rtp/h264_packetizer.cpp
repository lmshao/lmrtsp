/**
 * @author SHAO Liming <lmshao@163.com>
 * @copyright Copyright (c) 2025 SHAO Liming
 * @license MIT
 *
 * SPDX-License-Identifier: MIT
 */

#include "lmrtp/h264_packetizer.h"

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <arpa/inet.h>
#endif

#include <algorithm>
#include <iostream>

#include "internal_logger.h"

namespace lmshao::lmrtp {

namespace {
// Finds the start of a NAL unit. Returns a pointer to the first byte of the NAL unit payload (after the start code).
const uint8_t *find_nalu_start(const uint8_t *data, size_t size)
{
    if (size < 4) {
        return nullptr;
    }
    for (size_t i = 0; i <= size - 4; ++i) {
        if (data[i] == 0 && data[i + 1] == 0 && data[i + 2] == 1) {
            return &data[i + 3];
        }
        if (data[i] == 0 && data[i + 1] == 0 && data[i + 2] == 0 && data[i + 3] == 1) {
            return &data[i + 4];
        }
    }
    return nullptr;
}
} // namespace

H264Packetizer::H264Packetizer(uint32_t ssrc, uint16_t sequence_number, uint32_t timestamp, uint32_t mtu_size)
    : ssrc_(ssrc), sequence_number_(sequence_number), timestamp_(timestamp), mtu_size_(mtu_size)
{
    RTP_LOGD("H264Packetizer created: SSRC=0x%08X, MTU=%u", ssrc, mtu_size);
}

std::vector<RtpPacket> H264Packetizer::packetize(const MediaFrame &frame)
{
    packets_.clear();
    const uint8_t *frame_data = frame.data.data();
    size_t frame_size = frame.data.size();

    RTP_LOGD("H264Packetizer: packetizing frame, size: %zu", frame_size);

    const uint8_t *nalu_start = find_nalu_start(frame_data, frame_size);
    while (nalu_start) {
        const uint8_t *next_nalu_start = find_nalu_start(nalu_start, frame_size - (nalu_start - frame_data));
        size_t nalu_size = (next_nalu_start) ? (next_nalu_start - nalu_start - (next_nalu_start[-1] == 0 ? 4 : 3))
                                             : (frame_size - (nalu_start - frame_data));

        if (nalu_size <= mtu_size_ - 12) { // 12 bytes for RTP header
            PacketizeSingleNalu(nalu_start, nalu_size);
        } else {
            PacketizeFuA(nalu_start, nalu_size);
        }

        nalu_start = next_nalu_start;
    }

    if (!packets_.empty()) {
        packets_.back().header.marker = 1;
    }

    RTP_LOGD("H264Packetizer: generated %zu RTP packets", packets_.size());
    return packets_;
}

void H264Packetizer::PacketizeSingleNalu(const uint8_t *nalu, size_t nalu_size)
{
    RtpPacket packet;
    packet.header.version = 2;
    packet.header.padding = 0;
    packet.header.extension = 0;
    packet.header.csrc_count = 0;
    packet.header.marker = 0;        // Will be set for the last packet of the frame
    packet.header.payload_type = 96; // Dynamic payload type for H.264
    packet.header.sequence_number = htons(sequence_number_++);
    packet.header.timestamp = htonl(timestamp_);
    packet.header.ssrc = htonl(ssrc_);

    packet.payload.assign(nalu, nalu + nalu_size);
    packets_.push_back(std::move(packet));
}

void H264Packetizer::PacketizeFuA(const uint8_t *nalu, size_t nalu_size)
{
    uint8_t nalu_header = nalu[0];
    const uint8_t *nalu_data = nalu + 1;
    size_t nalu_data_size = nalu_size - 1;

    size_t max_payload_size = mtu_size_ - 12 - 2; // 12 for RTP header, 2 for FU-A headers

    size_t offset = 0;
    while (offset < nalu_data_size) {
        size_t payload_size = std::min<size_t>(max_payload_size, nalu_data_size - offset);

        RtpPacket packet;
        packet.header.version = 2;
        packet.header.padding = 0;
        packet.header.extension = 0;
        packet.header.csrc_count = 0;
        packet.header.marker = 0;
        packet.header.payload_type = 96;
        packet.header.sequence_number = htons(sequence_number_++);
        packet.header.timestamp = htonl(timestamp_);
        packet.header.ssrc = htonl(ssrc_);

        // FU indicator
        uint8_t fu_indicator = (nalu_header & 0xE0) | 28; // FU-A
        packet.payload.push_back(fu_indicator);

        // FU header
        uint8_t fu_header = nalu_header & 0x1F;
        if (offset == 0) { // Start bit
            fu_header |= 0x80;
        }
        if (offset + payload_size >= nalu_data_size) { // End bit
            fu_header |= 0x40;
        }
        packet.payload.push_back(fu_header);

        packet.payload.insert(packet.payload.end(), nalu_data + offset, nalu_data + offset + payload_size);
        packets_.push_back(std::move(packet));

        offset += payload_size;
    }
}

} // namespace lmshao::lmrtp