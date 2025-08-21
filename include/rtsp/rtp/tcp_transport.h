#ifndef RTSP_TCP_TRANSPORT_H
#define RTSP_TCP_TRANSPORT_H

#include "rtsp/rtp/i_transport.h"
#include "network/tcp_client.h"
#include "network/iclient_listener.h"
#include <memory>

namespace lmshao::rtsp::rtp {

class TcpTransport : public ITransport, public lmshao::network::IClientListener, public std::enable_shared_from_this<TcpTransport> {
public:
    TcpTransport();
    virtual ~TcpTransport();

    bool Init(const std::string& ip, uint16_t port) override;
    bool Send(const uint8_t* data, size_t size) override;
    void Close() override;

protected:
    void OnReceive(lmshao::network::socket_t fd, std::shared_ptr<lmshao::coreutils::DataBuffer> buffer) override;
    void OnClose(lmshao::network::socket_t fd) override;
    void OnError(lmshao::network::socket_t fd, const std::string &errorInfo) override;

private:
    std::shared_ptr<lmshao::network::TcpClient> tcp_client_;
};

}

#endif // RTSP_TCP_TRANSPORT_H