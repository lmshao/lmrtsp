#ifndef RTSP_RTP_SESSION_H
#define RTSP_RTP_SESSION_H

#include "rtsp/rtp/i_rtp_packetizer.h"
#include "rtsp/rtp/i_transport.h"
#include <memory>

namespace lmshao::rtsp::rtp {

class RtpSession {
public:
    RtpSession(std::unique_ptr<lmshao::rtsp::rtp::IRtpPacketizer> packetizer, std::unique_ptr<lmshao::rtsp::rtp::ITransport> transport);
    ~RtpSession() = default;

    void SendFrame(const lmshao::rtsp::rtp::MediaFrame& frame);

private:
    std::unique_ptr<lmshao::rtsp::rtp::IRtpPacketizer> packetizer_;
    std::unique_ptr<lmshao::rtsp::rtp::ITransport> transport_;
};

}

#endif // RTSP_RTP_SESSION_H