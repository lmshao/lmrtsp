/**
 * @author SHAO Liming <lmshao@163.com>
 * @copyright Copyright (c) 2025 SHAO Liming
 * @license MIT
 *
 * SPDX-License-Identifier: MIT
 */

#include "rtp_depacketizer_h264.h"

#include <cstring>

namespace lmshao::lmrtsp {

static inline void AppendStartCode(std::vector<uint8_t> &dst)
{
    static const uint8_t sc[4] = {0x00, 0x00, 0x00, 0x01};
    dst.insert(dst.end(), sc, sc + 4);
}

void RtpDepacketizerH264::FlushFrame()
{
    auto l = listener_.lock();
    if (!have_frame_data_ || pending_.empty() || !l)
        return;

    auto buffer = std::make_shared<lmcore::DataBuffer>(pending_.size());
    std::memcpy(buffer->Data(), pending_.data(), pending_.size());

    auto frame = std::make_shared<MediaFrame>();
    frame->timestamp = current_timestamp_;
    frame->media_type = MediaType::H264;
    frame->data = buffer;

    l->OnFrame(frame);

    pending_.clear();
    have_frame_data_ = false;
    fua_active_ = false;
}

void RtpDepacketizerH264::SubmitPacket(const std::shared_ptr<RtpPacket> &packet)
{
    if (!packet)
        return;

    // If timestamp changes and we have a pending frame, flush previous
    if (have_frame_data_ && packet->timestamp != current_timestamp_) {
        FlushFrame();
    }

    current_timestamp_ = packet->timestamp;

    auto payload = packet->payload;
    if (!payload || payload->Size() == 0)
        return;

    const uint8_t *data = payload->Data();
    size_t size = payload->Size();

    if (size == 0)
        return;

    uint8_t nal_or_fu_indicator = data[0];
    uint8_t nal_type = nal_or_fu_indicator & 0x1F;

    if (nal_type >= 1 && nal_type <= 23) {
        // Single NALU
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
    }

    if (packet->marker) {
        // End of access unit
        FlushFrame();
    }
}

} // namespace lmshao::lmrtsp