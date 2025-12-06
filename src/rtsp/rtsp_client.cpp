/**
 * @author SHAO Liming <lmshao@163.com>
 * @copyright Copyright (c) 2025 SHAO Liming
 * @license MIT
 *
 * SPDX-License-Identifier: MIT
 */

#include "lmrtsp/rtsp_client.h"

#include <lmcore/url.h>

#include "internal_logger.h"
#include "lmnet/iclient_listener.h"
#include "lmnet/tcp_client.h"
#include "lmrtsp/rtsp_client_session.h"
#include "lmrtsp/rtsp_headers.h"
#include "lmrtsp/rtsp_request.h"
#include "lmrtsp/rtsp_response.h"

namespace lmshao::lmrtsp {

// TCP Client Listener implementation
class RtspClient::TcpClientListener : public lmnet::IClientListener {
public:
    explicit TcpClientListener(std::weak_ptr<RtspClient> client) : client_(client) {}

    void OnReceive(lmnet::socket_t fd, std::shared_ptr<lmnet::DataBuffer> buffer) override
    {
        if (auto client = client_.lock()) {
            std::string response_str(reinterpret_cast<const char *>(buffer->Data()), buffer->Size());
            LMRTSP_LOGD("Received RTSP response: %s", response_str.c_str());

            try {
                RtspResponse response = RtspResponse::FromString(response_str);
                client->HandleResponse(response);
            } catch (const std::exception &e) {
                LMRTSP_LOGE("Failed to parse RTSP response: %s", e.what());
                client->NotifyError(-1, std::string("Failed to parse response: ") + e.what());
            }
        }
    }

    void OnClose(lmnet::socket_t fd) override
    {
        if (auto client = client_.lock()) {
            LMRTSP_LOGI("RTSP client disconnected from server");
            client->connected_.store(false);
            client->NotifyListener(
                [client](IRtspClientListener *listener) { listener->OnDisconnected(client->baseUrl_); });
        }
    }

