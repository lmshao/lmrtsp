/**
 * @author SHAO Liming <lmshao@163.com>
 * @copyright Copyright (c) 2025 SHAO Liming
 * @license MIT
 *
 * SPDX-License-Identifier: MIT
 */

#ifndef LMSHAO_RTSP_MEDIA_STREAM_H
#define LMSHAO_RTSP_MEDIA_STREAM_H

#include <network/iserver_listener.h>
#include <network/udp_client.h>
#include <network/udp_server.h>

#include <atomic>
#include <condition_variable>
#include <memory>
#include <mutex>
#include <queue>
#include <string>
#include <thread>

#include "rtp/i_rtp_packetizer.h"

using namespace lmshao::network;
using namespace lmshao::coreutils;
using namespace lmshao::rtp;

namespace lmshao::rtsp {

class RTSPSession;

// Media stream state enumeration
enum class StreamState {
    INIT,    // Initial state
    READY,   // Ready
    PLAYING, // Playing
    PAUSED   // Paused
};

// Media stream base class
class MediaStream {
public:
    MediaStream(const std::string &uri, const std::string &mediaType);
    virtual ~MediaStream();

    // Media stream control
    virtual bool Setup(const std::string &transport, const std::string &client_ip) = 0;
    virtual bool Play(const std::string &range = "") = 0;
    virtual bool Pause() = 0;
    virtual bool Teardown() = 0;

    // Media information
    std::string GetUri() const;
    std::string GetMediaType() const;
    StreamState GetState() const;
    int GetTrackId() const;
    virtual std::string GetRtpInfo() const = 0;

    // Transport information
    virtual std::string GetTransportInfo() const = 0;

    void SetSession(std::weak_ptr<RTSPSession> session);
    void SetTrackIndex(int index);

protected:
    std::string uri_;
    std::string mediaType_;
    StreamState state_;
    std::weak_ptr<RTSPSession> session_;
    int track_index_ = -1;
};

// RTP stream implementation
class RTPStream : public MediaStream, public IServerListener, public std::enable_shared_from_this<RTPStream> {
public:
    RTPStream(const std::string &uri, const std::string &mediaType);
    ~RTPStream() override;

    bool Setup(const std::string &transport, const std::string &client_ip) override;
    bool Play(const std::string &range = "") override;
    bool Pause() override;
    bool Teardown() override;

    std::string GetRtpInfo() const override;
    std::string GetTransportInfo() const override;

    void PushFrame(MediaFrame &&frame);

    uint16_t GetClientRtpPort() const { return clientRtpPort_; }
    uint16_t GetClientRtcpPort() const { return clientRtcpPort_; }

    // IServerListener implementation
    void OnAccept(std::shared_ptr<Session> session) override {}
    void OnReceive(std::shared_ptr<Session> session, std::shared_ptr<DataBuffer> data) override;
    void OnClose(std::shared_ptr<Session> session) override;
    void OnError(std::shared_ptr<Session> session, const std::string &error) override;

private:
    void SendMedia();

private:
    std::string transportInfo_;
    std::unique_ptr<IRtpPacketizer> packetizer_;
    std::shared_ptr<UdpServer> rtp_server_;
    std::shared_ptr<UdpServer> rtcp_server_;
    std::shared_ptr<UdpClient> rtp_client_;
    std::shared_ptr<UdpClient> rtcp_client_;

    // RTP session parameters
    uint16_t clientRtpPort_;
    uint16_t clientRtcpPort_;
    uint16_t serverRtpPort_;
    uint16_t serverRtcpPort_;
    std::string clientIp_;

    // RTP state
    uint16_t sequenceNumber_;
    uint32_t timestamp_;
    std::atomic<bool> isActive_;
    std::thread send_thread_;

    std::queue<MediaFrame> frame_queue_;
    std::mutex queue_mutex_;
    std::condition_variable queue_cv_;
};

// Factory method to create media stream
class MediaStreamFactory {
public:
    static std::shared_ptr<MediaStream> CreateStream(const std::string &uri, const std::string &mediaType);
};

} // namespace lmshao::rtsp

#endif // LMSHAO_RTSP_MEDIA_STREAM_H