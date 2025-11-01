/**
 * @author SHAO Liming <lmshao@163.com>
 * @copyright Copyright (c) 2025 SHAO Liming
 * @license MIT
 *
 * SPDX-License-Identifier: MIT
 */

#include "udp_rtp_transport_adapter.h"

#include <lmcore/data_buffer.h>
#include <lmnet/iclient_listener.h>
#include <lmnet/iserver_listener.h>
#include <lmnet/udp_server.h>
#include <lmrtsp/rtp_packet.h>

#include <sstream>

#include "internal_logger.h"

namespace lmshao::lmrtsp {

// UdpServerReceiveListener implementation
UdpRtpTransportAdapter::UdpServerReceiveListener::UdpServerReceiveListener(UdpRtpTransportAdapter *adapter,
                                                                           ListenerMode mode)
    : adapter_(adapter), mode_(mode)
{
}

void UdpRtpTransportAdapter::UdpServerReceiveListener::OnAccept(std::shared_ptr<lmnet::Session> session) {}

void UdpRtpTransportAdapter::UdpServerReceiveListener::OnReceive(std::shared_ptr<lmnet::Session> session,
                                                                 std::shared_ptr<lmnet::DataBuffer> buffer)
{
    if (!adapter_)
        return;

    if (mode_ == ListenerMode::RTP) {
        adapter_->OnRtpDataReceived(buffer);
    } else {
        adapter_->OnRtcpDataReceived(buffer);
    }
}

void UdpRtpTransportAdapter::UdpServerReceiveListener::OnClose(std::shared_ptr<lmnet::Session> session) {}

void UdpRtpTransportAdapter::UdpServerReceiveListener::OnError(std::shared_ptr<lmnet::Session> session,
                                                               const std::string &errorInfo)
{
    LMRTSP_LOGE("UdpServerReceiveListener %s: OnError: %s", mode_ == ListenerMode::RTP ? "RTP" : "RTCP",
                errorInfo.c_str());
}

// UdpClientReceiveListener implementation
UdpRtpTransportAdapter::UdpClientReceiveListener::UdpClientReceiveListener(UdpRtpTransportAdapter *adapter,
                                                                           ListenerMode mode)
    : adapter_(adapter), mode_(mode)
{
}

void UdpRtpTransportAdapter::UdpClientReceiveListener::OnReceive(lmnet::socket_t fd,
                                                                 std::shared_ptr<lmnet::DataBuffer> buffer)
{
    if (!adapter_)
        return;

    if (mode_ == ListenerMode::RTP) {
        adapter_->OnRtpDataReceived(buffer);
    } else {
        adapter_->OnRtcpDataReceived(buffer);
    }
}

void UdpRtpTransportAdapter::UdpClientReceiveListener::OnClose(lmnet::socket_t fd) {}

void UdpRtpTransportAdapter::UdpClientReceiveListener::OnError(lmnet::socket_t fd, const std::string &errorInfo)
{
    LMRTSP_LOGE("UdpClientReceiveListener %s: OnError: fd %d, %s", mode_ == ListenerMode::RTP ? "RTP" : "RTCP", fd,
                errorInfo.c_str());
}

// UdpRtpTransportAdapter implementation
UdpRtpTransportAdapter::UdpRtpTransportAdapter()
    : clientRtpPort_(0), clientRtcpPort_(0), serverRtpPort_(0), serverRtcpPort_(0), unicast_(true), active_(false)
{
}

UdpRtpTransportAdapter::~UdpRtpTransportAdapter()
{
    Close();
}

bool UdpRtpTransportAdapter::Setup(const TransportConfig &config)
{
    config_ = config;

    // Extract client information from config
    client_ip_ = config.client_ip;
    clientRtpPort_ = config.client_rtp_port;
    clientRtcpPort_ = config.client_rtcp_port;
    serverRtpPort_ = config.server_rtp_port;
    serverRtcpPort_ = config.server_rtcp_port;
    unicast_ = config.unicast;

    bool success = false;

    if (config_.mode == TransportConfig::Mode::SOURCE) {
        success = InitializeUdpClients();
    } else if (config_.mode == TransportConfig::Mode::SINK) {
        success = InitializeUdpServers();
    }

    if (success) {
        active_ = true;
        LMRTSP_LOGI("UDP RTP transport adapter setup successfully (RTCP {})", IsRtcpEnabled() ? "enabled" : "disabled");
    } else {
        LMRTSP_LOGE("Failed to setup UDP RTP transport adapter");
        Close();
    }

    return success;
}

bool UdpRtpTransportAdapter::SendPacket(const uint8_t *data, size_t size)
{
    if (!active_) {
        LMRTSP_LOGE("UDP transport not active");
        return false;
    }

    if (client_ip_.empty() || clientRtpPort_ == 0) {
        LMRTSP_LOGE("Client RTP address not configured");
        return false;
    }

    if (!data || size == 0) {
        LMRTSP_LOGE("Invalid RTP data");
        return false;
    }

    bool result = false;

    // In SERVER mode, use UdpClient to send data (client was created with remote address)
    if (config_.mode == TransportConfig::Mode::SOURCE && rtp_client_) {
        result = rtp_client_->Send(data, size);
    }
    // In CLIENT mode, typically we don't send RTP packets
    else if (config_.mode == TransportConfig::Mode::SINK) {
        LMRTSP_LOGE("SINK mode typically doesn't send RTP packets");
        return false;
    } else {
        LMRTSP_LOGE("No suitable RTP sender available");
        return false;
    }

    if (!result) {
        LMRTSP_LOGE("Failed to send RTP packet to %s:%u", client_ip_.c_str(), clientRtpPort_);
    }

    return result;
}

bool UdpRtpTransportAdapter::SendRtcpPacket(const uint8_t *data, size_t size)
{
    if (!active_) {
        LMRTSP_LOGE("UDP transport not active");
        return false;
    }

    // Check if RTCP is enabled
    if (!IsRtcpEnabled()) {
        LMRTSP_LOGE("RTCP is disabled, cannot send RTCP packet");
        return false;
    }

    if (client_ip_.empty() || clientRtcpPort_ == 0) {
        LMRTSP_LOGE("Client RTCP address not configured");
        return false;
    }

    if (!data || size == 0) {
        LMRTSP_LOGE("Invalid RTCP data");
        return false;
    }

    bool result = false;

    // In SERVER mode, use UdpClient to send data
    if (config_.mode == TransportConfig::Mode::SOURCE && rtcp_client_) {
        result = rtcp_client_->Send(data, size);
    }
    // In CLIENT mode, typically we don't send RTCP packets
    else if (config_.mode == TransportConfig::Mode::SINK) {
        LMRTSP_LOGE("SINK mode typically doesn't send RTCP packets");
        return false;
    } else {
        LMRTSP_LOGE("No suitable RTCP sender available");
        return false;
    }

    if (!result) {
        LMRTSP_LOGE("Failed to send RTCP packet to %s:%u", client_ip_.c_str(), clientRtcpPort_);
    }

    return result;
}

void UdpRtpTransportAdapter::Close()
{
    active_ = false;

    if (rtp_client_) {
        rtp_client_->Close();
        rtp_client_.reset();
    }

    if (rtcp_client_) {
        rtcp_client_->Close();
        rtcp_client_.reset();
    }

    if (rtp_server_) {
        rtp_server_->Stop();
        rtp_server_.reset();
    }

    if (rtcp_server_) {
        rtcp_server_->Stop();
        rtcp_server_.reset();
    }

    rtp_server_listener_.reset();
    rtcp_server_listener_.reset();
    rtp_client_listener_.reset();
    rtcp_client_listener_.reset();

    LMRTSP_LOGI("UDP RTP transport adapter closed");
}

std::string UdpRtpTransportAdapter::GetTransportInfo() const
{
    std::ostringstream oss;
    oss << "UDP;unicast;client_port=" << clientRtpPort_ << "-" << clientRtcpPort_ << ";server_port=" << serverRtpPort_
        << "-" << serverRtcpPort_;
    return oss.str();
}

bool UdpRtpTransportAdapter::IsActive() const
{
    return active_;
}

bool UdpRtpTransportAdapter::InitializeUdpServers()
{
    bool rtcp_enabled = IsRtcpEnabled();
    if (serverRtpPort_ == 0 || (rtcp_enabled && serverRtcpPort_ == 0)) {
        uint16_t allocated_port = FindAvailablePortPair();
        if (allocated_port == 0) {
            LMRTSP_LOGE("Failed to allocate server port pair");
            return false;
        }
        serverRtpPort_ = allocated_port;
        if (rtcp_enabled) {
            serverRtcpPort_ = allocated_port + 1;
        }
        LMRTSP_LOGI("Allocated server ports: RTP={}, RTCP={}", serverRtpPort_, rtcp_enabled ? serverRtcpPort_ : 0);
    }

    // Create RTP server (always required)
    rtp_server_ = lmnet::UdpServer::Create(serverRtpPort_);
    if (!rtp_server_) {
        LMRTSP_LOGE("Failed to create RTP server on port %u", serverRtpPort_);
        return false;
    }
    rtp_server_listener_ = std::make_shared<UdpServerReceiveListener>(this, ListenerMode::RTP);
    rtp_server_->SetListener(rtp_server_listener_);
    if (!(rtp_server_->Init() && rtp_server_->Start())) {
        LMRTSP_LOGE("Failed to start RTP server on port %u", serverRtpPort_);
        return false;
    }

    // Create RTCP server only if RTCP is enabled
    if (rtcp_enabled) {
        rtcp_server_ = lmnet::UdpServer::Create(serverRtcpPort_);
        if (!rtcp_server_) {
            LMRTSP_LOGE("Failed to create RTCP server on port %u", serverRtcpPort_);
            rtp_server_->Stop();
            rtp_server_.reset();
            return false;
        }
        rtcp_server_listener_ = std::make_shared<UdpServerReceiveListener>(this, ListenerMode::RTCP);
        rtcp_server_->SetListener(rtcp_server_listener_);
        if (!(rtcp_server_->Init() && rtcp_server_->Start())) {
            LMRTSP_LOGE("Failed to start RTCP server on port %u", serverRtcpPort_);
            if (rtp_server_) {
                rtp_server_->Stop();
                rtp_server_.reset();
            }
            return false;
        }
        LMRTSP_LOGI("UDP servers initialized: RTP port %u, RTCP port %u", serverRtpPort_, serverRtcpPort_);
    } else {
        LMRTSP_LOGI("UDP servers initialized: RTP port %u (RTCP disabled)", serverRtpPort_);
    }

    return true;
}

bool UdpRtpTransportAdapter::InitializeUdpClients()
{
    if (client_ip_.empty() || clientRtpPort_ == 0) {
        LMRTSP_LOGE("Client address not configured for UDP clients");
        return false;
    }

    bool rtcp_enabled = IsRtcpEnabled();

    // Check RTCP configuration when RTCP is enabled
    if (rtcp_enabled && clientRtcpPort_ == 0) {
        LMRTSP_LOGE("Client RTCP port not configured but RTCP is enabled");
        return false;
    }

    uint16_t rtp_local_port = 0;
    uint16_t rtcp_local_port = 0;
    if (config_.mode == TransportConfig::Mode::SOURCE) {
        // Allocate server ports if not specified
        if (serverRtpPort_ == 0 || (rtcp_enabled && serverRtcpPort_ == 0)) {
            uint16_t allocated_port = FindAvailablePortPair();
            if (allocated_port == 0) {
                LMRTSP_LOGE("Failed to allocate server port pair for UDP clients");
                return false;
            }
            serverRtpPort_ = allocated_port;
            if (rtcp_enabled) {
                serverRtcpPort_ = allocated_port + 1;
            }
            LMRTSP_LOGI("Allocated local ports for SOURCE mode: RTP={}, RTCP={}", serverRtpPort_,
                        rtcp_enabled ? serverRtcpPort_ : 0);
        }
        rtp_local_port = serverRtpPort_;
        rtcp_local_port = serverRtcpPort_;
    }

    // Create RTP client (always required)
    rtp_client_ = lmnet::UdpClient::Create(client_ip_, clientRtpPort_, "", rtp_local_port);
    if (!rtp_client_) {
        LMRTSP_LOGE("Failed to create RTP client for %s:%u, local_port=%u", client_ip_.c_str(), clientRtpPort_,
                    rtp_local_port);
        return false;
    }
    if (!rtp_client_->Init()) {
        LMRTSP_LOGE("Failed to init RTP client for %s:%u, local_port=%u", client_ip_.c_str(), clientRtpPort_,
                    rtp_local_port);
        return false;
    }
    rtp_client_listener_ = std::make_shared<UdpClientReceiveListener>(this, ListenerMode::RTP);
    rtp_client_->SetListener(rtp_client_listener_);

    // Create RTCP client only if RTCP is enabled
    if (rtcp_enabled) {
        rtcp_client_ = lmnet::UdpClient::Create(client_ip_, clientRtcpPort_, "", rtcp_local_port);
        if (!rtcp_client_) {
            LMRTSP_LOGE("Failed to create RTCP client for %s:%u, local_port=%u", client_ip_.c_str(), clientRtcpPort_,
                        rtcp_local_port);
            rtp_client_->Close();
            return false;
        }
        if (!rtcp_client_->Init()) {
            LMRTSP_LOGE("Failed to init RTCP client for %s:%u, local_port=%u", client_ip_.c_str(), clientRtcpPort_,
                        rtcp_local_port);
            rtp_client_->Close();
            return false;
        }
        rtcp_client_listener_ = std::make_shared<UdpClientReceiveListener>(this, ListenerMode::RTCP);
        rtcp_client_->SetListener(rtcp_client_listener_);

        LMRTSP_LOGI("UDP clients configured: remote %s:%u(RTP), %s:%u(RTCP); local bind %u(RTP), %u(RTCP)",
                    client_ip_.c_str(), clientRtpPort_, client_ip_.c_str(), clientRtcpPort_, rtp_local_port,
                    rtcp_local_port);
    } else {
        LMRTSP_LOGI("UDP clients configured: remote %s:%u(RTP); local bind %u(RTP) (RTCP disabled)", client_ip_.c_str(),
                    clientRtpPort_, rtp_local_port);
    }

    return true;
}

uint16_t UdpRtpTransportAdapter::FindAvailablePortPair(uint16_t start_port)
{
    // Use lmnet helper to get an idle even port for RTP (RTCP will be +1)
    (void)start_port; // unused
    return lmnet::UdpServer::GetIdlePortPair();
}

void UdpRtpTransportAdapter::OnRtpDataReceived(std::shared_ptr<lmnet::DataBuffer> buffer) const
{
    if (listener_) {
        listener_->OnRtpDataReceived(buffer);
    }
}

void UdpRtpTransportAdapter::OnRtcpDataReceived(std::shared_ptr<lmnet::DataBuffer> buffer) const
{
    if (listener_) {
        listener_->OnRtcpDataReceived(buffer);
    }
}

bool UdpRtpTransportAdapter::IsRtcpEnabled() const
{
    if (config_.mode == TransportConfig::Mode::SOURCE) {
        return config_.client_rtcp_port != 0;
    } else {
        return config_.server_rtcp_port != 0;
    }
}
} // namespace lmshao::lmrtsp
