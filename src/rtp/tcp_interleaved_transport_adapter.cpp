/**
 * @author SHAO Liming <lmshao@163.com>
 * @copyright Copyright (c) 2025 SHAO Liming
 * @license MIT
 *
 * SPDX-License-Identifier: MIT
 */

#include "tcp_interleaved_transport_adapter.h"

#include <sstream>

#include "internal_logger.h"
#include "lmrtsp/rtsp_server_session.h"

namespace lmshao::lmrtsp {

TcpInterleavedTransportAdapter::TcpInterleavedTransportAdapter(std::weak_ptr<RtspServerSession> RtspServerSession)
    : RtspServerSession_(RtspServerSession), rtpChannel_(0), rtcpChannel_(1), isSetup_(false)
{
}

TcpInterleavedTransportAdapter::~TcpInterleavedTransportAdapter()
{
    Close();
}

bool TcpInterleavedTransportAdapter::Setup(const TransportConfig &config)
{
    // Validate if RTSP session is valid
    auto session = RtspServerSession_.lock();
    if (!session) {
        LMRTSP_LOGE("TCP interleaved Setup failed: RTSP session expired");
        return false;
    }

    // Set channel numbers
    rtpChannel_ = config.interleavedChannels.first;
    rtcpChannel_ = config.interleavedChannels.second;

    // Validate channel numbers
    if (!ValidateChannels(rtpChannel_, rtcpChannel_)) {
        LMRTSP_LOGE("Invalid interleaved channels: rtp=%d, rtcp=%d", static_cast<int>(rtpChannel_),
                    static_cast<int>(rtcpChannel_));
        return false;
    }

    // Build transport info string
    std::ostringstream oss;
    oss << "RTP/AVP/TCP;interleaved=" << static_cast<int>(rtpChannel_) << "-" << static_cast<int>(rtcpChannel_);
    transportInfo_ = oss.str();

    LMRTSP_LOGD("TCP interleaved adapter Setup: interleaved=%d-%d", static_cast<int>(rtpChannel_),
                static_cast<int>(rtcpChannel_));

    isSetup_ = true;
    return true;
}

bool TcpInterleavedTransportAdapter::SendPacket(const uint8_t *data, size_t size)
{
    if (!isSetup_ || !data || size == 0) {
        LMRTSP_LOGE("SendPacket invalid: isSetup=%s, size=%zu", isSetup_ ? "true" : "false", size);
        return false;
    }

    auto session = RtspServerSession_.lock();
    if (!session) {
        LMRTSP_LOGE("SendPacket failed: RTSP session expired");
        return false;
    }

    // Send interleaved RTP data through RTSP session
    bool ok = session->SendInterleavedData(rtpChannel_, data, size);
    if (!ok) {
        LMRTSP_LOGE("Failed to send interleaved RTP: channel=%d, size=%zu", static_cast<int>(rtpChannel_), size);
    } else {
        LMRTSP_LOGD("Interleaved RTP sent: channel=%d, size=%zu", static_cast<int>(rtpChannel_), size);
    }
    return ok;
}

bool TcpInterleavedTransportAdapter::SendRtcpPacket(const uint8_t *data, size_t size)
{
    if (!isSetup_ || !data || size == 0) {
        LMRTSP_LOGE("SendRtcpPacket invalid: isSetup=%s, size=%zu", isSetup_ ? "true" : "false", size);
        return false;
    }

    auto session = RtspServerSession_.lock();
    if (!session) {
        LMRTSP_LOGE("SendRtcpPacket failed: RTSP session expired");
        return false;
    }

    // Send interleaved RTCP data through RTSP session
    bool ok = session->SendInterleavedData(rtcpChannel_, data, size);
    if (!ok) {
        LMRTSP_LOGE("Failed to send interleaved RTCP: channel=%d, size=%zu", static_cast<int>(rtcpChannel_), size);
    } else {
        LMRTSP_LOGD("Interleaved RTCP sent: channel=%d, size=%zu", static_cast<int>(rtcpChannel_), size);
    }
    return ok;
}

void TcpInterleavedTransportAdapter::Close()
{
    isSetup_ = false;
    transportInfo_.clear();
    // Note: Don't close RTSP session as it's managed externally
}

std::string TcpInterleavedTransportAdapter::GetTransportInfo() const
{
    return transportInfo_;
}

bool TcpInterleavedTransportAdapter::IsActive() const
{
    if (!isSetup_) {
        return false;
    }

    auto session = RtspServerSession_.lock();
    return session != nullptr;
}

bool TcpInterleavedTransportAdapter::ValidateChannels(uint8_t rtpChannel, uint8_t rtcpChannel) const
{
    // RTP channel must be even, RTCP channel must be RTP channel + 1
    if (rtpChannel % 2 != 0) {
        return false;
    }

    if (rtcpChannel != rtpChannel + 1) {
        return false;
    }

    return true;
}

} // namespace lmshao::lmrtsp