/**
 * @author SHAO Liming <lmshao@163.com>
 * @copyright Copyright (c) 2025 SHAO Liming
 * @license MIT
 *
 * SPDX-License-Identifier: MIT
 */

#include "lmrtsp/rtsp_server.h"

#include <lmcore/base64.h>
#include <lmcore/hex.h>
#include <lmnet/tcp_server.h>

#include "internal_logger.h"
#include "lmrtsp/irtsp_server_listener.h"
#include "lmrtsp/rtsp_server_session.h"
#include "rtsp_response.h"
#include "rtsp_server_listener.h"

namespace lmshao::lmrtsp {

RtspServer::RtspServer()
{
    LMRTSP_LOGD("RtspServer constructor called");
}

bool RtspServer::Init(const std::string &ip, uint16_t port)
{
    LMRTSP_LOGD("Initializing RTSP server on %s:%d", ip.c_str(), port);

    serverIP_ = ip;
    serverPort_ = port;

    // Create TCP server
    tcpServer_ = lmnet::TcpServer::Create(ip, port);
    if (!tcpServer_) {
        LMRTSP_LOGE("Failed to create TCP server");
        return false;
    }

    // Set listener
    serverListener_ = std::make_shared<RtspServerListener>(shared_from_this());
    tcpServer_->SetListener(serverListener_);

    if (!tcpServer_->Init()) {
        LMRTSP_LOGE("Failed to initialize TCP server");
        return false;
    }

    LMRTSP_LOGD("RTSP server initialized successfully");
    return true;
}

bool RtspServer::Start()
{
    LMRTSP_LOGD("Starting RTSP server");
    if (!tcpServer_) {
        LMRTSP_LOGE("TCP server not initialized");
        return false;
    }

    if (!tcpServer_->Start()) {
        LMRTSP_LOGE("Failed to start TCP server");
        return false;
    }

    running_.store(true);
    LMRTSP_LOGD("RTSP server started successfully");
    return true;
}

bool RtspServer::Stop()
{
    LMRTSP_LOGD("Stopping RTSP server");
    if (!tcpServer_) {
        LMRTSP_LOGE("TCP server not initialized");
        return false;
    }

    if (!tcpServer_->Stop()) {
        LMRTSP_LOGE("Failed to stop TCP server");
        return false;
    }

    running_.store(false);

    // Clean up all sessions
    {
        std::lock_guard<std::mutex> lock(sessionsMutex_);
        sessions_.clear();
    }

    LMRTSP_LOGD("RTSP server stopped successfully");
    return true;
}

void RtspServer::HandleRequest(std::shared_ptr<RtspServerSession> session, const RtspRequest &request)
{
    LMRTSP_LOGD("Handling %s request for session %s", request.method_.c_str(), session->GetSessionId().c_str());

    // Get client IP for callback notifications
    std::string client_ip = GetClientIP(session);

    // Pre-process SETUP request - extract and set media stream info BEFORE processing
    const std::string &method = request.method_;
    if (method == "SETUP") {
        // Extract stream path from URI (remove /track0, /track1, etc and rtsp:// prefix)
        std::string stream_path = request.uri_;
        int track_index = -1;

        // Remove rtsp:// prefix if present
        size_t rtsp_pos = stream_path.find("rtsp://");
        if (rtsp_pos != std::string::npos) {
            size_t slash_pos = stream_path.find('/', rtsp_pos + 7);
            if (slash_pos != std::string::npos) {
                stream_path = stream_path.substr(slash_pos);
            }
        }

        size_t track_pos = stream_path.rfind("/track");
        if (track_pos != std::string::npos) {
            // Verify it's followed by digits (track number)
            bool is_track_suffix = true;
            std::string track_num_str;
            for (size_t i = track_pos + 6; i < stream_path.length(); ++i) {
                if (!std::isdigit(stream_path[i])) {
                    is_track_suffix = false;
                    break;
                }
                track_num_str += stream_path[i];
            }
            if (is_track_suffix && !track_num_str.empty()) {
                track_index = std::stoi(track_num_str);
                stream_path = stream_path.substr(0, track_pos);
                LMRTSP_LOGD("Extracted track index: %d from SETUP URI", track_index);
            }
        }

        // Remove trailing slash if present (e.g., /path/file.h264/ -> /path/file.h264)
        // This handles cases where clients append a trailing slash to the URI
        if (!stream_path.empty() && stream_path.back() == '/') {
            stream_path.pop_back();
        }

        // Get media stream info
        auto stream_info = GetMediaStream(stream_path);
        if (stream_info) {
            // Check if this is a multi-track stream and track index is specified
            if (track_index >= 0 && !stream_info->sub_tracks.empty()) {
                if (track_index < static_cast<int>(stream_info->sub_tracks.size())) {
                    // Set the specific sub-track as media stream info
                    session->SetMediaStreamInfo(stream_info->sub_tracks[track_index]);
                    LMRTSP_LOGD("Set sub-track %d MediaStreamInfo - codec: %s", track_index,
                                stream_info->sub_tracks[track_index]->codec.c_str());
                } else {
                    LMRTSP_LOGW("Track index %d out of range (total tracks: %zu)", track_index,
                                stream_info->sub_tracks.size());
                    session->SetMediaStreamInfo(stream_info);
                }
            } else {
                // Single track stream or no track index specified
                session->SetMediaStreamInfo(stream_info);
                LMRTSP_LOGD("Set MediaStreamInfo before ProcessRequest - codec: %s", stream_info->codec.c_str());
            }
        } else {
            LMRTSP_LOGW("No MediaStreamInfo found for stream: %s", stream_path.c_str());
        }
    }

    // Process request directly through session state machine
    RtspResponse response = session->ProcessRequest(request);

    // Notify callback about the request after processing
    if (method == "SETUP") {
        std::string transport = "";
        auto it = request.general_header_.find("Transport");
        if (it != request.general_header_.end()) {
            transport = it->second;
        }
        LMRTSP_LOGD("invoke OnStreamRequested");
        NotifyListener(
            [&](IRtspServerListener *listener) { listener->OnSetupReceived(client_ip, transport, request.uri_); });
    } else if (method == "PLAY") {
        std::string range = "";
        auto it = request.general_header_.find("Range");
        if (it != request.general_header_.end()) {
            range = it->second;
        }
        NotifyListener(
            [&](IRtspServerListener *listener) { listener->OnPlayReceived(client_ip, request.uri_, range); });
    } else if (method == "PAUSE") {
        NotifyListener([&](IRtspServerListener *listener) { listener->OnPauseReceived(client_ip, request.uri_); });
    } else if (method == "TEARDOWN") {
        NotifyListener([&](IRtspServerListener *listener) { listener->OnTeardownReceived(client_ip, request.uri_); });
    }

    // Send response
    auto lmnetSession = session->GetNetworkSession();
    if (lmnetSession) {
        LMRTSP_LOGD("Send response: \n%s", response.ToString().c_str());
        lmnetSession->Send(response.ToString());
    }
}

void RtspServer::HandleStatelessRequest(std::shared_ptr<lmnet::Session> lmnetSession, const RtspRequest &request)
{
    LMRTSP_LOGD("Handling stateless %s request", request.method_.c_str());

    RtspResponse response;
    int cseq = 0;

    // Extract CSeq from request
    if (request.general_header_.find(CSEQ) != request.general_header_.end()) {
        cseq = std::stoi(request.general_header_.at(CSEQ));
    }

    if (request.method_ == METHOD_OPTIONS) {
        response = RtspResponseFactory::CreateOptionsOK(cseq).SetServer("RTSP Server/1.0").Build();
    } else if (request.method_ == METHOD_DESCRIBE) {
        // Notify callback for stream request
        std::string client_ip = "";
        if (lmnetSession) {
            client_ip = lmnetSession->host;
        }
        LMRTSP_LOGD("invoke OnStreamRequested");
        NotifyListener([&](IRtspServerListener *listener) { listener->OnStreamRequested(request.uri_, client_ip); });

        // Generate SDP for the requested stream
        std::string sdp = GenerateSDP(request.uri_, GetServerIP(), GetServerPort());
        response = RtspResponseFactory::CreateDescribeOK(cseq).SetServer("RTSP Server/1.0").SetSdp(sdp).Build();
    } else {
        // This should not happen as we only call this for OPTIONS and DESCRIBE
        response = RtspResponseFactory::CreateMethodNotAllowed(cseq).Build();
    }

    // Send response
    if (lmnetSession) {
        LMRTSP_LOGD("Send stateless response: \n%s", response.ToString().c_str());
        lmnetSession->Send(response.ToString());
    }
}

void RtspServer::SendErrorResponse(std::shared_ptr<lmnet::Session> lmnetSession, const RtspRequest &request,
                                   int statusCode, const std::string &reasonPhrase)
{
    int cseq = 0;

    // Extract CSeq from request
    if (request.general_header_.find(CSEQ) != request.general_header_.end()) {
        cseq = std::stoi(request.general_header_.at(CSEQ));
    }

    RtspResponse response;
    switch (statusCode) {
        case 400:
            response = RtspResponseFactory::CreateBadRequest(cseq).Build();
            break;
        case 404:
            response = RtspResponseFactory::CreateNotFound(cseq).Build();
            break;
        case 454:
            response = RtspResponseFactory::CreateSessionNotFound(cseq).Build();
            break;
        case 500:
            response = RtspResponseFactory::CreateInternalServerError(cseq).Build();
            break;
        default:
            response = RtspResponseFactory::CreateError(static_cast<StatusCode>(statusCode), cseq).Build();
            break;
    }

    // Send error response
    if (lmnetSession) {
        LMRTSP_LOGD("Send error response (%d %s): \n%s", statusCode, reasonPhrase.c_str(), response.ToString().c_str());
        lmnetSession->Send(response.ToString());
    }
}

std::shared_ptr<RtspServerSession> RtspServer::CreateSession(std::shared_ptr<lmnet::Session> lmnetSession)
{
    auto session = std::make_shared<RtspServerSession>(lmnetSession, weak_from_this());
    {
        std::lock_guard<std::mutex> lock(sessionsMutex_);
        sessions_[session->GetSessionId()] = session;
    }
    LMRTSP_LOGD("Created new RTSP session: %s", session->GetSessionId().c_str());
    return session;
}

void RtspServer::RemoveSession(const std::string &sessionId)
{
    std::shared_ptr<RtspServerSession> session;
    {
        std::lock_guard<std::mutex> lock(sessionsMutex_);
        auto it = sessions_.find(sessionId);
        if (it != sessions_.end()) {
            LMRTSP_LOGD("Removing RTSP session: %s", sessionId.c_str());
            session = it->second; // Keep reference before erasing
            sessions_.erase(it);
        }
    }

    // Notify callback about session destruction (outside lock to avoid deadlock)
    if (session) {
        NotifyListener([&](IRtspServerListener *listener) { listener->OnSessionDestroyed(sessionId); });
    }
}

std::shared_ptr<RtspServerSession> RtspServer::GetSession(const std::string &sessionId)
{
    std::lock_guard<std::mutex> lock(sessionsMutex_);
    auto it = sessions_.find(sessionId);
    if (it != sessions_.end()) {
        return it->second;
    }
    return nullptr;
}

std::unordered_map<std::string, std::shared_ptr<RtspServerSession>> RtspServer::GetSessions()
{
    std::lock_guard<std::mutex> lock(sessionsMutex_);
    return sessions_;
}

// Listener interface implementation
void RtspServer::SetListener(std::shared_ptr<IRtspServerListener> listener)
{
    std::lock_guard<std::mutex> lock(listenerMutex_);
    listener_ = listener;
    LMRTSP_LOGD("RTSP server listener set");
}

std::shared_ptr<IRtspServerListener> RtspServer::GetListener() const
{
    std::lock_guard<std::mutex> lock(listenerMutex_);
    return listener_;
}

// Media stream management implementation
bool RtspServer::AddMediaStream(const std::string &stream_path, std::shared_ptr<MediaStreamInfo> stream_info)
{
    std::lock_guard<std::mutex> lock(streamsMutex_);
    mediaStreams_[stream_path] = stream_info;
    LMRTSP_LOGD("Added media stream: %s", stream_path.c_str());
    return true;
}

bool RtspServer::RemoveMediaStream(const std::string &stream_path)
{
    std::lock_guard<std::mutex> lock(streamsMutex_);
    auto it = mediaStreams_.find(stream_path);
    if (it != mediaStreams_.end()) {
        mediaStreams_.erase(it);
        LMRTSP_LOGD("Removed media stream: %s", stream_path.c_str());
        return true;
    }
    return false;
}

std::shared_ptr<MediaStreamInfo> RtspServer::GetMediaStream(const std::string &stream_path)
{
    std::lock_guard<std::mutex> lock(streamsMutex_);

    // Debug: Log all available streams
    LMRTSP_LOGD("Looking for stream: %s", stream_path.c_str());
    LMRTSP_LOGD("Available streams count: %zu", mediaStreams_.size());
    for (const auto &pair : mediaStreams_) {
        LMRTSP_LOGD("  - Stream: '%s'", pair.first.c_str());
    }

    auto it = mediaStreams_.find(stream_path);
    if (it != mediaStreams_.end()) {
        LMRTSP_LOGD("Stream found: %s", stream_path.c_str());
        return it->second;
    }
    LMRTSP_LOGD("Stream not found: %s", stream_path.c_str());
    return nullptr;
}

std::vector<std::string> RtspServer::GetMediaStreamPaths() const
{
    std::lock_guard<std::mutex> lock(streamsMutex_);
    std::vector<std::string> paths;
    for (const auto &pair : mediaStreams_) {
        paths.push_back(pair.first);
    }
    return paths;
}

// Client management implementation
std::vector<std::string> RtspServer::GetConnectedClients() const
{
    std::lock_guard<std::mutex> lock(sessionsMutex_);
    std::vector<std::string> clients;
    for (const auto &pair : sessions_) {
        auto session = pair.second;
        if (session && session->GetNetworkSession()) {
            clients.push_back(session->GetNetworkSession()->host);
        }
    }
    return clients;
}

bool RtspServer::DisconnectClient(const std::string &client_ip)
{
    std::lock_guard<std::mutex> lock(sessionsMutex_);
    std::vector<std::string> sessionsToRemove;

    for (const auto &pair : sessions_) {
        auto session = pair.second;
        if (session && session->GetNetworkSession() && session->GetNetworkSession()->host == client_ip) {
            sessionsToRemove.push_back(pair.first);
        }
    }

    for (const auto &sessionId : sessionsToRemove) {
        sessions_.erase(sessionId);
    }

    return !sessionsToRemove.empty();
}

size_t RtspServer::GetClientCount() const
{
    std::lock_guard<std::mutex> lock(sessionsMutex_);
    return sessions_.size();
}

// Server information
bool RtspServer::IsRunning() const
{
    return running_.load();
}

std::string RtspServer::GetServerIP() const
{
    return serverIP_;
}

uint16_t RtspServer::GetServerPort() const
{
    return serverPort_;
}

// SDP generation moved to rtsp_server_sdp.cpp

// Helper methods
std::string RtspServer::GetClientIP(std::shared_ptr<RtspServerSession> session) const
{
    if (session && session->GetNetworkSession()) {
        return session->GetNetworkSession()->host;
    }
    return "";
}

void RtspServer::NotifyListener(std::function<void(IRtspServerListener *)> func)
{
    std::lock_guard<std::mutex> lock(listenerMutex_);
    if (listener_) {
        func(listener_.get());
    }
}

} // namespace lmshao::lmrtsp