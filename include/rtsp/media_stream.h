/**
 * @author SHAO Liming <lmshao@163.com>
 * @copyright Copyright (c) 2025 SHAO Liming
 * @license MIT
 *
 * SPDX-License-Identifier: MIT
 */

#ifndef MEDIA_STREAM_H
#define MEDIA_STREAM_H

#include <memory>
#include <string>

namespace lmshao::rtsp {

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
    virtual bool Setup(const std::string &transport) = 0;
    virtual bool Play(const std::string &range = "") = 0;
    virtual bool Pause() = 0;
    virtual bool Teardown() = 0;

    // Media information
    std::string GetUri() const;
    std::string GetMediaType() const;
    StreamState GetState() const;
    virtual std::string GetRtpInfo() const = 0;

    // Transport information
    virtual std::string GetTransportInfo() const = 0;

protected:
    std::string uri_;
    std::string mediaType_;
    StreamState state_;
};

// RTP stream implementation
class RTPStream : public MediaStream {
public:
    RTPStream(const std::string &uri, const std::string &mediaType);
    ~RTPStream() override;

    bool Setup(const std::string &transport) override;
    bool Play(const std::string &range = "") override;
    bool Pause() override;
    bool Teardown() override;

    std::string GetRtpInfo() const override;
    std::string GetTransportInfo() const override;

private:
    std::string transportInfo_;

    // RTP session parameters
    uint16_t clientRtpPort_;
    uint16_t clientRtcpPort_;
    uint16_t serverRtpPort_;
    uint16_t serverRtcpPort_;
    std::string clientIp_;

    // RTP state
    uint16_t sequenceNumber_;
    uint32_t timestamp_;
    bool isActive_;
};

// Factory method to create media stream
class MediaStreamFactory {
public:
    static std::shared_ptr<MediaStream> CreateStream(const std::string &uri, const std::string &mediaType);
};

} // namespace lmshao::rtsp

#endif // MEDIA_STREAM_H