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
    uint32_t timestamp = frame->timestamp; // assume AAC timestamp provided by upstream

    // Simple fragmentation without RFC3640 AU headers (basic payloading).
    // If needed later, AU headers can be added.
    size_t max_payload = mtuSize_ - RtpHeaderSize();
    if (max_payload <= 0)
        return;

    size_t offset = 0;
    while (offset < size) {
        size_t chunk = std::min(max_payload, size - offset);
        bool is_last = (offset + chunk) >= size;

        auto packet = std::make_shared<RtpPacket>();
        packet->version = 2;
        packet->payload_type = payloadType_;
        packet->sequence_number = sequenceNumber_++;
        packet->timestamp = timestamp;
        packet->ssrc = ssrc_;
        packet->marker = is_last ? 1 : 0;

        auto payload = std::make_shared<lmcore::DataBuffer>(chunk);
        payload->Assign(data + offset, chunk);
        packet->payload = payload;

        if (auto l2 = listener_.lock())
            l2->OnPacket(packet);

        offset += chunk;
    }
}

} // namespace lmshao::lmrtsp