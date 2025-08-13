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

// 媒体流状态枚举
enum class StreamState {
    INIT,    // 初始状态
    READY,   // 准备就绪
    PLAYING, // 正在播放
    PAUSED   // 已暂停
};

// 媒体流基类
class MediaStream {
public:
    MediaStream(const std::string &uri, const std::string &mediaType);
    virtual ~MediaStream();

    // 媒体流控制
    virtual bool Setup(const std::string &transport) = 0;
    virtual bool Play(const std::string &range = "") = 0;
    virtual bool Pause() = 0;
    virtual bool Teardown() = 0;

    // 媒体信息
    std::string GetUri() const;
    std::string GetMediaType() const;
    StreamState GetState() const;
    virtual std::string GetRtpInfo() const = 0;

    // 传输信息
    virtual std::string GetTransportInfo() const = 0;

protected:
    std::string uri_;
    std::string mediaType_;
    StreamState state_;
};

// RTP流实现
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

    // RTP会话参数
    uint16_t clientRtpPort_;
    uint16_t clientRtcpPort_;
    uint16_t serverRtpPort_;
    uint16_t serverRtcpPort_;
    std::string clientIp_;

    // RTP状态
    uint16_t sequenceNumber_;
    uint32_t timestamp_;
    bool isActive_;
};

// 工厂方法创建媒体流
class MediaStreamFactory {
public:
    static std::shared_ptr<MediaStream> CreateStream(const std::string &uri, const std::string &mediaType);
};

} // namespace lmshao::rtsp

#endif // MEDIA_STREAM_H