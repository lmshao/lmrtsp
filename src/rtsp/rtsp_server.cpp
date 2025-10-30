/**
 * @author SHAO Liming <lmshao@163.com>
 * @copyright Copyright (c) 2025 SHAO Liming
 * @license MIT
 *
 * SPDX-License-Identifier: MIT
 */

#include "lmrtsp/rtsp_server.h"

#include <lmnet/tcp_server.h>

#include "internal_logger.h"
#include "lmrtsp/irtsp_server_callback.h"
#include "lmrtsp/rtsp_session.h"
#include "rtsp_response.h"
#include "rtsp_server_listener.h"

namespace lmshao::lmrtsp {

// Base64 encoding table
static const char kBase64Table[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

// Helper function to encode SPS/PPS to Base64
static std::string Base64Encode(const std::vector<uint8_t> &data)
{
    std::string result;
    result.reserve((data.size() + 2) / 3 * 4);

    size_t i = 0;
    while (i + 2 < data.size()) {
        result += kBase64Table[(data[i] >> 2) & 0x3F];
        result += kBase64Table[((data[i] & 0x03) << 4) | ((data[i + 1] & 0xF0) >> 4)];
        result += kBase64Table[((data[i + 1] & 0x0F) << 2) | ((data[i + 2] & 0xC0) >> 6)];
        result += kBase64Table[data[i + 2] & 0x3F];
        i += 3;
    }

    if (i < data.size()) {
        result += kBase64Table[(data[i] >> 2) & 0x3F];
        if (i + 1 < data.size()) {
            result += kBase64Table[((data[i] & 0x03) << 4) | ((data[i + 1] & 0xF0) >> 4)];
            result += kBase64Table[((data[i + 1] & 0x0F) << 2)];
        } else {
            result += kBase64Table[(data[i] & 0x03) << 4];
            result += '=';
        }
        result += '=';
    }

    return result;
}

RTSPServer::RTSPServer()
{
    LMRTSP_LOGD("RTSPServer constructor called");
}

bool RTSPServer::Init(const std::string &ip, uint16_t port)
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
    serverListener_ = std::make_shared<RTSPServerListener>(shared_from_this());
    tcpServer_->SetListener(serverListener_);

    if (!tcpServer_->Init()) {
        LMRTSP_LOGE("Failed to initialize TCP server");
        return false;
    }

    LMRTSP_LOGD("RTSP server initialized successfully");
    return true;
}

bool RTSPServer::Start()
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

bool RTSPServer::Stop()
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

void RTSPServer::HandleRequest(std::shared_ptr<RTSPSession> session, const RTSPRequest &request)
{
    LMRTSP_LOGD("Handling %s request for session %s", request.method_.c_str(), session->GetSessionId().c_str());

    // Get client IP for callback notifications
    std::string client_ip = GetClientIP(session);

    // Process request directly through session state machine
    RTSPResponse response = session->ProcessRequest(request);

    // Notify callback about the request after processing
    const std::string &method = request.method_;
    if (method == "SETUP") {
        std::string transport = "";
        auto it = request.general_header_.find("Transport");
        if (it != request.general_header_.end()) {
            transport = it->second;
        }
        LMRTSP_LOGD("invoke OnStreamRequested");
        NotifyCallback(
            [&](IRTSPServerCallback *callback) { callback->OnSetupReceived(client_ip, transport, request.uri_); });
    } else if (method == "PLAY") {
        std::string range = "";
        auto it = request.general_header_.find("Range");
        if (it != request.general_header_.end()) {
            range = it->second;
        }
        NotifyCallback(
            [&](IRTSPServerCallback *callback) { callback->OnPlayReceived(client_ip, request.uri_, range); });
    } else if (method == "PAUSE") {
        NotifyCallback([&](IRTSPServerCallback *callback) { callback->OnPauseReceived(client_ip, request.uri_); });
    } else if (method == "TEARDOWN") {
        NotifyCallback([&](IRTSPServerCallback *callback) { callback->OnTeardownReceived(client_ip, request.uri_); });
    }

    // Send response
    auto lmnetSession = session->GetNetworkSession();
    if (lmnetSession) {
        LMRTSP_LOGD("Send response: \n%s", response.ToString().c_str());
        lmnetSession->Send(response.ToString());
    }
}

void RTSPServer::HandleStatelessRequest(std::shared_ptr<lmnet::Session> lmnetSession, const RTSPRequest &request)
{
    LMRTSP_LOGD("Handling stateless %s request", request.method_.c_str());

    RTSPResponse response;
    int cseq = 0;

    // Extract CSeq from request
    if (request.general_header_.find(CSEQ) != request.general_header_.end()) {
        cseq = std::stoi(request.general_header_.at(CSEQ));
    }

    if (request.method_ == METHOD_OPTIONS) {
        response = RTSPResponseFactory::CreateOptionsOK(cseq).SetServer("RTSP Server/1.0").Build();
    } else if (request.method_ == METHOD_DESCRIBE) {
        // Notify callback for stream request
        std::string client_ip = "";
        if (lmnetSession) {
            client_ip = lmnetSession->host;
        }
        LMRTSP_LOGD("invoke OnStreamRequested");
        NotifyCallback([&](IRTSPServerCallback *callback) { callback->OnStreamRequested(request.uri_, client_ip); });

        // Generate SDP for the requested stream
        std::string sdp = GenerateSDP(request.uri_, GetServerIP(), GetServerPort());
        response = RTSPResponseFactory::CreateDescribeOK(cseq).SetServer("RTSP Server/1.0").SetSdp(sdp).Build();
    } else {
        // This should not happen as we only call this for OPTIONS and DESCRIBE
        response = RTSPResponseFactory::CreateMethodNotAllowed(cseq).Build();
    }

    // Send response
    if (lmnetSession) {
        LMRTSP_LOGD("Send stateless response: \n%s", response.ToString().c_str());
        lmnetSession->Send(response.ToString());
    }
}

void RTSPServer::SendErrorResponse(std::shared_ptr<lmnet::Session> lmnetSession, const RTSPRequest &request,
                                   int statusCode, const std::string &reasonPhrase)
{
    int cseq = 0;

    // Extract CSeq from request
    if (request.general_header_.find(CSEQ) != request.general_header_.end()) {
        cseq = std::stoi(request.general_header_.at(CSEQ));
    }

    RTSPResponse response;
    switch (statusCode) {
        case 400:
            response = RTSPResponseFactory::CreateBadRequest(cseq).Build();
            break;
        case 404:
            response = RTSPResponseFactory::CreateNotFound(cseq).Build();
            break;
        case 454:
            response = RTSPResponseFactory::CreateSessionNotFound(cseq).Build();
            break;
        case 500:
            response = RTSPResponseFactory::CreateInternalServerError(cseq).Build();
            break;
        default:
            response = RTSPResponseFactory::CreateError(static_cast<StatusCode>(statusCode), cseq).Build();
            break;
    }

    // Send error response
    if (lmnetSession) {
        LMRTSP_LOGD("Send error response (%d %s): \n%s", statusCode, reasonPhrase.c_str(), response.ToString().c_str());
        lmnetSession->Send(response.ToString());
    }
}

std::shared_ptr<RTSPSession> RTSPServer::CreateSession(std::shared_ptr<lmnet::Session> lmnetSession)
{
    auto session = std::make_shared<RTSPSession>(lmnetSession, weak_from_this());
    {
        std::lock_guard<std::mutex> lock(sessionsMutex_);
        sessions_[session->GetSessionId()] = session;
    }
    LMRTSP_LOGD("Created new RTSP session: %s", session->GetSessionId().c_str());
    return session;
}

void RTSPServer::RemoveSession(const std::string &sessionId)
{
    std::lock_guard<std::mutex> lock(sessionsMutex_);
    auto it = sessions_.find(sessionId);
    if (it != sessions_.end()) {
        LMRTSP_LOGD("Removing RTSP session: %s", sessionId.c_str());
        sessions_.erase(it);
    }
}

std::shared_ptr<RTSPSession> RTSPServer::GetSession(const std::string &sessionId)
{
    std::lock_guard<std::mutex> lock(sessionsMutex_);
    auto it = sessions_.find(sessionId);
    if (it != sessions_.end()) {
        return it->second;
    }
    return nullptr;
}

std::unordered_map<std::string, std::shared_ptr<RTSPSession>> RTSPServer::GetSessions()
{
    std::lock_guard<std::mutex> lock(sessionsMutex_);
    return sessions_;
}

// Callback interface implementation
void RTSPServer::SetCallback(std::shared_ptr<IRTSPServerCallback> callback)
{
    std::lock_guard<std::mutex> lock(callbackMutex_);
    callback_ = callback;
    LMRTSP_LOGD("RTSP server callback set");
}

std::shared_ptr<IRTSPServerCallback> RTSPServer::GetCallback() const
{
    std::lock_guard<std::mutex> lock(callbackMutex_);
    return callback_;
}

// Media stream management implementation
bool RTSPServer::AddMediaStream(const std::string &stream_path, std::shared_ptr<MediaStreamInfo> stream_info)
{
    std::lock_guard<std::mutex> lock(streamsMutex_);
    mediaStreams_[stream_path] = stream_info;
    LMRTSP_LOGD("Added media stream: %s", stream_path.c_str());
    return true;
}

bool RTSPServer::RemoveMediaStream(const std::string &stream_path)
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

std::shared_ptr<MediaStreamInfo> RTSPServer::GetMediaStream(const std::string &stream_path)
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

std::vector<std::string> RTSPServer::GetMediaStreamPaths() const
{
    std::lock_guard<std::mutex> lock(streamsMutex_);
    std::vector<std::string> paths;
    for (const auto &pair : mediaStreams_) {
        paths.push_back(pair.first);
    }
    return paths;
}

// Client management implementation
std::vector<std::string> RTSPServer::GetConnectedClients() const
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

bool RTSPServer::DisconnectClient(const std::string &client_ip)
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

size_t RTSPServer::GetClientCount() const
{
    std::lock_guard<std::mutex> lock(sessionsMutex_);
    return sessions_.size();
}

// Server information
bool RTSPServer::IsRunning() const
{
    return running_.load();
}

std::string RTSPServer::GetServerIP() const
{
    return serverIP_;
}

uint16_t RTSPServer::GetServerPort() const
{
    return serverPort_;
}

// SDP generation implementation
std::string RTSPServer::GenerateSDP(const std::string &stream_path, const std::string &server_ip, uint16_t server_port)
{
    // Extract path from full RTSP URL if needed
    std::string path = stream_path;
    if (stream_path.find("rtsp://") == 0) {
        // Find the path part after host:port
        size_t schemeEnd = stream_path.find("://");
        if (schemeEnd != std::string::npos) {
            size_t hostStart = schemeEnd + 3;
            size_t pathStart = stream_path.find('/', hostStart);
            if (pathStart != std::string::npos) {
                path = stream_path.substr(pathStart);
            }
        }
    }

    auto stream_info = GetMediaStream(path);
    if (!stream_info) {
        LMRTSP_LOGE("Media stream not found: %s (original: %s)", path.c_str(), stream_path.c_str());
        return "";
    }

    // Basic SDP generation logic
    std::string sdp;
    sdp += "v=0\r\n";
    sdp += "o=- 0 0 IN IP4 " + server_ip + "\r\n";
    sdp += "s=RTSP Session\r\n";
    sdp += "c=IN IP4 " + server_ip + "\r\n";
    sdp += "t=0 0\r\n";
    sdp += "a=range:npt=0-\r\n"; // Add range attribute for VLC compatibility

    // Session-level control attribute - use wildcard
    sdp += "a=control:*\r\n";

    if (stream_info->media_type == "video") {
        // Use UDP mode (RTP/AVP), not TCP
        sdp += "m=video 0 RTP/AVP " + std::to_string(stream_info->payload_type) + "\r\n";
        sdp += "a=rtpmap:" + std::to_string(stream_info->payload_type) + " " + stream_info->codec + "/" +
               std::to_string(stream_info->clock_rate) + "\r\n";

        // Add fmtp with H.264 parameters if available
        if (stream_info->codec == "H264" && !stream_info->sps.empty() && !stream_info->pps.empty()) {
            // Extract profile-level-id from SPS (bytes 1-3)
            std::string profileLevelId = "42001f"; // Default: Baseline Profile Level 3.1
            if (stream_info->sps.size() >= 4) {
                char buf[7];
                snprintf(buf, sizeof(buf), "%02x%02x%02x", stream_info->sps[1], stream_info->sps[2],
                         stream_info->sps[3]);
                profileLevelId = buf;
            }

            // Base64 encode SPS and PPS
            std::string spsBase64 = Base64Encode(stream_info->sps);
            std::string ppsBase64 = Base64Encode(stream_info->pps);

            sdp += "a=fmtp:" + std::to_string(stream_info->payload_type) +
                   " packetization-mode=1;profile-level-id=" + profileLevelId + ";sprop-parameter-sets=" + spsBase64 +
                   "," + ppsBase64 + "\r\n";
        }

        if (stream_info->width > 0 && stream_info->height > 0) {
            sdp += "a=framerate:" + std::to_string(stream_info->frame_rate) + "\r\n";
        }

        // Media-level control attribute - use relative path
        sdp += "a=control:track0\r\n";
    } else if (stream_info->media_type == "audio") {
        sdp += "m=audio 0 RTP/AVP " + std::to_string(stream_info->payload_type) + "\r\n";
        sdp += "a=rtpmap:" + std::to_string(stream_info->payload_type) + " " + stream_info->codec + "/" +
               std::to_string(stream_info->sample_rate) + "\r\n";
        sdp += "a=control:track1\r\n";
    }

    return sdp;
}

// Helper methods
std::string RTSPServer::GetClientIP(std::shared_ptr<RTSPSession> session) const
{
    if (session && session->GetNetworkSession()) {
        return session->GetNetworkSession()->host;
    }
    return "";
}

void RTSPServer::NotifyCallback(std::function<void(IRTSPServerCallback *)> func)
{
    std::lock_guard<std::mutex> lock(callbackMutex_);
    if (callback_) {
        func(callback_.get());
    }
}

} // namespace lmshao::lmrtsp