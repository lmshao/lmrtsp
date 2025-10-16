/**
 * @author SHAO Liming <lmshao@163.com>
 * @copyright Copyright (c) 2025 SHAO Liming
 * @license MIT
 *
 * SPDX-License-Identifier: MIT
 */

#include "rtsp_server_listener.h"

#include <lmcore/data_buffer.h>
#include <lmnet/session.h>

#include <algorithm>

#include "internal_logger.h"
#include "rtsp_request.h"
#include "rtsp_server.h"
#include "rtsp_session.h"

namespace lmshao::lmrtsp {

// Helper function: Get all session IDs
// Since RTSPServer class doesn't provide GetAllSessions method, we need an alternative
// This function is a temporary solution, in actual application RTSPServer class should be modified to add
// GetAllSessions method
std::vector<std::string> GetSessionIds(std::shared_ptr<RTSPServer> server)
{
    std::vector<std::string> sessionIds;

    // Note: This is a temporary implementation, we actually cannot get all session IDs
    // Because RTSPServer class doesn't provide method to access sessions_ member
    // In actual application, RTSPServer class should be modified to add GetAllSessions method

    // Since we cannot get all sessions, return an empty list here
    // This means we cannot properly clean up RTSP sessions related to disconnected lmnet sessions
    // This may cause memory leaks or other issues

    return sessionIds;
}

RTSPServerListener::RTSPServerListener(std::shared_ptr<RTSPServer> server) : rtspServer_(server)
{
    RTSP_LOGD("RTSPServerListener created");
}

void RTSPServerListener::OnError(std::shared_ptr<lmnet::Session> session, const std::string &errorInfo)
{
    RTSP_LOGE("Network error for client %s:%d - %s", session->host.c_str(), session->port, errorInfo.c_str());

    // Log incomplete data if any exists
    auto it = incompleteRequests_.find(session->fd);
    if (it != incompleteRequests_.end()) {
        RTSP_LOGW("Client %s:%d had incomplete request data (%zu bytes) when error occurred", session->host.c_str(),
                  session->port, it->second.size());
        // Log first 100 characters of incomplete data for debugging
        size_t debugSize = (it->second.size() < 100) ? it->second.size() : 100;
        std::string debugData = it->second.substr(0, debugSize);
        for (char &c : debugData) {
            if (c < 32 && c != '\r' && c != '\n')
                c = '.';
        }
        RTSP_LOGW("Incomplete data: [%s]%s", debugData.c_str(), it->second.size() > 100 ? "..." : "");
    }

    // Clean up incomplete request data
    incompleteRequests_.erase(session->fd);

    // Notify callback
    auto server = rtspServer_.lock();
    if (server) {
        server->NotifyCallback([&](IRTSPServerCallback *callback) { callback->OnError(session->host, -1, errorInfo); });
    }
}

void RTSPServerListener::OnClose(std::shared_ptr<lmnet::Session> session)
{
    RTSP_LOGD("Client disconnected: %s:%d", session->host.c_str(), session->port);

    // Clean up incomplete request data
    incompleteRequests_.erase(session->fd);

    // Notify callback about client disconnection
    auto server = rtspServer_.lock();
    if (server) {
        server->NotifyCallback([&](IRTSPServerCallback *callback) { callback->OnClientDisconnected(session->host); });

        // Traverse all sessions to find RTSP sessions using this lmnet session
        // Note: We need to traverse all sessions and check if lmnet sessions match
        std::vector<std::string> sessionsToRemove;

        // Get all session IDs and check lmnet sessions
        auto sessions = server->GetSessions();
        for (const auto &pair : sessions) {
            auto rtspSession = pair.second;
            if (rtspSession && rtspSession->GetNetworkSession() == session) {
                // Found related session, mark for deletion
                sessionsToRemove.push_back(pair.first);
            }
        }

        // Delete marked sessions
        for (const auto &sessionId : sessionsToRemove) {
            server->RemoveSession(sessionId);
        }
    }
}

void RTSPServerListener::OnAccept(std::shared_ptr<lmnet::Session> session)
{
    RTSP_LOGD("New client connected: %s:%d", session->host.c_str(), session->port);

    // Notify callback about client connection
    auto server = rtspServer_.lock();
    if (server) {
        server->NotifyCallback([&](IRTSPServerCallback *callback) {
            callback->OnClientConnected(session->host, ""); // User-Agent will be obtained from RTSP request
        });
    }

    // Don't create RTSP session at this stage, wait until first RTSP request arrives
}

void RTSPServerListener::OnReceive(std::shared_ptr<lmnet::Session> session, std::shared_ptr<lmcore::DataBuffer> buffer)
{
    // Get received data
    std::string data(reinterpret_cast<const char *>(buffer->Data()), buffer->Size());
    RTSP_LOGD("Received data from %s:%d, size: %zu", session->host.c_str(), session->port, data.size());

    // Log raw data for debugging (first 200 characters)
    size_t debugSize = (data.size() < 200) ? data.size() : 200;
    std::string debugData = data.substr(0, debugSize);
    // Replace non-printable characters with dots for logging
    for (char &c : debugData) {
        if (c < 32 && c != '\r' && c != '\n') {
            c = '.';
        }
    }
    RTSP_LOGD("Raw data content: [%s]%s", debugData.c_str(), data.size() > 200 ? "..." : "");

    // Check if this is TCP interleaved data (starts with $)
    if (!data.empty() && data[0] == '$') {
        // This is TCP interleaved RTP/RTCP data, not an RTSP request
        HandleInterleavedData(session, data);
        return;
    }

    // Check if there's previously incomplete request data
    auto it = incompleteRequests_.find(session->fd);
    if (it != incompleteRequests_.end()) {
        // Merge previous data
        RTSP_LOGD("Found incomplete data (%zu bytes), merging with new data", it->second.size());
        data = it->second + data;
        incompleteRequests_.erase(it);
    }

    // Parse RTSP request
    if (!ParseRTSPRequest(data, session)) {
        // If parsing fails, data might be incomplete, save it and wait for more data
        HandleIncompleteData(session, data);
    }
}

bool RTSPServerListener::ParseRTSPRequest(const std::string &data, std::shared_ptr<lmnet::Session> session)
{
    // Check if it's a complete RTSP request
    // RTSP request ends with \r\n\r\n, or if there's message body, need to check Content-Length
    size_t headerEnd = data.find(CRLFCRLF);
    if (headerEnd == std::string::npos) {
        RTSP_LOGD("Incomplete RTSP request, waiting for more data");
        return false;
    }

    // Parse request headers
    std::string headerData = data.substr(0, headerEnd + 4); // Include \r\n\r\n
    // Check Content-Length
    size_t contentLengthPos = headerData.find(CONTENT_LENGTH);
    int contentLength = 0;
    if (contentLengthPos != std::string::npos) {
        size_t colonPos = headerData.find(':', contentLengthPos);
        if (colonPos != std::string::npos) {
            size_t valueStart = headerData.find_first_not_of(" \t", colonPos + 1);
            size_t valueEnd = headerData.find(CRLF, valueStart);
            if (valueStart != std::string::npos && valueEnd != std::string::npos) {
                std::string lengthStr = headerData.substr(valueStart, valueEnd - valueStart);
                // Remove any trailing whitespace
                size_t lastNonSpace = lengthStr.find_last_not_of(" \t\r\n");
                if (lastNonSpace != std::string::npos) {
                    lengthStr = lengthStr.substr(0, lastNonSpace + 1);
                }

                try {
                    if (!lengthStr.empty()) {
                        contentLength = std::stoi(lengthStr);
                    }
                } catch (const std::exception &e) {
                    RTSP_LOGE("Failed to parse Content-Length value: '%s', error: %s", lengthStr.c_str(), e.what());
                    contentLength = 0;
                }
            }
        }
    }

    // Check if there's complete message body
    if (data.size() < headerEnd + 4 + contentLength) {
        RTSP_LOGD("Incomplete RTSP request body, waiting for more data");
        return false;
    }

    // Extract complete request data
    std::string completeRequest = data.substr(0, headerEnd + 4 + contentLength);

    // Use RTSPRequest::FromString to parse request
    try {
        auto request = RTSPRequest::FromString(completeRequest);

        // Add detailed logging for request parsing
        RTSP_LOGD("Parsed RTSP request - Method: [%s], URI: [%s], Version: [%s]", request.method_.c_str(),
                  request.uri_.c_str(), request.version_.c_str());

        // Check if request parsing was successful
        if (request.method_.empty()) {
            RTSP_LOGE("Failed to parse RTSP method from request. Request content:\n%s", completeRequest.c_str());
            return false;
        }

        // Get server instance
        auto server = rtspServer_.lock();
        if (!server) {
            RTSP_LOGE("RTSP server instance not available");
            return false;
        }

        // Handle stateless requests (OPTIONS, DESCRIBE) directly without creating session
        if (request.method_ == METHOD_OPTIONS || request.method_ == METHOD_DESCRIBE) {
            RTSP_LOGD("Handling stateless request [%s]: \n%s", request.method_.c_str(), completeRequest.c_str());
            server->HandleStatelessRequest(session, request);
            // Return immediately after handling stateless request to avoid double processing
            return true;
        } else {
            // Get or create RTSP session for stateful requests
            std::shared_ptr<RTSPSession> rtspSession = nullptr;

            // Check if session ID already exists
            std::string sessionId;
            if (request.general_header_.find(SESSION) != request.general_header_.end()) {
                sessionId = request.general_header_.at(SESSION);
                rtspSession = server->GetSession(sessionId);
                RTSP_LOGD("Found existing session ID: %s", sessionId.c_str());
            }

            // For SETUP request, create a new session if none exists
            // For other requests, session must already exist
            if (!rtspSession && request.method_ == METHOD_SETUP) {
                RTSP_LOGD("Creating new RTSP session for SETUP request");
                rtspSession = server->CreateSession(session);
            }

            // Handle request
            if (rtspSession) {
                RTSP_LOGD("Handling stateful request [%s]: \n%s", request.method_.c_str(), completeRequest.c_str());
                server->HandleRequest(rtspSession, request);
            } else {
                RTSP_LOGE("Failed to create or find RTSP session for method: [%s]. Request:\n%s",
                          request.method_.c_str(), completeRequest.c_str());
                // Send error response for requests that require a session but don't have one
                server->SendErrorResponse(session, request, 454, "Session Not Found");
            }
        }

        // Check if there's remaining data (may contain multiple requests
        if (completeRequest.size() < data.size()) {
            std::string remainingData = data.substr(completeRequest.size());
            // Recursively process remaining data
            ParseRTSPRequest(remainingData, session);
        }

        return true;
    } catch (const std::exception &e) {
        RTSP_LOGE("Failed to parse RTSP request: %s", e.what());
        return false;
    }
}

void RTSPServerListener::HandleIncompleteData(std::shared_ptr<lmnet::Session> session, const std::string &data)
{
    // Store incomplete data, wait for more data to arrive
    incompleteRequests_[session->fd] = data;
    RTSP_LOGD("Stored incomplete request data for client %s:%d, size: %zu", session->host.c_str(), session->port,
              data.size());
}

void RTSPServerListener::HandleInterleavedData(std::shared_ptr<lmnet::Session> session, const std::string &data)
{
    // TCP interleaved data format: $<channel><length><data>
    // This data should be ignored as it's from VLC (client to server RTCP feedback)
    // and not needed for our simple streaming implementation

    RTSP_LOGD("Received TCP interleaved data from %s:%d, size: %zu (ignored)", session->host.c_str(), session->port,
              data.size());

    // Note: In a complete implementation, we would parse this as RTCP feedback
    // from the client and use it to adjust streaming parameters
    // For our simple streaming server, we can safely ignore client RTCP packets
}

} // namespace lmshao::lmrtsp