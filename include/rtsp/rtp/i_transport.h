#ifndef RTSP_I_TRANSPORT_H
#define RTSP_I_TRANSPORT_H

#include <cstdint>
#include <string>
#include <vector>

namespace lmshao::rtsp::rtp {

class ITransport {
public:
    virtual ~ITransport() = default;

    virtual bool Init(const std::string& ip, uint16_t port) = 0;
    virtual bool Send(const uint8_t* data, size_t len) = 0;
    virtual void Close() = 0;
};

}

#endif // RTSP_I_TRANSPORT_H