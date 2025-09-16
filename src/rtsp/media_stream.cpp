/**
 * @author SHAO Liming <lmshao@163.com>
 * @copyright Copyright (c) 2025 SHAO Liming
 * @license MIT
 *
 * SPDX-License-Identifier: MIT
 */

#include "media_stream.h"

#include <chrono>
#include <vector>

#include "internal_logger.h"
#include "lmrtp/h264_packetizer.h"
#include "rtsp_session.h"

namespace lmshao::lmrtsp {

// MediaStream base class implementation
MediaStream::MediaStream(const std::string &uri, const std::string &mediaType)
    : uri_(uri), mediaType_(mediaType), state_(StreamState::INIT)
{
    RTSP_LOGD("Created MediaStream for URI: %s, type: %s", uri.c_str(), mediaType.c_str());
}

MediaStream::~MediaStream()
{
    RTSP_LOGD("Destroyed MediaStream for URI: %s", uri_.c_str());
}

std::string MediaStream::GetUri() const
{
    return uri_;
}

std::string MediaStream::GetMediaType() const
{
    return mediaType_;
}

StreamState MediaStream::GetState() const
{
    return state_;
}

int MediaStream::GetTrackId() const
{
    return track_index_;
}

void MediaStream::SetSession(std::weak_ptr<RTSPSession> session)
{
    session_ = session;
}

void MediaStream::SetTrackIndex(int index)
{
    track_index_ = index;
}

// RTPStream implementation
RTPStream::RTPStream(const std::string &uri, const std::string &mediaType)
    : MediaStream(uri, mediaType), clientRtpPort_(0), clientRtcpPort_(0), serverRtpPort_(0), serverRtcpPort_(0),
      sequenceNumber_(0), timestamp_(0), isActive_(false)
{
}

RTPStream::~RTPStream()
{
    // Ensure resources are released
    Teardown();
}

bool RTPStream::Setup(const std::string &transport, const std::string &client_ip)
{
    clientIp_ = client_ip;
    RTSP_LOGD("Setting up RTP stream with transport: %s", transport.c_str());

    // Parse Transport header
    // Example: Transport: RTP/AVP;unicast;client_port=4588-4589

    // Check transport type
    if (transport.find("RTP/AVP") == std::string::npos) {
        RTSP_LOGE("Unsupported transport protocol");
        return false;
    }

    // Check if it's unicast
    bool isUnicast = (transport.find("unicast") != std::string::npos);
    if (!isUnicast) {
        RTSP_LOGE("Only unicast mode is supported");
        return false;
    }

    // Parse client ports
    size_t clientPortPos = transport.find("client_port=");
    if (clientPortPos != std::string::npos) {
        size_t portStart = clientPortPos + 12; // Length of "client_port="
        size_t portEnd = transport.find(';', portStart);
        if (portEnd == std::string::npos) {
            portEnd = transport.length();
        }

        std::string portRange = transport.substr(portStart, portEnd - portStart);
        size_t dashPos = portRange.find('-');

        if (dashPos != std::string::npos) {
            clientRtpPort_ = std::stoi(portRange.substr(0, dashPos));
            clientRtcpPort_ = std::stoi(portRange.substr(dashPos + 1));
            RTSP_LOGD("Client ports: RTP=%d, RTCP=%d", clientRtpPort_, clientRtcpPort_);
        } else {
            RTSP_LOGE("Invalid client_port format");
            return false;
        }
    } else {
        RTSP_LOGE("Missing client_port parameter");
        return false;
    }

    // Allocate server ports
    auto port = lmnet::UdpServer::GetIdlePortPair();
    if (port == 0) {
        RTSP_LOGE("Failed to get idle port pair");
        return false;
    }
    serverRtpPort_ = port;
    serverRtcpPort_ = port + 1;

    rtp_server_ = std::make_shared<lmnet::UdpServer>(serverRtpPort_);
    rtp_server_->SetListener(shared_from_this());
    if (!rtp_server_->Start()) {
        RTSP_LOGE("Failed to start rtp server");
        return false;
    }

    rtcp_server_ = std::make_shared<lmnet::UdpServer>(serverRtcpPort_);
    rtcp_server_->SetListener(shared_from_this());
    if (!rtcp_server_->Start()) {
        RTSP_LOGE("Failed to start rtcp server");
        return false;
    }

    rtp_client_ = std::make_shared<lmnet::UdpClient>(clientIp_, clientRtpPort_);
    if (!rtp_client_->Init()) {
        RTSP_LOGE("Failed to init rtp client");
        return false;
    }

    rtcp_client_ = std::make_shared<lmnet::UdpClient>(clientIp_, clientRtcpPort_);
    if (!rtcp_client_->Init()) {
        RTSP_LOGE("Failed to init rtcp client");
        return false;
    }

    // Save transport information
    transportInfo_ =
        transport + ";server_port=" + std::to_string(serverRtpPort_) + "-" + std::to_string(serverRtcpPort_);

    // Update state
    state_ = StreamState::READY;

    RTSP_LOGD("RTP stream setup successful");
    return true;
}

bool RTPStream::Play(const std::string &range)
{
    RTSP_LOGD("Playing RTP stream, range: %s", range.c_str());

    if (state_ != StreamState::READY && state_ != StreamState::PAUSED) {
        RTSP_LOGE("Cannot play stream in current state");
        return false;
    }

    if (auto session = session_.lock()) {
        RTSP_LOGD("Session is valid, ready to send frames for track %d", track_index_);
        isActive_ = true;
        send_thread_ = std::thread(&RTPStream::SendMedia, this);
    } else {
        RTSP_LOGE("Session is expired, cannot play stream");
        return false;
    }

    // Update state
    state_ = StreamState::PLAYING;

    RTSP_LOGD("RTP stream play started");
    return true;
}

bool RTPStream::Pause()
{
    RTSP_LOGD("Pausing RTP stream");

    if (state_ != StreamState::PLAYING) {
        RTSP_LOGE("Cannot pause stream in current state");
        return false;
    }

    // In actual implementation, RTP data transmission should be paused here

    // Update state
    state_ = StreamState::PAUSED;

    RTSP_LOGD("RTP stream paused");
    return true;
}

bool RTPStream::Teardown()
{
    RTSP_LOGD("Tearing down RTP stream");

    if (state_ == StreamState::INIT) {
        RTSP_LOGD("Stream already in INIT state");
        return true;
    }

    isActive_ = false;
    queue_cv_.notify_all();
    if (send_thread_.joinable()) {
        send_thread_.join();
    }

    if (rtp_server_) {
        rtp_server_->Stop();
    }

    if (rtcp_server_) {
        rtcp_server_->Stop();
    }

    if (rtp_client_) {
        // rtp_client_->Close();
    }
    if (rtcp_client_) {
        // rtcp_client_->Close();
    }

    // In actual implementation, RTP data transmission should be stopped and resources released

    // Update state
    state_ = StreamState::INIT;

    RTSP_LOGD("RTP stream teardown successful");
    return true;
}

std::string RTPStream::GetRtpInfo() const
{
    // In actual implementation, this should return the value of RTP-Info header
    // Example: RTP-Info: url=rtsp://example.com/media.mp4/track1;seq=12345;rtptime=3450012
    return "url=" + uri_ + ";seq=" + std::to_string(sequenceNumber_) + ";rtptime=" + std::to_string(timestamp_);
}

std::string RTPStream::GetTransportInfo() const
{
    return transportInfo_;
}

void RTPStream::PushFrame(MediaFrame &&frame)
{
    std::unique_lock<std::mutex> lock(queue_mutex_);
    frame_queue_.push(std::move(frame));
    lock.unlock();
    queue_cv_.notify_one();
}

void RTPStream::OnReceive(std::shared_ptr<lmnet::Session> session, std::shared_ptr<lmcore::DataBuffer> data)
{
    RTSP_LOGD("RTPStream received a packet");
}

void RTPStream::OnClose(std::shared_ptr<lmnet::Session> session)
{
    RTSP_LOGD("RTPStream session closed");
}

void RTPStream::OnError(std::shared_ptr<lmnet::Session> session, const std::string &error)
{
    RTSP_LOGE("RTPStream error: %s", error.c_str());
}

void RTPStream::SendMedia()
{
    RTSP_LOGD("SendMedia thread started");

    while (isActive_) {
        std::unique_lock<std::mutex> lock(queue_mutex_);
        queue_cv_.wait(lock, [this] { return !frame_queue_.empty() || !isActive_; });

        if (!isActive_) {
            break;
        }

        MediaFrame frame = std::move(frame_queue_.front());
        frame_queue_.pop();
        lock.unlock();

        // Pack frame into RTP packets and send them
        if (packetizer_) {
            auto packets = packetizer_->packetize(frame);
            for (const auto &packet : packets) {
                auto buffer = packet.serialize();
                if (!rtp_client_->Send(buffer.data(), buffer.size())) {
                    RTSP_LOGE("Failed to send RTP packet");
                }
            }
        } else {
            RTSP_LOGE("No packetizer available");
        }
    }
    RTSP_LOGD("SendMedia thread finished");
}

// MediaStreamFactory implementation
std::shared_ptr<MediaStream> MediaStreamFactory::CreateStream(const std::string &uri, const std::string &mediaType)
{
    RTSP_LOGD("Creating media stream for URI: %s, type: %s", uri.c_str(), mediaType.c_str());

    // Currently only supports RTP streams
    return std::make_shared<RTPStream>(uri, mediaType);
}

} // namespace lmshao::lmrtsp