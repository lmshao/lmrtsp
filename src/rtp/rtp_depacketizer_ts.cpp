/**
 * @author SHAO Liming <lmshao@163.com>
 * @copyright Copyright (c) 2025 SHAO Liming
 * @license MIT
 *
 * SPDX-License-Identifier: MIT
 */

#include "rtp_depacketizer_ts.h"

#include "internal_logger.h"

namespace lmshao::lmrtsp {

void RtpDepacketizerTs::SubmitPacket(const std::shared_ptr<RtpPacket> &packet)
{
    if (!packet) {
        LMRTSP_LOGD("SubmitPacket: packet is null");
        return;
    }

    LMRTSP_LOGD("SubmitPacket: timestamp=%u, seq=%u, marker=%d", packet->timestamp, packet->sequence_number,
                packet->marker);

    auto payload = packet->payload;
    if (!payload || payload->Size() == 0) {
        LMRTSP_LOGD("Empty payload, size: %zu", payload ? payload->Size() : 0);
        return;
    }

    const uint8_t *data = payload->Data();
    size_t size = payload->Size();

    ProcessTsData(data, size, packet->timestamp);
}

void RtpDepacketizerTs::ProcessTsData(const uint8_t *data, size_t size, uint32_t timestamp)
{
    auto l = listener_.lock();
    if (!l) {
        LMRTSP_LOGD("ProcessTsData: no listener");
        return;
    }

    // Validate that payload contains complete TS packets
    if (size % TS_PACKET_SIZE != 0) {
        LMRTSP_LOGW("Invalid TS payload size: %zu (not multiple of %zu)", size, TS_PACKET_SIZE);
        return;
    }

    size_t numTsPackets = size / TS_PACKET_SIZE;
    LMRTSP_LOGD("Processing %zu TS packets from RTP payload", numTsPackets);

    // Validate all TS packets have correct sync byte
    bool allValid = true;
    for (size_t i = 0; i < numTsPackets; i++) {
        const uint8_t *tsPacket = data + (i * TS_PACKET_SIZE);
        if (!ValidateTsPacket(tsPacket)) {
            LMRTSP_LOGW("Invalid TS packet at index %zu (sync byte missing)", i);
            allValid = false;
            break;
        }
    }

    if (!allValid) {
        return;
    }

    // Create MediaFrame with TS data
    auto buffer = std::make_shared<lmcore::DataBuffer>(size);
    buffer->Assign(data, size);

    auto frame = std::make_shared<MediaFrame>();
    frame->timestamp = timestamp;
    frame->media_type = MediaType::MP2T;
    frame->data = buffer;

    LMRTSP_LOGD("Delivering TS frame: size=%zu, ts=%u, packets=%zu", size, timestamp, numTsPackets);
    l->OnFrame(frame);
}

bool RtpDepacketizerTs::ValidateTsPacket(const uint8_t *packet)
{
    // Check sync byte (0x47)
    return packet[0] == TS_SYNC_BYTE;
}

} // namespace lmshao::lmrtsp