    void OnError(lmnet::socket_t fd, const std::string &errorInfo) override
    {
        if (auto client = client_.lock()) {
            LMRTSP_LOGE("RTSP client error: %s", errorInfo.c_str());
            client->NotifyError(-1, errorInfo);
        }
    }

private:
    std::weak_ptr<RtspClient> client_;
};

// RtspClient implementation
RtspClient::RtspClient()
{
    // tcpListener_ will be created in Connect() method when we know the server IP and port
}

RtspClient::RtspClient(std::shared_ptr<IRtspClientListener> listener) : listener_(listener)
{
    // tcpListener_ will be created in Connect() method when we know the server IP and port
}

RtspClient::~RtspClient()
{
    Disconnect();
}

bool RtspClient::Connect(const std::string &url, int timeout_ms)
{
    try {
        ParseUrl(url, serverIP_, serverPort_, baseUrl_);

        LMRTSP_LOGI("Connecting to RTSP server: %s:%d", serverIP_.c_str(), serverPort_);

        // Create TCP client with server IP and port
        tcpClient_ = lmnet::TcpClient::Create(serverIP_, serverPort_);
        if (!tcpClient_->Init()) {
            LMRTSP_LOGE("Failed to initialize TCP client");
            return false;
        }

        tcpClient_->SetListener(std::make_shared<TcpClientListener>(shared_from_this()));

        if (!tcpClient_->Connect()) {
            LMRTSP_LOGE("Failed to connect to %s:%d", serverIP_.c_str(), serverPort_);
            return false;
        }

        // TCP connection established
        connected_.store(true);
        LMRTSP_LOGI("TCP connection established to %s:%d", serverIP_.c_str(), serverPort_);

        // Connection already established, proceed with RTSP communication

        LMRTSP_LOGI("Connected to RTSP server successfully");
        return true;
    } catch (const std::exception &e) {
        LMRTSP_LOGE("Exception during connect: %s", e.what());
        return false;
    }
}

bool RtspClient::Disconnect()
{
    try {
        // Teardown all sessions
        std::lock_guard<std::mutex> lock(sessionsMutex_);
        for (auto &[session_id, session] : sessions_) {
            session->Cleanup();
        }
        sessions_.clear();

        // Close TCP client
        if (tcpClient_) {
            tcpClient_->Close();
        }

        connected_.store(false);
        LMRTSP_LOGI("Disconnected from RTSP server");
        return true;
    } catch (const std::exception &e) {
        LMRTSP_LOGE("Exception during disconnect: %s", e.what());
        return false;
    }
}

bool RtspClient::IsConnected() const
{
    return connected_.load();
}

bool RtspClient::Options(const std::string &url)
{
    if (!connected_.load()) {
        LMRTSP_LOGE("Not connected to server");
        return false;
    }

    try {
        RtspRequest request;
        request.method_ = "OPTIONS";
        request.uri_ = url;
        request.version_ = RTSP_VERSION;

        request.general_header_["CSeq"] = GenerateCSeq();
        request.general_header_["User-Agent"] = userAgent_;

        std::string request_str = request.ToString();
        LMRTSP_LOGD("Sending OPTIONS request");

        return SendRequest(request);
    } catch (const std::exception &e) {
        LMRTSP_LOGE("Exception in OPTIONS: %s", e.what());
        return false;
    }
}

bool RtspClient::Describe(const std::string &url)
{
    if (!connected_.load()) {
        LMRTSP_LOGE("Not connected to server");
        return false;
    }

    try {
        RtspRequest request;
        request.method_ = "DESCRIBE";
        request.uri_ = url;
        request.version_ = RTSP_VERSION;

        request.general_header_["CSeq"] = GenerateCSeq();
        request.general_header_["User-Agent"] = userAgent_;
        request.general_header_["Accept"] = "application/sdp";

        std::string request_str = request.ToString();
        LMRTSP_LOGD("Sending DESCRIBE request");

        return SendRequest(request);
    } catch (const std::exception &e) {
        LMRTSP_LOGE("Exception in DESCRIBE: %s", e.what());
        return false;
    }
}

bool RtspClient::Setup(const std::string &url, const std::string &transport)
{
    if (!connected_.load()) {
        LMRTSP_LOGE("Not connected to server");
        return false;
    }

    try {
        RtspRequest request;
        request.method_ = "SETUP";
        request.uri_ = url;
        request.version_ = RTSP_VERSION;

        request.general_header_["CSeq"] = GenerateCSeq();
        request.general_header_["User-Agent"] = userAgent_;
        request.general_header_["Transport"] = transport;

        std::string request_str = request.ToString();
        LMRTSP_LOGD("Sending SETUP request:\n%s", request_str.c_str());

        return SendRequest(request);
    } catch (const std::exception &e) {
        LMRTSP_LOGE("Exception in SETUP: %s", e.what());
        return false;
    }
}

bool RtspClient::Play(const std::string &url)
{
    if (!connected_.load()) {
        LMRTSP_LOGE("Not connected to server");
        return false;
    }

    try {
        RtspRequest request;
        request.method_ = "PLAY";
        request.uri_ = url;
        request.version_ = RTSP_VERSION;

        request.general_header_["CSeq"] = GenerateCSeq();
        request.general_header_["User-Agent"] = userAgent_;

        std::string request_str = request.ToString();
        LMRTSP_LOGD("Sending PLAY request:\n%s", request_str.c_str());

        return SendRequest(request);
    } catch (const std::exception &e) {
        LMRTSP_LOGE("Exception in PLAY: %s", e.what());
        return false;
    }
}

bool RtspClient::Pause(const std::string &url)
{
    if (!connected_.load()) {
        LMRTSP_LOGE("Not connected to server");
        return false;
    }

    try {
        RtspRequest request;
        request.method_ = "PAUSE";
        request.uri_ = url;
        request.version_ = RTSP_VERSION;

        request.general_header_["CSeq"] = GenerateCSeq();
        request.general_header_["User-Agent"] = userAgent_;

        std::string request_str = request.ToString();
        LMRTSP_LOGD("Sending PAUSE request:\n%s", request_str.c_str());

        return SendRequest(request);
    } catch (const std::exception &e) {
        LMRTSP_LOGE("Exception in PAUSE: %s", e.what());
        return false;
    }
}

bool RtspClient::Teardown(const std::string &url)
{
    if (!connected_.load()) {
        LMRTSP_LOGE("Not connected to server");
        return false;
    }

    try {
        RtspRequest request;
        request.method_ = "TEARDOWN";
        request.uri_ = url;
        request.version_ = RTSP_VERSION;

        request.general_header_["CSeq"] = GenerateCSeq();
        request.general_header_["User-Agent"] = userAgent_;

        std::string request_str = request.ToString();
        LMRTSP_LOGD("Sending TEARDOWN request:\n%s", request_str.c_str());

        return SendRequest(request);
    } catch (const std::exception &e) {
        LMRTSP_LOGE("Exception in TEARDOWN: %s", e.what());
        return false;
    }
}

std::shared_ptr<RtspClientSession> RtspClient::CreateSession(const std::string &url)
{
    try {
        auto session = std::make_shared<RtspClientSession>(url, shared_from_this());
        if (session->Initialize()) {
            std::lock_guard<std::mutex> lock(sessionsMutex_);
            sessions_[session->GetSessionId()] = session;
            LMRTSP_LOGI("Created session: %s for URL: %s", session->GetSessionId().c_str(), url.c_str());
            return session;
        } else {
            LMRTSP_LOGE("Failed to initialize session for URL: %s", url.c_str());
            return nullptr;
        }
    } catch (const std::exception &e) {
        LMRTSP_LOGE("Exception creating session: %s", e.what());
        return nullptr;
    }
}

void RtspClient::RemoveSession(const std::string &session_id)
{
    std::lock_guard<std::mutex> lock(sessionsMutex_);
    auto it = sessions_.find(session_id);
    if (it != sessions_.end()) {
        it->second->Cleanup();
        sessions_.erase(it);
        LMRTSP_LOGI("Removed session: %s", session_id.c_str());
    }
}

std::shared_ptr<RtspClientSession> RtspClient::GetSession(const std::string &session_id)
{
    std::lock_guard<std::mutex> lock(sessionsMutex_);
    auto it = sessions_.find(session_id);
    return (it != sessions_.end()) ? it->second : nullptr;
}

void RtspClient::SetListener(std::shared_ptr<IRtspClientListener> listener)
{
    std::lock_guard<std::mutex> lock(listenerMutex_);
    listener_ = listener;
}

std::shared_ptr<IRtspClientListener> RtspClient::GetListener() const
{
    std::lock_guard<std::mutex> lock(listenerMutex_);
    return listener_;
}

void RtspClient::SetUserAgent(const std::string &user_agent)
{
    userAgent_ = user_agent;
}

std::string RtspClient::GetUserAgent() const
{
    return userAgent_;
}

void RtspClient::SetTimeout(int timeout_ms)
{
    timeoutMs_ = timeout_ms;
}

int RtspClient::GetTimeout() const
{
    return timeoutMs_;
}

std::string RtspClient::GetServerIP() const
{
    return serverIP_;
}

uint16_t RtspClient::GetServerPort() const
{
    return serverPort_;
}

size_t RtspClient::GetSessionCount() const
{
    std::lock_guard<std::mutex> lock(sessionsMutex_);
    return sessions_.size();
}

// Private methods
std::string RtspClient::GenerateCSeq()
{
    std::lock_guard<std::mutex> lock(requestMutex_);
    return std::to_string(cseq_++);
}

bool RtspClient::SendRequest(const RtspRequest &request)
{
    if (!connected_.load()) {
        LMRTSP_LOGE("Not connected to server");
        return false;
    }

    try {
        std::string request_str = request.ToString();
        auto buffer = lmcore::DataBuffer::Create(request_str.length());
        buffer->Assign(request_str.data(), request_str.length());

        if (!tcpClient_->Send(buffer)) {
            LMRTSP_LOGE("Failed to send request");
            return false;
        }

        return true;
    } catch (const std::exception &e) {
        LMRTSP_LOGE("Exception sending request: %s", e.what());
        return false;
    }
}

void RtspClient::HandleResponse(const RtspResponse &response)
{
    LMRTSP_LOGD("Handling RTSP response: %d %s", static_cast<int>(response.status_),
                GetReasonPhrase(response.status_).c_str());

    // Find the corresponding session based on CSeq
    auto cseq_it = response.general_header_.find("CSeq");
    if (cseq_it != response.general_header_.end()) {
        std::string cseq = cseq_it->second;

        // Handle based on method (we would need to track this in the request)
        // For now, try to find a session and route the response
        std::lock_guard<std::mutex> lock(sessionsMutex_);

        // Check if this is a DESCRIBE response
        auto session_it = sessions_.begin();
        if (session_it != sessions_.end()) {
            auto session = session_it->second;

            if (response.status_ == StatusCode::OK) {
                // Route response based on content
                auto content_type_it = response.general_header_.find("Content-Type");
                if (content_type_it != response.general_header_.end() && content_type_it->second == "application/sdp") {
                    // DESCRIBE response
                    if (response.messageBody_) {
                        session->HandleDescribeResponse(*response.messageBody_);
                    }
                } else {
                    // Other responses
                    auto session_id_it = response.general_header_.find("Session");
                    if (session_id_it != response.general_header_.end()) {
                        std::string session_id = session_id_it->second;
                        // Find session by ID and route response
                        for (auto &[id, sess] : sessions_) {
                            if (id == session_id) {
                                // Route response to appropriate handler
                                // This would need better method tracking
                                break;
                            }
                        }
                    }
                }
            } else {
                // Error response
                NotifyError(static_cast<int>(response.status_), GetReasonPhrase(response.status_));
            }
        }
    }
}

void RtspClient::ParseUrl(const std::string &url, std::string &host, uint16_t &port, std::string &path)
{
    auto parsed_url = lmcore::URL::Parse(url);
    if (!parsed_url || !parsed_url->IsRTSP()) {
        throw std::invalid_argument("Invalid RTSP URL: " + url);
    }

    host = parsed_url->Host();
    port = parsed_url->Port();
    path = parsed_url->Path();
    if (path.empty()) {
        path = "/";
    }
}

void RtspClient::NotifyError(int error_code, const std::string &error_message)
{
    NotifyListener([this, error_code, error_message](IRtspClientListener *listener) {
        listener->OnError(baseUrl_, error_code, error_message);
    });
}

void RtspClient::NotifyListener(std::function<void(IRtspClientListener *)> func)
{
    std::lock_guard<std::mutex> lock(listenerMutex_);
    if (listener_) {
        func(listener_.get());
    }
}

} // namespace lmshao::lmrtsp