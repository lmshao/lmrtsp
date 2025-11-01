/**
 * @author SHAO Liming <lmshao@163.com>
 * @copyright Copyright (c) 2025 SHAO Liming
 * @license MIT
 *
 * SPDX-License-Identifier: MIT
 */

#ifndef LMSHAO_LMRTSP_UDP_RTP_TRANSPORT_ADAPTER_H
#define LMSHAO_LMRTSP_UDP_RTP_TRANSPORT_ADAPTER_H

#include <lmnet/iclient_listener.h>
#include <lmnet/iserver_listener.h>
#include <lmnet/udp_client.h>
#include <lmnet/udp_server.h>

#include <memory>
#include <string>

#include "i_rtp_transport_adapter.h"

namespace lmshao::lmrtsp {
using namespace lmshao::lmnet;

class UdpRtpTransportAdapterListener {
public:
    virtual ~UdpRtpTransportAdapterListener() = default;
    virtual void OnRtpDataReceived(std::shared_ptr<lmnet::DataBuffer> buffer) = 0;
    virtual void OnRtcpDataReceived(std::shared_ptr<lmnet::DataBuffer> buffer) = 0;
};

class UdpRtpTransportAdapter : public IRtpTransportAdapter {
public:
    UdpRtpTransportAdapter();
    ~UdpRtpTransportAdapter() override;

    bool Setup(const TransportConfig &config) override;
    bool SendPacket(const uint8_t *data, size_t size) override;
    bool SendRtcpPacket(const uint8_t *data, size_t size) override;
    void Close() override;
    std::string GetTransportInfo() const override;
    bool IsActive() const override;

    void SetOnDataListener(std::shared_ptr<UdpRtpTransportAdapterListener> listener) { listener_ = listener; }

    // Port getters for dynamically allocated ports
    uint16_t GetServerRtpPort() const { return serverRtpPort_; }
    uint16_t GetServerRtcpPort() const { return serverRtcpPort_; }
    uint16_t GetClientRtpPort() const { return clientRtpPort_; }
    uint16_t GetClientRtcpPort() const { return clientRtcpPort_; }

private:
    void OnRtpDataReceived(std::shared_ptr<lmnet::DataBuffer> buffer) const;
    void OnRtcpDataReceived(std::shared_ptr<lmnet::DataBuffer> buffer) const;

    bool IsRtcpEnabled() const;
    bool InitializeUdpClients();
    bool InitializeUdpServers();
    uint16_t FindAvailablePortPair(uint16_t start_port = 0);

private:
    enum class ListenerMode {
        RTP,
        RTCP
    };

    class UdpServerReceiveListener : public lmnet::IServerListener {
    public:
        explicit UdpServerReceiveListener(UdpRtpTransportAdapter *adapter, ListenerMode mode);
        void OnAccept(std::shared_ptr<lmnet::Session> session) override;
        void OnReceive(std::shared_ptr<lmnet::Session> session, std::shared_ptr<lmnet::DataBuffer> buffer) override;
        void OnClose(std::shared_ptr<lmnet::Session> session) override;
        void OnError(std::shared_ptr<lmnet::Session> session, const std::string &errorInfo) override;

    private:
        UdpRtpTransportAdapter *adapter_;
        ListenerMode mode_;
    };

    class UdpClientReceiveListener : public lmnet::IClientListener {
    public:
        explicit UdpClientReceiveListener(UdpRtpTransportAdapter *adapter, ListenerMode mode);
        void OnReceive(lmnet::socket_t fd, std::shared_ptr<lmnet::DataBuffer> buffer) override;
        void OnClose(lmnet::socket_t fd) override;
        void OnError(lmnet::socket_t fd, const std::string &errorInfo) override;

    private:
        UdpRtpTransportAdapter *adapter_;
        ListenerMode mode_;
    };

private:
    // Transport runtime
    TransportConfig config_{};
    bool active_{false};
    bool unicast_{true};

    // Endpoint info
    std::string client_ip_{};
    uint16_t clientRtpPort_{0};
    uint16_t clientRtcpPort_{0};
    uint16_t serverRtpPort_{0};
    uint16_t serverRtcpPort_{0};

    // UDP networking (lmnet)
    std::shared_ptr<lmnet::UdpClient> rtp_client_{};
    std::shared_ptr<lmnet::UdpClient> rtcp_client_{};
    std::shared_ptr<lmnet::UdpServer> rtp_server_{};
    std::shared_ptr<lmnet::UdpServer> rtcp_server_{};

    // Strong refs for listeners to avoid weak_ptr expiration inside lmnet
    std::shared_ptr<lmnet::IServerListener> rtp_server_listener_{};
    std::shared_ptr<lmnet::IServerListener> rtcp_server_listener_{};
    std::shared_ptr<lmnet::IClientListener> rtp_client_listener_{};
    std::shared_ptr<lmnet::IClientListener> rtcp_client_listener_{};

    std::shared_ptr<UdpRtpTransportAdapterListener> listener_{};
};

} // namespace lmshao::lmrtsp

#endif // LMSHAO_LMRTSP_UDP_RTP_TRANSPORT_ADAPTER_H