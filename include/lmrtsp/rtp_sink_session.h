/**
 * @author SHAO Liming <lmshao@163.com>
 * @copyright Copyright (c) 2025 SHAO Liming
 * @license MIT
 *
 * SPDX-License-Identifier: MIT
 */

#ifndef LMSHAO_LMRTSP_RTP_SINK_SESSION_H
#define LMSHAO_LMRTSP_RTP_SINK_SESSION_H

#include <memory>
#include <string>

#include "lmcore/async_timer.h"
#include "lmrtsp/media_types.h"
#include "lmrtsp/transport_config.h"

namespace lmshao::lmrtsp {

class IRtpTransportAdapter;
class IRtpDepacketizer;
class RtcpReceiverContext;

class RtpSinkSessionListener {
public:
    virtual ~RtpSinkSessionListener() = default;

    virtual void OnFrame(const std::shared_ptr<MediaFrame> &frame) = 0;
    virtual void OnError(int code, const std::string &message) = 0;
};

struct RtpSinkSessionConfig {
    std::string session_id;
    uint32_t expected_ssrc = 0;

    MediaType video_type = MediaType::H264;
    uint8_t video_payload_type = 96;

    TransportConfig transport;
    uint32_t recv_buffer_size = 65536;

    // RTCP configuration
    bool enable_rtcp = false;         // Enable RTCP
    uint32_t rtcp_interval_ms = 5000; // RTCP report interval in milliseconds
    std::string rtcp_cname;           // RTCP CNAME (Canonical Name)
    std::string rtcp_name;            // RTCP NAME (User Name)
};

class RtpSinkSession {
public:
    RtpSinkSession();
    ~RtpSinkSession();

    bool Initialize(const RtpSinkSessionConfig &config);

    bool Start();
    void Stop();

    void SetListener(std::shared_ptr<RtpSinkSessionListener> listener);

    bool IsRunning() const { return running_; }

    // Get RTCP context (for statistics)
    RtcpReceiverContext *GetRtcpContext() const { return rtcpContext_.get(); }

private:
    class DepacketizerListener;
    class TransportListener;

    // Internal data handling methods
    void HandleRtpData(std::shared_ptr<lmcore::DataBuffer> buffer);
    void HandleRtcpData(std::shared_ptr<lmcore::DataBuffer> buffer);
    void HandleFrame(const std::shared_ptr<MediaFrame> &frame);
    void HandleDepacketizerError(int code, const std::string &message);

    // RTCP methods
    void StartRtcpTimer();
    void StopRtcpTimer();
    void SendRtcpReport();

    RtpSinkSessionConfig config_{};
    bool initialized_ = false;
    bool running_ = false;

    uint32_t lastTimestamp_ = 0;
    uint16_t lastSequenceNumber_ = 0;
    uint32_t rtcpSsrc_ = 0; // Generated RTCP SSRC for receiver

    std::unique_ptr<IRtpDepacketizer> videoDepacketizer_;
    std::weak_ptr<RtpSinkSessionListener> listener_;
    std::unique_ptr<IRtpTransportAdapter> transportAdapter_;

    // Internal listeners
    std::shared_ptr<DepacketizerListener> depacketizerListener_;
    std::shared_ptr<TransportListener> transportListener_;

    // RTCP support
    std::shared_ptr<RtcpReceiverContext> rtcpContext_;
    std::unique_ptr<lmcore::AsyncTimer> rtcpTimer_;
    lmcore::AsyncTimer::TimerId rtcpTimerId_ = 0;
};

} // namespace lmshao::lmrtsp

#endif // LMSHAO_LMRTSP_RTP_SINK_SESSION_H