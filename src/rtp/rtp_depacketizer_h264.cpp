/**
 * @author SHAO Liming <lmshao@163.com>
 * @copyright Copyright (c) 2025 SHAO Liming
 * @license MIT
 *
 * SPDX-License-Identifier: MIT
 */

#include "rtp_depacketizer_h264.h"

#include <cstdio>
#include <cstring>

namespace lmshao::lmrtsp {

static inline void AppendStartCode(std::vector<uint8_t> &dst)
{
    static const uint8_t sc[4] = {0x00, 0x00, 0x00, 0x01};
    dst.insert(dst.end(), sc, sc + 4);
}

void RtpDepacketizerH264::FlushFrame()
{
    printf("[H264Depacketizer] FlushFrame called\n");
    printf("[H264Depacketizer] have_frame_data_: %s\n", have_frame_data_ ? "true" : "false");
    printf("[H264Depacketizer] pending_.size(): %zu\n", pending_.size());

    auto l = listener_.lock();
    printf("[H264Depacketizer] listener_.lock() result: %s\n", l ? "valid" : "null");

    if (!have_frame_data_ || pending_.empty() || !l) {
        printf("[H264Depacketizer] Early return - have_frame_data_: %s, pending_.empty(): %s, listener: %s\n",
               have_frame_data_ ? "true" : "false", pending_.empty() ? "true" : "false", l ? "valid" : "null");
        return;
    }

    auto buffer = std::make_shared<lmcore::DataBuffer>(pending_.size());
    buffer->Assign(pending_.data(), pending_.size());

    auto frame = std::make_shared<MediaFrame>();
    frame->timestamp = current_timestamp_;
    frame->media_type = MediaType::H264;
    frame->data = buffer;

    printf("[H264Depacketizer] Calling listener->OnFrame with frame size: %zu\n", pending_.size());
    l->OnFrame(frame);

    pending_.clear();
    have_frame_data_ = false;
    fua_active_ = false;
}

void RtpDepacketizerH264::ResetState()
{
    printf("[H264Depacketizer] Resetting state due to packet loss\n");
    pending_.clear();
    have_frame_data_ = false;
    fua_active_ = false;
}

void RtpDepacketizerH264::SubmitPacket(const std::shared_ptr<RtpPacket> &packet)
{
    if (!packet) {
        printf("[H264Depacketizer] SubmitPacket: packet is null\n");
        return;
    }

    printf("[H264Depacketizer] SubmitPacket: timestamp=%u, seq=%u, marker=%d\n", packet->timestamp,
           packet->sequence_number, packet->marker);

    // Check for sequence number gap
    if (sequence_initialized_) {
        uint16_t expected_seq = static_cast<uint16_t>(last_sequence_number_ + 1);
        if (packet->sequence_number != expected_seq) {
            printf("[H264Depacketizer] Sequence gap detected: got %u, expected %u\n", packet->sequence_number,
                   expected_seq);
            // Only discard if we're in the middle of a fragmented unit (FU-A)
            if (fua_active_) {
                printf("[H264Depacketizer] Gap during FU-A - discarding incomplete frame\n");
                ResetState();
            }
            // If not in FU-A, we can continue safely
        }
    }
    last_sequence_number_ = packet->sequence_number;
    sequence_initialized_ = true;

    // If timestamp changes and we have a pending frame, flush previous
    if (have_frame_data_ && packet->timestamp != current_timestamp_) {
        printf("[H264Depacketizer] Timestamp changed, flushing previous frame\n");
        FlushFrame();
    }

    current_timestamp_ = packet->timestamp;

    auto payload = packet->payload;
    if (!payload || payload->Size() == 0) {
        printf("[H264Depacketizer] Empty payload, size: %zu\n", payload ? payload->Size() : 0);
        return;
    }

    const uint8_t *data = payload->Data();
    size_t size = payload->Size();

    if (size == 0) {
        printf("[H264Depacketizer] Zero size payload\n");
        return;
    }

    uint8_t nal_or_fu_indicator = data[0];
    uint8_t nal_type = nal_or_fu_indicator & 0x1F;

    printf("[H264Depacketizer] NAL type: %d, payload size: %zu\n", nal_type, size);

    if (nal_type >= 1 && nal_type <= 23) {
        // Single NALU
        printf("[H264Depacketizer] Processing single NALU\n");
        AppendStartCode(pending_);
        pending_.insert(pending_.end(), data, data + size);
        have_frame_data_ = true;
        fua_active_ = false;
    } else if (nal_type == 28 && size >= 2) {
        // FU-A
        uint8_t fu_header = data[1];
        bool start = (fu_header & 0x80) != 0;
        bool end = (fu_header & 0x40) != 0;
        uint8_t original_nal_type = fu_header & 0x1F;

        printf("[H264Depacketizer] Processing FU-A: start=%d, end=%d, original_nal_type=%d\n", start, end,
               original_nal_type);

        uint8_t F = (nal_or_fu_indicator & 0x80) >> 7;
        uint8_t NRI = (nal_or_fu_indicator & 0x60) >> 5;
        uint8_t reconstructed_nal_header = static_cast<uint8_t>((F << 7) | (NRI << 5) | (original_nal_type & 0x1F));

        const uint8_t *fragment = data + 2;
        size_t fragment_size = size - 2;

        if (start) {
            AppendStartCode(pending_);
            pending_.push_back(reconstructed_nal_header);
            fua_active_ = true;
        }

        if (fragment_size > 0) {
            pending_.insert(pending_.end(), fragment, fragment + fragment_size);
            have_frame_data_ = true;
        }

        if (end) {
            fua_active_ = false;
        }
    } else {
        // Unsupported NAL types like STAP-A (24), etc. For simplicity, ignore.
        printf("[H264Depacketizer] Unsupported NAL type: %d\n", nal_type);
    }

    if (packet->marker) {
        // End of access unit
        printf("[H264Depacketizer] Marker bit set, flushing frame\n");
        FlushFrame();
    }
}

} // namespace lmshao::lmrtsp