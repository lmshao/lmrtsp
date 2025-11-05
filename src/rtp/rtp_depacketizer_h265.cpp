/**
 * @author SHAO Liming <lmshao@163.com>
 * @copyright Copyright (c) 2025 SHAO Liming
 * @license MIT
 *
 * SPDX-License-Identifier: MIT
 */

#include "rtp_depacketizer_h265.h"

#include "internal_logger.h"

namespace lmshao::lmrtsp {

static inline void AppendStartCode(std::vector<uint8_t> &dst)
{
    static const uint8_t sc[4] = {0x00, 0x00, 0x00, 0x01};
    dst.insert(dst.end(), sc, sc + 4);
}

void RtpDepacketizerH265::FlushFrame()
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
    frame->media_type = MediaType::H265;
    frame->data = buffer;

    LMRTSP_LOGD("Calling listener->OnFrame with frame size: %zu", pending_.size());
    l->OnFrame(frame);

    pending_.clear();
    haveFrameData_ = false;
    fuActive_ = false;
}

void RtpDepacketizerH265::ResetState()
{
    LMRTSP_LOGD("Resetting state due to packet loss");
    pending_.clear();
    haveFrameData_ = false;
    fuActive_ = false;
}

void RtpDepacketizerH265::SubmitPacket(const std::shared_ptr<RtpPacket> &packet)
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
            if (fuActive_) {
                LMRTSP_LOGD("Gap during FU - discarding incomplete frame");
                ResetState();
            }
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

    if (size < 2) {
        LMRTSP_LOGD("Payload too small: %zu bytes", size);
        return;
    }

    // H.265 NAL unit header is 2 bytes
    uint8_t nal_unit_type = (data[0] >> 1) & 0x3F; // Extract NAL unit type from first byte

    LMRTSP_LOGD("NAL unit type: %d, payload size: %zu", nal_unit_type, size);

    // H.265 NAL unit types: 0-47 (various types), 48=AP (Aggregation Packet), 49=FU (Fragmentation Unit)
    if (nal_unit_type <= 47) {
        // Single NAL unit packet
        LMRTSP_LOGD("Processing single NAL unit");
        AppendStartCode(pending_);
        pending_.insert(pending_.end(), data, data + size);
        haveFrameData_ = true;
        fuActive_ = false;
    } else if (nal_unit_type == 49 && size >= 3) {
        // FU (Fragmentation Unit) - RFC 7798
        uint8_t fu_header = data[2];
        bool start = (fu_header & 0x80) != 0;
        bool end = (fu_header & 0x40) != 0;
        uint8_t fu_type = fu_header & 0x3F;

        LMRTSP_LOGD("Processing FU: start=%d, end=%d, fu_type=%d", start, end, fu_type);

        // Reconstruct NAL unit header (2 bytes)
        // First byte: F(1) | Type(6) | LayerId(6) from PayloadHdr, but Type = fu_type
        // Second byte: TID(3) from PayloadHdr
        uint8_t reconstructed_nal_header[2];
        reconstructed_nal_header[0] = (data[0] & 0x81) | (fu_type << 1); // Keep F and LayerId[5], set Type
        reconstructed_nal_header[1] = data[1];                           // Keep LayerId[0-4] and TID

        const uint8_t *fragment = data + 3; // Skip PayloadHdr (2 bytes) + FU header (1 byte)
        size_t fragment_size = size - 3;

        if (start) {
            AppendStartCode(pending_);
            pending_.insert(pending_.end(), reconstructed_nal_header, reconstructed_nal_header + 2);
            fuActive_ = true;
        }

        if (fragment_size > 0) {
            pending_.insert(pending_.end(), fragment, fragment + fragment_size);
            haveFrameData_ = true;
        }

        if (end) {
            fuActive_ = false;
        }
    } else if (nal_unit_type == 48) {
        // AP (Aggregation Packet) - not commonly used in RTSP
        LMRTSP_LOGD("AP packet type not implemented: %d", nal_unit_type);
    } else {
        // PACI (50) or other unsupported types
        LMRTSP_LOGD("Unsupported NAL unit type: %d", nal_unit_type);
    }

    if (packet->marker) {
        // End of access unit
        LMRTSP_LOGD("Marker bit set, flushing frame");
        FlushFrame();
    }
}

} // namespace lmshao::lmrtsp
