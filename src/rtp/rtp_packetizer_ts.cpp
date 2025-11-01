/**
 * @author SHAO Liming <lmshao@163.com>
 * @copyright Copyright (c) 2025 SHAO Liming
 * @license MIT
 *
 * SPDX-License-Identifier: MIT
 */

#include "rtp_packetizer_ts.h"

#include "internal_logger.h"

namespace lmshao::lmrtsp {

void RtpPacketizerTs::SubmitFrame(const std::shared_ptr<MediaFrame> &frame)
{
    if (!frame || !frame->data || frame->data->Size() == 0) {
        LMRTSP_LOGW("SubmitFrame: invalid frame");
        return;
    }

    if (frame->media_type != MediaType::MP2T) {
        LMRTSP_LOGW("SubmitFrame: not MP2T media type");
        return;
    }

    const uint8_t *data = frame->data->Data();
    size_t size = frame->data->Size();

    LMRTSP_LOGD("Packetizing TS data: size=%zu, timestamp=%u", size, frame->timestamp);
    PacketizeTs(data, size, frame->timestamp);
}

void RtpPacketizerTs::PacketizeTs(const uint8_t *data, size_t size, uint32_t timestamp)
{
    auto l = listener_.lock();
    if (!l) {
        LMRTSP_LOGW("PacketizeTs: no listener");
        return;
    }

    // Calculate how many TS packets (188 bytes) fit in one RTP payload
    size_t maxTsPacketsPerRtp = (mtuSize_ - 12) / TS_PACKET_SIZE; // 12 bytes RTP header
    if (maxTsPacketsPerRtp == 0) {
        maxTsPacketsPerRtp = 1;
    }

    size_t offset = 0;
    while (offset < size) {
        // Calculate how many TS packets to include in this RTP packet
        size_t remainingBytes = size - offset;
        size_t remainingTsPackets = remainingBytes / TS_PACKET_SIZE;
        size_t tsPacketsInThisRtp = std::min(remainingTsPackets, maxTsPacketsPerRtp);

        if (tsPacketsInThisRtp == 0) {
            // Not enough data for a complete TS packet, skip remaining bytes
            LMRTSP_LOGW("Incomplete TS packet at end, skipping %zu bytes", remainingBytes);
            break;
        }

        size_t payloadSize = tsPacketsInThisRtp * TS_PACKET_SIZE;

        // Create RTP packet
        auto rtpPacket = std::make_shared<RtpPacket>();
        rtpPacket->version = 2;
        rtpPacket->padding = 0;
        rtpPacket->extension = 0;
        rtpPacket->csrc_count = 0;
        rtpPacket->marker = 0; // Usually 0 for TS streams
        rtpPacket->payload_type = payloadType_;
        rtpPacket->sequence_number = sequenceNumber_++;
        rtpPacket->timestamp = timestamp;
        rtpPacket->ssrc = ssrc_;

        // Set payload
        auto payload = std::make_shared<lmcore::DataBuffer>(payloadSize);
        payload->Assign(data + offset, payloadSize);
        rtpPacket->payload = payload;

        LMRTSP_LOGD("Created RTP packet: seq=%u, ts=%u, payload_size=%zu (TS packets=%zu)", rtpPacket->sequence_number,
                    rtpPacket->timestamp, payloadSize, tsPacketsInThisRtp);

        l->OnPacket(rtpPacket);

        offset += payloadSize;
    }

    LMRTSP_LOGD("TS packetization complete: total_bytes=%zu", size);
}

} // namespace lmshao::lmrtsp
