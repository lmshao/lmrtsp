/**
 * @author SHAO Liming <lmshao@163.com>
 * @copyright Copyright (c) 2025 SHAO Liming
 * @license MIT
 *
 * SPDX-License-Identifier: MIT
 */

#include "rtp_packetizer_h265.h"

#include <algorithm>
#include <cstring>

#include "internal_logger.h"

namespace lmshao::lmrtsp {

static inline size_t RtpHeaderSize()
{
    return 12;
}

const uint8_t *RtpPacketizerH265::FindStartCode(const uint8_t *data, size_t size)
{
    if (!data || size < 3)
        return nullptr;
    for (size_t i = 0; i + 3 <= size; ++i) {
        if (data[i] == 0 && data[i + 1] == 0 && data[i + 2] == 1)
            return data + i;
        if (i + 4 <= size && data[i] == 0 && data[i + 1] == 0 && data[i + 2] == 0 && data[i + 3] == 1)
            return data + i;
    }
    return nullptr;
}

const uint8_t *RtpPacketizerH265::FindNextStartCode(const uint8_t *data, size_t size)
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

void RtpPacketizerH265::SubmitFrame(const std::shared_ptr<MediaFrame> &frame)
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
    uint32_t timestamp = frame->timestamp;

    LMRTSP_LOGI("Processing frame - size: %u, timestamp: %u", size, timestamp);

    const uint8_t *start = FindStartCode(data, size);
    if (!start) {
        LMRTSP_LOGE("No start code found");
        return;
    }

    LMRTSP_LOGI("Found start code, beginning NALU processing");

    int nalu_count = 0;
    while (start) {
        nalu_count++;
        LMRTSP_LOGI("Processing NALU #%d", nalu_count);

        const uint8_t *next = nullptr;

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

        size_t max_payload = mtuSize_ - RtpHeaderSize();
        if (nalu_size <= max_payload) {
            LMRTSP_LOGI("Using Single NALU packetization for NALU #%d", nalu_count);
            PacketizeSingleNalu(nalu_payload, nalu_size, timestamp, last_nalu);
        } else {
            LMRTSP_LOGI("Using FU packetization for NALU #%d", nalu_count);
            PacketizeFuA(nalu_payload, nalu_size, timestamp, last_nalu);
        }

        start = next;
    }

    LMRTSP_LOGI("SubmitFrame completed - processed %d NALUs", nalu_count);
}

void RtpPacketizerH265::PacketizeSingleNalu(const uint8_t *nalu, size_t nalu_size, uint32_t timestamp, bool last_nalu)
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

    auto l = listener_.lock();
    if (l) {
        l->OnPacket(packet);
    }
}

void RtpPacketizerH265::PacketizeFuA(const uint8_t *nalu, size_t nalu_size, uint32_t timestamp, bool last_nalu)
{
    if (!nalu || nalu_size < 2)
        return;

    // H.265 NAL header is 2 bytes
    // Format: |F(1)|Type(6)|LayerId(6)|TID(3)|
    uint8_t nal_header_byte1 = nalu[0];
    uint8_t nal_header_byte2 = nalu[1];

    // Extract NAL unit type (bits 1-6 of first byte)
    uint8_t nal_type = (nal_header_byte1 >> 1) & 0x3F;

    // FU payload header for H.265 (RFC 7798)
    // PayloadHdr (2 bytes) + FU header (1 byte)
    // PayloadHdr: Type=49 (FU), LayerId and TID from original
    uint8_t fu_indicator_byte1 = (nal_header_byte1 & 0x81) | (49 << 1); // Keep F and set Type=49
    uint8_t fu_indicator_byte2 = nal_header_byte2;                      // Keep LayerId and TID

    size_t max_fragment_size = mtuSize_ - RtpHeaderSize() - 3; // 3 bytes for PayloadHdr + FU header
    const uint8_t *payload_data = nalu + 2;                    // Skip original 2-byte NAL header
    size_t payload_size = nalu_size - 2;

    size_t offset = 0;
    bool first_fragment = true;

    while (offset < payload_size) {
        size_t fragment_size = std::min(max_fragment_size, payload_size - offset);
        bool last_fragment = (offset + fragment_size >= payload_size);

        // FU header (1 byte): S(1) | E(1) | FuType(6)
        uint8_t fu_header = nal_type; // FuType = original NAL type
        if (first_fragment) {
            fu_header |= 0x80; // Set Start bit
        }
        if (last_fragment) {
            fu_header |= 0x40; // Set End bit
        }

        auto packet = std::make_shared<RtpPacket>();
        packet->version = 2;
        packet->payload_type = payloadType_;
        packet->sequence_number = sequenceNumber_++;
        packet->timestamp = timestamp;
        packet->ssrc = ssrc_;
        packet->marker = (last_fragment && last_nalu) ? 1 : 0;

        // Construct FU payload: PayloadHdr (2 bytes) + FU header (1 byte) + fragment data
        auto payload = std::make_shared<lmcore::DataBuffer>(3 + fragment_size);
        payload->Assign(fu_indicator_byte1);
        payload->Append(fu_indicator_byte2);
        payload->Append(fu_header);
        payload->Append(payload_data + offset, fragment_size);

        packet->payload = payload;

        auto l = listener_.lock();
        if (l) {
            l->OnPacket(packet);
        }

        offset += fragment_size;
        first_fragment = false;
    }
}

} // namespace lmshao::lmrtsp
