/**
 * @author SHAO Liming <lmshao@163.com>
 * @copyright Copyright (c) 2025 SHAO Liming
 * @license MIT
 *
 * SPDX-License-Identifier: MIT
 */

#include "rtp_depacketizer_aac.h"

namespace lmshao::lmrtsp {

void RtpDepacketizerAac::FlushFrame()
{
    auto l = listener_.lock();
    if (!haveFrameData_ || pending_.empty() || !l)
        return;

    auto buffer = std::make_shared<lmcore::DataBuffer>(pending_.size());
    buffer->Assign(pending_.data(), pending_.size());

    auto frame = std::make_shared<MediaFrame>();
    frame->timestamp = currentTimestamp_;
    frame->media_type = MediaType::AAC;
    frame->data = buffer;

    l->OnFrame(frame);

    pending_.clear();
    haveFrameData_ = false;
}

void RtpDepacketizerAac::SubmitPacket(const std::shared_ptr<RtpPacket> &packet)
{
    if (!packet)
        return;

    if (haveFrameData_ && packet->timestamp != currentTimestamp_) {
        FlushFrame();
    }

    currentTimestamp_ = packet->timestamp;

    auto payload = packet->payload;
    if (!payload || payload->Size() == 0)
        return;

    const uint8_t *data = payload->Data();
    size_t size = payload->Size();

    pending_.insert(pending_.end(), data, data + size);
    haveFrameData_ = true;

    if (packet->marker) {
        FlushFrame();
    }
}

} // namespace lmshao::lmrtsp