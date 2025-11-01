/**
 * @author SHAO Liming <lmshao@163.com>
 * @copyright Copyright (c) 2025 SHAO Liming
 * @license MIT
 *
 * SPDX-License-Identifier: MIT
 */

#include "rtp_depacketizer_h264.h"

#include "internal_logger.h"

namespace lmshao::lmrtsp {

static inline void AppendStartCode(std::vector<uint8_t> &dst)
{
    static const uint8_t sc[4] = {0x00, 0x00, 0x00, 0x01};
    dst.insert(dst.end(), sc, sc + 4);
}

void RtpDepacketizerH264::FlushFrame()
{
    LMRTSP_LOGD("FlushFrame called");
    LMRTSP_LOGD("haveFrameData_: %s", haveFrameData_ ? "true" : "false");
    LMRTSP_LOGD("pending_.size(): %zu", pending_.size());

    auto l = listener_.lock();
    LMRTSP_LOGD("listener_.lock() result: %s", l ? "valid" : "null");

    if (!haveFrameData_ || pending_.empty() || !l) {
        LMRTSP_LOGD("Early return - haveFrameData_: %s, pending_.empty(): %s, listener: %s",
                    haveFrameData_ ? "true" : "false", pending_.empty() ? "true" : "false", l ? "valid" : "null");
        return;
    }

    auto buffer = std::make_shared<lmcore::DataBuffer>(pending_.size());
    buffer->Assign(pending_.data(), pending_.size());

    auto frame = std::make_shared<MediaFrame>();
    frame->timestamp = currentTimestamp_;
    frame->media_type = MediaType::H264;
    frame->data = buffer;

    LMRTSP_LOGD("Calling listener->OnFrame with frame size: %zu", pending_.size());
    l->OnFrame(frame);

    pending_.clear();
    haveFrameData_ = false;
    fuaActive_ = false;
}

void RtpDepacketizerH264::ResetState()
{
    LMRTSP_LOGD("Resetting state due to packet loss");
    pending_.clear();
    haveFrameData_ = false;
    fuaActive_ = false;
}

void RtpDepacketizerH264::SubmitPacket(const std::shared_ptr<RtpPacket> &packet)
{
    if (!packet) {
        LMRTSP_LOGD("SubmitPacket: packet is null");
        return;
    }

    LMRTSP_LOGD("SubmitPacket: timestamp=%u, seq=%u, marker=%d", packet->timestamp, packet->sequence_number,
                packet->marker);

    // Check for sequence number gap
    if (sequenceInitialized_) {
        uint16_t expected_seq = static_cast<uint16_t>(lastSequenceNumber_ + 1);
        if (packet->sequence_number != expected_seq) {
            LMRTSP_LOGD("Sequence gap detected: got %u, expected %u", packet->sequence_number, expected_seq);
            // Only discard if we're in the middle of a fragmented unit (FU-A)
            if (fuaActive_) {
                LMRTSP_LOGD("Gap during FU-A - discarding incomplete frame");
                ResetState();
            }
            // If not in FU-A, we can continue safely
        }
    }
    lastSequenceNumber_ = packet->sequence_number;
    sequenceInitialized_ = true;

    // If timestamp changes and we have a pending frame, flush previous
    if (haveFrameData_ && packet->timestamp != currentTimestamp_) {
        LMRTSP_LOGD("Timestamp changed, flushing previous frame");
        FlushFrame();
    }

    currentTimestamp_ = packet->timestamp;

    auto payload = packet->payload;
    if (!payload || payload->Size() == 0) {
        LMRTSP_LOGD("Empty payload, size: %zu", payload ? payload->Size() : 0);
        return;
    }

    const uint8_t *data = payload->Data();
    size_t size = payload->Size();

    if (size == 0) {
        LMRTSP_LOGD("Zero size payload");
        return;
    }

    uint8_t nal_or_fu_indicator = data[0];
    uint8_t nal_type = nal_or_fu_indicator & 0x1F;

    LMRTSP_LOGD("NAL type: %d, payload size: %zu", nal_type, size);

    if (nal_type >= 1 && nal_type <= 23) {
        // Single NALU
        LMRTSP_LOGD("Processing single NALU");
        AppendStartCode(pending_);
        pending_.insert(pending_.end(), data, data + size);
        haveFrameData_ = true;
        fuaActive_ = false;
    } else if (nal_type == 28 && size >= 2) {
        // FU-A
        uint8_t fu_header = data[1];
        bool start = (fu_header & 0x80) != 0;
        bool end = (fu_header & 0x40) != 0;
        uint8_t original_nal_type = fu_header & 0x1F;

        LMRTSP_LOGD("Processing FU-A: start=%d, end=%d, original_nal_type=%d", start, end, original_nal_type);

        uint8_t F = (nal_or_fu_indicator & 0x80) >> 7;
        uint8_t NRI = (nal_or_fu_indicator & 0x60) >> 5;
        uint8_t reconstructed_nal_header = static_cast<uint8_t>((F << 7) | (NRI << 5) | (original_nal_type & 0x1F));

        const uint8_t *fragment = data + 2;
        size_t fragment_size = size - 2;

        if (start) {
            AppendStartCode(pending_);
            pending_.push_back(reconstructed_nal_header);
            fuaActive_ = true;
        }

        if (fragment_size > 0) {
            pending_.insert(pending_.end(), fragment, fragment + fragment_size);
            haveFrameData_ = true;
        }

        if (end) {
            fuaActive_ = false;
        }
    } else {
        // Unsupported NAL types like STAP-A (24), etc. For simplicity, ignore.
        LMRTSP_LOGD("Unsupported NAL type: %d", nal_type);
    }

    if (packet->marker) {
        // End of access unit
        LMRTSP_LOGD("Marker bit set, flushing frame");
        FlushFrame();
    }
}

} // namespace lmshao::lmrtsp