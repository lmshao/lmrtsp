/**
 * @author SHAO Liming <lmshao@163.com>
 * @copyright Copyright (c) 2025 SHAO Liming
 * @license MIT
 *
 * SPDX-License-Identifier: MIT
 */

#include "media_stream.h"

#include "rtsp_log.h"

namespace lmshao::rtsp {

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

bool RTPStream::Setup(const std::string &transport)
{
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
        size_t portEnd = transport.find(";", portStart);
        if (portEnd == std::string::npos) {
            portEnd = transport.length();
        }

        std::string portRange = transport.substr(portStart, portEnd - portStart);
        size_t dashPos = portRange.find("-");

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
    // In actual implementation, available ports should be dynamically allocated
    serverRtpPort_ = 6000;  // Example port
    serverRtcpPort_ = 6001; // Example port

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

    // In actual implementation, RTP data transmission should be started here
    // A thread can be created to handle media data transmission

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

// MediaStreamFactory implementation
std::shared_ptr<MediaStream> MediaStreamFactory::CreateStream(const std::string &uri, const std::string &mediaType)
{
    RTSP_LOGD("Creating media stream for URI: %s, type: %s", uri.c_str(), mediaType.c_str());

    // Currently only supports RTP streams
    return std::make_shared<RTPStream>(uri, mediaType);
}

} // namespace lmshao::rtsp