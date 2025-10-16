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
#include "lmrtsp/h264_packetizer.h"
#include "lmrtsp/rtp_packet.h"
#include "rtsp_session.h"

namespace lmshao::lmrtsp {

// MediaStream base class implementation
MediaStream::MediaStream(const std::string &uri, const std::string &mediaType)
    : uri_(uri), mediaType_(mediaType), state_(StreamState::INIT)
{
    LMRTSP_LOGD("Created MediaStream for URI: %s, type: %s", uri.c_str(), mediaType.c_str());
}

MediaStream::~MediaStream()
{
    LMRTSP_LOGD("Destroyed MediaStream for URI: %s", uri_.c_str());
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
    LMRTSP_LOGD("Setting up RTP stream with transport: %s", transport.c_str());

    // Check transport type
    if (transport.find("RTP/AVP") == std::string::npos) {
        LMRTSP_LOGE("Unsupported transport protocol");
        return false;
    }

    // Check if it's unicast
    bool isUnicast = (transport.find("unicast") != std::string::npos);
    if (!isUnicast) {
        LMRTSP_LOGE("Only unicast mode is supported");
        return false;
    }

    // Check transport protocol (UDP vs TCP)
    bool isTcpTransport = (transport.find("RTP/AVP/TCP") != std::string::npos);
    bool isUdpTransport = (transport.find("RTP/AVP/UDP") != std::string::npos ||
                           (transport.find("RTP/AVP") != std::string::npos && !isTcpTransport));

    if (isTcpTransport) {
        // TCP interleaved transport
        isTcpTransport_ = true;
        LMRTSP_LOGD("Setting up TCP interleaved transport");
    } else if (isUdpTransport) {
        // UDP transport
        isTcpTransport_ = false;
        LMRTSP_LOGD("Setting up UDP transport");
    } else {
        LMRTSP_LOGE("Unsupported transport protocol");
        return false;
    }

    if (isTcpTransport_) {
        // Parse TCP interleaved channels
        size_t interleavedPos = transport.find("interleaved=");
        if (interleavedPos != std::string::npos) {
            size_t channelStart = interleavedPos + 12; // Length of "interleaved="
            size_t channelEnd = transport.find(';', channelStart);
            if (channelEnd == std::string::npos) {
                channelEnd = transport.length();
            }

            std::string channelRange = transport.substr(channelStart, channelEnd - channelStart);
            // Trim whitespace
            size_t firstNonSpace = channelRange.find_first_not_of(" \t\r\n");
            size_t lastNonSpace = channelRange.find_last_not_of(" \t\r\n");
            if (firstNonSpace != std::string::npos && lastNonSpace != std::string::npos) {
                channelRange = channelRange.substr(firstNonSpace, lastNonSpace - firstNonSpace + 1);
            }

            LMRTSP_LOGD("Parsed interleaved channel range: '%s'", channelRange.c_str());

            size_t dashPos = channelRange.find('-');
            if (dashPos != std::string::npos) {
                try {
                    std::string rtpStr = channelRange.substr(0, dashPos);
                    std::string rtcpStr = channelRange.substr(dashPos + 1);
                    rtpInterleaved_ = static_cast<uint8_t>(std::stoi(rtpStr));
                    rtcpInterleaved_ = static_cast<uint8_t>(std::stoi(rtcpStr));
                    LMRTSP_LOGD("TCP interleaved channels: RTP=%d, RTCP=%d", rtpInterleaved_, rtcpInterleaved_);
                } catch (const std::exception &e) {
                    LMRTSP_LOGE("Failed to parse interleaved channels: %s, using defaults", e.what());
                    rtpInterleaved_ = 0;
                    rtcpInterleaved_ = 1;
                }
            } else {
                LMRTSP_LOGE("Invalid interleaved channel format, using defaults");
                rtpInterleaved_ = 0;
                rtcpInterleaved_ = 1;
            }
        } else {
            // If no interleaved channels specified, use defaults
            rtpInterleaved_ = 0;
            rtcpInterleaved_ = 1;
            LMRTSP_LOGD("Using default TCP interleaved channels: RTP=%d, RTCP=%d", rtpInterleaved_, rtcpInterleaved_);
        }

        transportInfo_ = transport;
    } else {
        // Parse UDP client ports
        size_t clientPortPos = transport.find("client_port=");
        if (clientPortPos != std::string::npos) {
            size_t portStart = clientPortPos + 12; // Length of "client_port="
            size_t portEnd = transport.find(';', portStart);
            if (portEnd == std::string::npos) {
                portEnd = transport.length();
            }

            std::string portRange = transport.substr(portStart, portEnd - portStart);
            // Trim whitespace
            size_t firstNonSpace = portRange.find_first_not_of(" \t\r\n");
            size_t lastNonSpace = portRange.find_last_not_of(" \t\r\n");
            if (firstNonSpace != std::string::npos && lastNonSpace != std::string::npos) {
                portRange = portRange.substr(firstNonSpace, lastNonSpace - firstNonSpace + 1);
            }

            LMRTSP_LOGD("Parsed client port range: '%s'", portRange.c_str());

            size_t dashPos = portRange.find('-');
            if (dashPos != std::string::npos) {
                try {
                    std::string rtpPortStr = portRange.substr(0, dashPos);
                    std::string rtcpPortStr = portRange.substr(dashPos + 1);
                    clientRtpPort_ = static_cast<uint16_t>(std::stoi(rtpPortStr));
                    clientRtcpPort_ = static_cast<uint16_t>(std::stoi(rtcpPortStr));
                    LMRTSP_LOGD("Client UDP ports: RTP=%d, RTCP=%d", clientRtpPort_, clientRtcpPort_);
                } catch (const std::exception &e) {
                    LMRTSP_LOGE("Failed to parse client ports: %s", e.what());
                    return false;
                }
            } else {
                LMRTSP_LOGE("Invalid client_port format");
                return false;
            }
        } else {
            LMRTSP_LOGE("UDP transport requires client_port parameter");
            return false;
        }

        // Create UDP clients for RTP and RTCP
        rtp_client_ = std::make_shared<UdpClient>(client_ip, clientRtpPort_);
        if (!rtp_client_->Init()) {
            LMRTSP_LOGE("Failed to initialize RTP UDP client");
            return false;
        }
        LMRTSP_LOGD("RTP UDP client initialized for %s:%d", client_ip.c_str(), clientRtpPort_);

        rtcp_client_ = std::make_shared<UdpClient>(client_ip, clientRtcpPort_);
        if (!rtcp_client_->Init()) {
            LMRTSP_LOGE("Failed to initialize RTCP UDP client");
            return false;
        }
        LMRTSP_LOGD("RTCP UDP client initialized for %s:%d", client_ip.c_str(), clientRtcpPort_);

        // Allocate server ports (simplified: use any available ports)
        // In production, you'd want to manage a port pool
        serverRtpPort_ = 0;  // Let OS assign
        serverRtcpPort_ = 0; // Let OS assign

        // Build transport response
        char transportBuf[256];
        snprintf(transportBuf, sizeof(transportBuf), "RTP/AVP/UDP;unicast;client_port=%d-%d;server_port=%d-%d",
                 clientRtpPort_, clientRtcpPort_, serverRtpPort_, serverRtcpPort_);
        transportInfo_ = transportBuf;
        LMRTSP_LOGD("Transport info: %s", transportInfo_.c_str());
    }

    // Initialize H.264 packetizer
    packetizer_ = std::make_unique<lmrtsp::H264Packetizer>(12345, // SSRC
                                                           0,     // Initial sequence number
                                                           0,     // Initial timestamp
                                                           1400   // MTU size
    );

    // Update state
    state_ = StreamState::READY;

    LMRTSP_LOGD("RTP stream setup successful");
    return true;
}

bool RTPStream::Play(const std::string &range)
{
    LMRTSP_LOGD("Playing RTP stream, range: %s", range.c_str());

    if (state_ != StreamState::READY && state_ != StreamState::PAUSED) {
        LMRTSP_LOGE("Cannot play stream in current state");
        return false;
    }

    if (auto session = session_.lock()) {
        LMRTSP_LOGD("Session is valid, ready to send frames for track %d", track_index_);
        isActive_ = true;
        send_thread_ = std::thread(&RTPStream::SendMedia, this);
    } else {
        LMRTSP_LOGE("Session is expired, cannot play stream");
        return false;
    }

    // Update state
    state_ = StreamState::PLAYING;

    LMRTSP_LOGD("RTP stream play started");
    return true;
}

bool RTPStream::Pause()
{
    LMRTSP_LOGD("Pausing RTP stream");

    if (state_ != StreamState::PLAYING) {
        LMRTSP_LOGE("Cannot pause stream in current state");
        return false;
    }

    // In actual implementation, RTP data transmission should be paused here

    // Update state
    state_ = StreamState::PAUSED;

    LMRTSP_LOGD("RTP stream paused");
    return true;
}

bool RTPStream::Teardown()
{
    LMRTSP_LOGD("Tearing down RTP stream");

    if (state_ == StreamState::INIT) {
        LMRTSP_LOGD("Stream already in INIT state");
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

    LMRTSP_LOGD("RTP stream teardown successful");
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
    LMRTSP_LOGD("RTPStream received a packet");
}

void RTPStream::OnClose(std::shared_ptr<lmnet::Session> session)
{
    LMRTSP_LOGD("RTPStream session closed");
}

void RTPStream::OnError(std::shared_ptr<lmnet::Session> session, const std::string &error)
{
    LMRTSP_LOGE("RTPStream error: %s", error.c_str());
}

void RTPStream::SendMedia()
{
    LMRTSP_LOGD("SendMedia thread started");

    while (isActive_) {
        std::unique_lock<std::mutex> lock(queue_mutex_);
        queue_cv_.wait(lock, [this] { return !frame_queue_.empty() || !isActive_; });

        if (!isActive_) {
            break;
        }

        MediaFrame frame = std::move(frame_queue_.front());
        frame_queue_.pop();
        lock.unlock();

        // Update timestamp and sequence number
        timestamp_ = frame.timestamp;
        sequenceNumber_++;

        // Pack frame into RTP packets and send them
        if (packetizer_) {
            auto packets = packetizer_->packetize(frame);
            for (const auto &packet : packets) {
                auto buffer = packet.serialize();

                if (isTcpTransport_) {
                    // TCP interleaved transport
                    if (auto session = session_.lock()) {
                        if (auto networkSession = session->GetNetworkSession()) {
                            // Send RTP packet with TCP interleaved header
                            // Format: $<channel><length><data>
                            std::vector<uint8_t> interleaved_data;
                            interleaved_data.push_back('$');
                            interleaved_data.push_back(rtpInterleaved_);

                            uint16_t length = static_cast<uint16_t>(buffer.size());
                            interleaved_data.push_back((length >> 8) & 0xFF);
                            interleaved_data.push_back(length & 0xFF);

                            interleaved_data.insert(interleaved_data.end(), buffer.begin(), buffer.end());

                            networkSession->Send(interleaved_data.data(), interleaved_data.size());
                        } else {
                            LMRTSP_LOGE("No network session available for TCP interleaved transport");
                        }
                    } else {
                        LMRTSP_LOGE("Session expired, cannot send TCP interleaved data");
                    }
                } else {
                    // UDP transport
                    if (!rtp_client_->Send(buffer.data(), buffer.size())) {
                        LMRTSP_LOGE("Failed to send RTP packet via UDP");
                    }
                }
            }
        } else {
            LMRTSP_LOGE("No packetizer available");
        }
    }
    LMRTSP_LOGD("SendMedia thread finished");
}

// MediaStreamFactory implementation
std::shared_ptr<MediaStream> MediaStreamFactory::CreateStream(const std::string &uri, const std::string &mediaType)
{
    LMRTSP_LOGD("Creating media stream for URI: %s, type: %s", uri.c_str(), mediaType.c_str());

    // Currently only supports RTP streams
    return std::make_shared<RTPStream>(uri, mediaType);
}

} // namespace lmshao::lmrtsp