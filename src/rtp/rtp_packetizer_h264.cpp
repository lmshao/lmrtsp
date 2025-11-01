/**
 * @author SHAO Liming <lmshao@163.com>
 * @copyright Copyright (c) 2025 SHAO Liming
 * @license MIT
 *
 * SPDX-License-Identifier: MIT
 */

#include "rtp_packetizer_h264.h"

#include <algorithm>
#include <cstring>

#include "internal_logger.h"

namespace lmshao::lmrtsp {

static inline size_t RtpHeaderSize()
{
    return 12; // fixed RTP header without extensions
}

const uint8_t *RtpPacketizerH264::FindStartCode(const uint8_t *data, size_t size)
{
    if (!data || size < 3)
        return nullptr;
    for (size_t i = 0; i + 3 <= size; ++i) {
        // 0x000001
        if (data[i] == 0 && data[i + 1] == 0 && data[i + 2] == 1)
            return data + i;
        // 0x00000001
        if (i + 4 <= size && data[i] == 0 && data[i + 1] == 0 && data[i + 2] == 0 && data[i + 3] == 1)
            return data + i;
    }
    return nullptr;
}

const uint8_t *RtpPacketizerH264::FindNextStartCode(const uint8_t *data, size_t size)
{
    if (!data || size < 3)
        return nullptr;
    for (size_t i = 1; i + 3 <= size; ++i) {
        if (data[i] == 0 && data[i + 1] == 0 && data[i + 2] == 1)
            return data + i;
        if (i + 4 <= size && data[i] == 0 && data[i + 1] == 0 && data[i + 2] == 0 && data[i + 3] == 1)
            return data + i;
    }
    return nullptr;
}

void RtpPacketizerH264::SubmitFrame(const std::shared_ptr<MediaFrame> &frame)
{
    LMRTSP_LOGI("SubmitFrame called - frame: %s", frame ? "valid" : "null");

    auto l = listener_.lock();
    if (!l || !frame || !frame->data) {
        LMRTSP_LOGE("SubmitFrame failed - listener: %s, frame: %s, frame->data: %s", l ? "valid" : "null",
                    frame ? "valid" : "null", (frame && frame->data) ? "valid" : "null");
        return;
    }

    auto buf = frame->data;
    const uint8_t *data = buf->Data();
    size_t size = buf->Size();
    uint32_t timestamp = frame->timestamp; // assume already in 90kHz or precomputed

    LMRTSP_LOGI("Processing frame - size: %u, timestamp: %u", size, timestamp);

    const uint8_t *start = FindStartCode(data, size);
    if (!start) {
        LMRTSP_LOGE("No start code found, treat entire buffer as a single NALU without start code");
        return;
    }

    LMRTSP_LOGI("Found start code, beginning NALU processing");

    // Iterate over NAL units by start codes
    int nalu_count = 0;
    while (start) {
        nalu_count++;
        LMRTSP_LOGI("Processing NALU #%d", nalu_count);

        const uint8_t *next = nullptr;

        // Skip current start code to find next one
        size_t skip_bytes = 0;
        if (start + 3 <= data + size && start[0] == 0 && start[1] == 0 && start[2] == 1) {
            skip_bytes = 3;
        } else if (start + 4 <= data + size && start[0] == 0 && start[1] == 0 && start[2] == 0 && start[3] == 1) {
            skip_bytes = 4;
        }

        if (skip_bytes > 0 && start + skip_bytes < data + size) {
            next = FindNextStartCode(start + skip_bytes, data + size - (start + skip_bytes));
        }
        const uint8_t *nalu_payload = nullptr;
        size_t nalu_size = 0;

        // Determine NAL payload start by skipping start code length (3 or 4 bytes)
        if (start + 3 <= data + size && start[0] == 0 && start[1] == 0 && start[2] == 1) {
            nalu_payload = start + 3;
        } else if (start + 4 <= data + size && start[0] == 0 && start[1] == 0 && start[2] == 0 && start[3] == 1) {
            nalu_payload = start + 4;
        }

        if (!nalu_payload) {
            LMRTSP_LOGE("Invalid NALU payload for NALU #%d", nalu_count);
            break;
        }

        if (next) {
            nalu_size = static_cast<size_t>(next - nalu_payload);
        } else {
            nalu_size = static_cast<size_t>(data + size - nalu_payload);
        }

        bool last_nalu = (next == nullptr);
        LMRTSP_LOGI("NALU #%d - size: %u, last_nalu: %s", nalu_count, nalu_size, last_nalu ? "true" : "false");

        // Use Single NALU if fits MTU, otherwise FU-A
        size_t max_payload = mtuSize_ - RtpHeaderSize();
        if (nalu_size <= max_payload) {
            LMRTSP_LOGI("Using Single NALU packetization for NALU #%d", nalu_count);
            PacketizeSingleNalu(nalu_payload, nalu_size, timestamp, last_nalu);
        } else {
            LMRTSP_LOGI("Using FU-A packetization for NALU #%d", nalu_count);
            PacketizeFuA(nalu_payload, nalu_size, timestamp, last_nalu);
        }

        start = next;
    }

    LMRTSP_LOGI("SubmitFrame completed - processed %d NALUs", nalu_count);
}

void RtpPacketizerH264::PacketizeSingleNalu(const uint8_t *nalu, size_t nalu_size, uint32_t timestamp, bool last_nalu)
{
    if (!nalu || nalu_size == 0)
        return;

    auto packet = std::make_shared<RtpPacket>();
    packet->version = 2;
    packet->payload_type = payloadType_;
    packet->sequence_number = sequenceNumber_++;
    packet->timestamp = timestamp;
    packet->ssrc = ssrc_;
    packet->marker = last_nalu ? 1 : 0;

    auto payload = std::make_shared<lmcore::DataBuffer>(nalu_size);
    payload->Assign(nalu, nalu_size);
    packet->payload = payload;

    if (auto l = listener_.lock()) {
        LMRTSP_LOGI("Sent RTP packet with SSRC %u, seq %u, timestamp %u, payload type %u, size %u", packet->ssrc,
                    packet->sequence_number, packet->timestamp, packet->payload_type, payload->Size());
        l->OnPacket(packet);
    }
}

void RtpPacketizerH264::PacketizeFuA(const uint8_t *nalu, size_t nalu_size, uint32_t timestamp, bool last_nalu)
{
    if (!nalu || nalu_size <= 1)
        return;

    const uint8_t nal_header = nalu[0];
    const uint8_t F = (nal_header & 0x80) >> 7;
    const uint8_t NRI = (nal_header & 0x60) >> 5;
    const uint8_t type = nal_header & 0x1F;

    // FU-A: indicator (F,NRI,Type=28), header (S/E/R=0, Type)
    const uint8_t fu_indicator = static_cast<uint8_t>((F << 7) | (NRI << 5) | 28);

    // Available payload per fragment (excluding 2 bytes FU headers)
    size_t max_fragment = mtuSize_ - RtpHeaderSize() - 2;
    if (max_fragment <= 0)
        return;

    // Skip original NAL header, start fragment at nalu+1
    size_t offset = 1;
    size_t remaining = nalu_size - offset;
    bool first = true;

    while (remaining > 0) {
        size_t chunk = std::min(remaining, max_fragment);
        bool is_last_fragment = (remaining <= max_fragment);

        uint8_t fu_header =
            static_cast<uint8_t>(((first ? 1 : 0) << 7) | ((is_last_fragment ? 1 : 0) << 6) | (0 << 5) | (type & 0x1F));

        auto packet = std::make_shared<RtpPacket>();
        packet->version = 2;
        packet->payload_type = payloadType_;
        packet->sequence_number = sequenceNumber_++;
        packet->timestamp = timestamp;
        packet->ssrc = ssrc_;
        packet->marker = (is_last_fragment && last_nalu) ? 1 : 0;

        auto payload = std::make_shared<lmcore::DataBuffer>(chunk + 2);
        payload->Assign((uint8_t)fu_indicator);
        payload->Append((uint8_t)fu_header);
        payload->Append(nalu + offset, chunk);
        packet->payload = payload;

        if (auto l = listener_.lock()) {
            LMRTSP_LOGI("Sent RTP packet with SSRC %u, seq %u, timestamp %u, payload type %u, size %u", packet->ssrc,
                        packet->sequence_number, packet->timestamp, packet->payload_type, payload->Size());
            l->OnPacket(packet);
        }

        offset += chunk;
        remaining -= chunk;
        first = false;
    }
}

} // namespace lmshao::lmrtsp