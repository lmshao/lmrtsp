/**
 * @author SHAO Liming <lmshao@163.com>
 * @copyright Copyright (c) 2025 SHAO Liming
 * @license MIT
 *
 * SPDX-License-Identifier: MIT
 */

#include "rtsp_server.h"

#include <network/tcp_server.h>

#include "rtsp_command.h"
#include "rtsp_log.h"
#include "rtsp_server_listener.h"
#include "rtsp_session.h"

namespace lmshao::rtsp {

std::shared_ptr<RTSPServer> RTSPServer::GetInstance()
{
    static std::shared_ptr<RTSPServer> instance(new RTSPServer());
    return instance;
}

bool RTSPServer::Init(const std::string &ip, uint16_t port)
{
    RTSP_LOGD("Initializing RTSP server on %s:%d", ip.c_str(), port);

    // Create TCP server
    tcpServer_ = network::TcpServer::Create(ip, port);
    if (!tcpServer_) {
        RTSP_LOGE("Failed to create TCP server");
        return false;
    }

    // Set listener
    serverListener_ = std::make_shared<RTSPServerListener>(shared_from_this());
    tcpServer_->SetListener(serverListener_);

    if (!tcpServer_->Init()) {
        RTSP_LOGE("Failed to initialize TCP server");
        return false;
    }

    RTSP_LOGD("RTSP server initialized successfully");
    return true;
}

bool RTSPServer::Start()
{
    RTSP_LOGD("Starting RTSP server");
    if (!tcpServer_) {
        RTSP_LOGE("TCP server not initialized");
        return false;
    }

    if (!tcpServer_->Start()) {
        RTSP_LOGE("Failed to start TCP server");
        return false;
    }

    RTSP_LOGD("RTSP server started successfully");
    return true;
}

bool RTSPServer::Stop()
{
    RTSP_LOGD("Stopping RTSP server");
    if (!tcpServer_) {
        RTSP_LOGE("TCP server not initialized");
        return false;
    }

    if (!tcpServer_->Stop()) {
        RTSP_LOGE("Failed to stop TCP server");
        return false;
    }

    // Clean up all sessions
    sessions_.clear();

    RTSP_LOGD("RTSP server stopped successfully");
    return true;
}

void RTSPServer::HandleRequest(std::shared_ptr<RTSPSession> session, const RTSPRequest &request)
{
    RTSP_LOGD("Handling %s request for session %s", request.method_.c_str(), session->GetSessionId().c_str());

    // Use command pattern to handle request
    auto command = RTSPCommandFactory::CreateCommand(request);
    if (command) {
        RTSPResponse response = command->Execute(session.get());

        // Send response
        auto networkSession = session->GetNetworkSession();
        if (networkSession) {
            networkSession->Send(response.ToString());
        }
    } else {
        RTSP_LOGE("Unknown RTSP method: %s", request.method_.c_str());

        // Send error response
        int cseq = 0;
        if (request.general_header_.find("CSeq") != request.general_header_.end()) {
            cseq = std::stoi(request.general_header_.at("CSeq"));
        }
        auto response = RTSPResponseFactory::CreateError(StatusCode::NotImplemented, cseq).Build();
        auto networkSession = session->GetNetworkSession();
        if (networkSession) {
            networkSession->Send(response.ToString());
        }
    }
}

std::shared_ptr<RTSPSession> RTSPServer::CreateSession(std::shared_ptr<network::Session> networkSession)
{
    auto session = std::make_shared<RTSPSession>(networkSession);
    sessions_[session->GetSessionId()] = session;
    RTSP_LOGD("Created new RTSP session: %s", session->GetSessionId().c_str());
    return session;
}

void RTSPServer::RemoveSession(const std::string &sessionId)
{
    auto it = sessions_.find(sessionId);
    if (it != sessions_.end()) {
        RTSP_LOGD("Removing RTSP session: %s", sessionId.c_str());
        sessions_.erase(it);
    }
}

std::shared_ptr<RTSPSession> RTSPServer::GetSession(const std::string &sessionId)
{
    auto it = sessions_.find(sessionId);
    if (it != sessions_.end()) {
        return it->second;
    }
    return nullptr;
}

} // namespace lmshao::rtsp