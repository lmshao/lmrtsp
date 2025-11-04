/**
 * @author SHAO Liming <lmshao@163.com>
 * @copyright Copyright (c) 2025 SHAO Liming
 * @license MIT
 *
 * SPDX-License-Identifier: MIT
 */

#include "rtp_packetizer_aac.h"

#include <algorithm>
#include <cstring>

namespace lmshao::lmrtsp {

static inline size_t RtpHeaderSize()
{
    return 12; // fixed RTP header without extensions
}

void RtpPacketizerAac::SubmitFrame(const std::shared_ptr<MediaFrame> &frame)
{
    auto l = listener_.lock();
    if (!l || !frame || !frame->data)
        return;

    auto buf = frame->data;
    const uint8_t *data = buf->Data();
    size_t size = buf->Size();
    uint32_t timestamp = frame->timestamp;

    // RFC 3640 AAC-hbr mode: Add AU headers section
    // AU-headers-length (16 bits) + AU-header (16 bits: 13 bits size + 3 bits index)
    // For single AU per packet: AU-headers-length = 16 (bits), AU-size = frame size

    size_t au_header_section_size = 4; // 2 bytes for AU-headers-length + 2 bytes for AU-header
    size_t max_payload = mtuSize_ - RtpHeaderSize() - au_header_section_size;

    if (max_payload <= 0 || size > max_payload) {
        // Frame too large for single packet, skip for now
        // TODO: implement fragmentation with multiple AUs
        return;
    }

    // Build RTP packet with AU headers
    auto packet = std::make_shared<RtpPacket>();
    packet->version = 2;
    packet->payload_type = payloadType_;
    packet->sequence_number = sequenceNumber_++;
    packet->timestamp = timestamp;
    packet->ssrc = ssrc_;
    packet->marker = 1; // Single complete AU

    // Construct payload: AU-headers-length + AU-header + AAC frame data
    auto payload = std::make_shared<lmcore::DataBuffer>(au_header_section_size + size);
    uint8_t *p = payload->Data();

    // AU-headers-length in bits (16 bits for one AU-header)
    p[0] = 0x00;
    p[1] = 0x10; // 16 bits

    // AU-header: 13 bits size + 3 bits AU-Index (0)
    // Size is in bytes, left-aligned in 16 bits
    uint16_t au_size = static_cast<uint16_t>(size);
    p[2] = (au_size >> 5) & 0xFF; // Upper 8 bits of 13-bit size
    p[3] = (au_size << 3) & 0xFF; // Lower 5 bits of size + 3 bits index (0)

    // Copy AAC frame data
    std::memcpy(p + 4, data, size);
    payload->SetSize(au_header_section_size + size);

    packet->payload = payload;

    if (auto l2 = listener_.lock())
        l2->OnPacket(packet);
}

} // namespace lmshao::lmrtsp