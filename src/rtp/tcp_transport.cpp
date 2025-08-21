#include "rtsp/rtp/tcp_transport.h"

namespace lmshao::rtsp::rtp {

TcpTransport::TcpTransport() = default;

TcpTransport::~TcpTransport() {
    Close();
}

bool TcpTransport::Init(const std::string& ip, uint16_t port) {
    tcp_client_ = lmshao::network::TcpClient::Create(ip, port);
    if (!tcp_client_) {
        return false;
    }
    tcp_client_->SetListener(shared_from_this());
    return tcp_client_->Init();
}

bool TcpTransport::Send(const uint8_t* data, size_t size) {
    if (!tcp_client_) {
        return false;
    }
    return tcp_client_->Send(reinterpret_cast<const char*>(data), size) > 0;
}

void TcpTransport::Close() {
    if (tcp_client_) {
        tcp_client_->Close();
    }
}

void TcpTransport::OnReceive(lmshao::network::socket_t fd, std::shared_ptr<lmshao::coreutils::DataBuffer> buffer) {
    // Not used for sending RTP data
}

void TcpTransport::OnClose(lmshao::network::socket_t fd) {
    // Not used for sending RTP data
}

void TcpTransport::OnError(lmshao::network::socket_t fd, const std::string &errorInfo)
{
    // Not used for sending RTP data
}

}