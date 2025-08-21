#ifndef RTSP_UDP_TRANSPORT_H
#define RTSP_UDP_TRANSPORT_H

#include "rtsp/rtp/i_transport.h"
#include "network/udp_client.h"
#include <memory>

namespace lmshao::rtsp::rtp {

class UdpTransport : public ITransport {
public:
    UdpTransport();
    virtual ~UdpTransport();

    bool Init(const std::string& ip, uint16_t port) override;
    bool Send(const uint8_t* data, size_t len) override;
    void Close() override;

private:
    std::shared_ptr<lmshao::network::UdpClient> udp_client_;
};

}

#endif // RTSP_UDP_TRANSPORT_H
