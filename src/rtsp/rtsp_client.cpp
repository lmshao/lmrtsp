/**
 * @author SHAO Liming <lmshao@163.com>
 * @copyright Copyright (c) 2025 SHAO Liming
 * @license MIT
 *
 * SPDX-License-Identifier: MIT
 */

#include "lmrtsp/rtsp_client.h"

#include <lmcore/url.h>

#include <chrono>
#include <thread>

#include "internal_logger.h"
#include "lmnet/iclient_listener.h"
#include "lmnet/tcp_client.h"
#include "lmrtsp/rtsp_client_session.h"
#include "lmrtsp/rtsp_headers.h"
#include "lmrtsp/rtsp_request.h"
#include "lmrtsp/rtsp_response.h"
#include "rtsp_client_session_state.h"

namespace lmshao::lmrtsp {

// TCP Client Listener implementation
class RtspClient::TcpClientListener : public lmnet::IClientListener {
public:
    explicit TcpClientListener(std::weak_ptr<RtspClient> client) : client_(client) {}

    void OnReceive(lmnet::socket_t fd, std::shared_ptr<lmnet::DataBuffer> buffer) override
    {
        LMRTSP_LOGI("TcpClientListener::OnReceive called, fd=%d, size=%zu", fd, buffer ? buffer->Size() : 0);
        if (auto client = client_.lock()) {
            if (!buffer || buffer->Size() == 0) {
                LMRTSP_LOGW("Received empty buffer");
                return;
            }
            std::string response_str(reinterpret_cast<const char *>(buffer->Data()), buffer->Size());
            LMRTSP_LOGI("Received RTSP response (%zu bytes):\n%.200s%s", response_str.size(), response_str.c_str(),
                        response_str.size() > 200 ? "..." : "");

            try {
                RtspResponse response = RtspResponse::FromString(response_str);
                client->HandleResponse(response);
            } catch (const std::exception &e) {
                LMRTSP_LOGE("Failed to parse RTSP response: %s", e.what());
                LMRTSP_LOGE("Response content:\n%s", response_str.c_str());
                client->NotifyError(-1, std::string("Failed to parse response: ") + e.what());
            }
        } else {
            LMRTSP_LOGE("Failed to lock client in OnReceive");
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

        // Create and keep the listener alive
        tcpListener_ = std::make_shared<TcpClientListener>(shared_from_this());
        tcpClient_->SetListener(tcpListener_);

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

bool RtspClient::SendOptionsRequest(const std::string &url)
{
    if (!connected_.load()) {
        LMRTSP_LOGE("Not connected to server");
        return false;
    }

    try {
        RtspRequest request;
        request.method_ = METHOD_OPTIONS;
        request.uri_ = url;
        request.version_ = RTSP_VERSION;

        request.general_header_[CSEQ] = GenerateCSeq();
        request.general_header_[USER_AGENT] = userAgent_;

        std::string request_str = request.ToString();
        LMRTSP_LOGD("Sending OPTIONS request");

        return SendRequest(request);
    } catch (const std::exception &e) {
        LMRTSP_LOGE("Exception in OPTIONS: %s", e.what());
        return false;
    }
}

bool RtspClient::SendDescribeRequest(const std::string &url)
{
    if (!connected_.load()) {
        LMRTSP_LOGE("Not connected to server");
        return false;
    }

    try {
        RtspRequest request;
        request.method_ = METHOD_DESCRIBE;
        request.uri_ = url;
        request.version_ = RTSP_VERSION;

        request.general_header_[CSEQ] = GenerateCSeq();
        request.general_header_[USER_AGENT] = userAgent_;
        request.general_header_["Accept"] = "application/sdp";

        // For on-demand content, request to start from the beginning
        // Some servers (like Live555) may use this to initialize the session position
        request.general_header_[RANGE] = "npt=0-";

        std::string request_str = request.ToString();
        LMRTSP_LOGD("Sending DESCRIBE request");

        return SendRequest(request);
    } catch (const std::exception &e) {
        LMRTSP_LOGE("Exception in DESCRIBE: %s", e.what());
        return false;
    }
}

bool RtspClient::SendSetupRequest(const std::string &url, const std::string &transport)
{
    if (!connected_.load()) {
        LMRTSP_LOGE("Not connected to server");
        return false;
    }

    try {
        RtspRequest request;
        request.method_ = METHOD_SETUP;
        request.uri_ = url;
        request.version_ = RTSP_VERSION;

        request.general_header_[CSEQ] = GenerateCSeq();
        request.general_header_[USER_AGENT] = userAgent_;
        request.general_header_[TRANSPORT] = transport;

        std::string request_str = request.ToString();
        LMRTSP_LOGD("Sending SETUP request:\n%s", request_str.c_str());

        return SendRequest(request);
    } catch (const std::exception &e) {
        LMRTSP_LOGE("Exception in SETUP: %s", e.what());
        return false;
    }
}

bool RtspClient::SendPlayRequest(const std::string &url, const std::string &session_id)
{
    if (!connected_.load()) {
        LMRTSP_LOGE("Not connected to server");
        return false;
    }

    try {
        RtspRequest request;
        request.method_ = METHOD_PLAY;
        request.uri_ = url;
        request.version_ = RTSP_VERSION;

        request.general_header_[CSEQ] = GenerateCSeq();
        request.general_header_[USER_AGENT] = userAgent_;

        // Add Session header if provided
        if (!session_id.empty()) {
            request.general_header_[SESSION] = session_id;
        }

        // Request playback from the beginning
        request.general_header_[RANGE] = "npt=0.000-";

        std::string request_str = request.ToString();
        LMRTSP_LOGD("Sending PLAY request:\n%s", request_str.c_str());

        return SendRequest(request);
    } catch (const std::exception &e) {
        LMRTSP_LOGE("Exception in PLAY: %s", e.what());
        return false;
    }
}

bool RtspClient::SendPauseRequest(const std::string &url, const std::string &session_id)
{
    if (!connected_.load()) {
        LMRTSP_LOGE("Not connected to server");
        return false;
    }

    try {
        RtspRequest request;
        request.method_ = METHOD_PAUSE;
        request.uri_ = url;
        request.version_ = RTSP_VERSION;

        request.general_header_[CSEQ] = GenerateCSeq();
        request.general_header_[USER_AGENT] = userAgent_;

        // Add Session header if provided
        if (!session_id.empty()) {
            request.general_header_[SESSION] = session_id;
        }

        std::string request_str = request.ToString();
        LMRTSP_LOGD("Sending PAUSE request:\n%s", request_str.c_str());

        return SendRequest(request);
    } catch (const std::exception &e) {
        LMRTSP_LOGE("Exception in PAUSE: %s", e.what());
        return false;
    }
}

bool RtspClient::SendTeardownRequest(const std::string &url, const std::string &session_id)
{
    if (!connected_.load()) {
        LMRTSP_LOGE("Not connected to server");
        return false;
    }

    try {
        RtspRequest request;
        request.method_ = METHOD_TEARDOWN;
        request.uri_ = url;
        request.version_ = RTSP_VERSION;

        request.general_header_[CSEQ] = GenerateCSeq();
        request.general_header_[USER_AGENT] = userAgent_;

        // Add Session header if provided
        if (!session_id.empty()) {
            request.general_header_[SESSION] = session_id;
        }

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

std::string RtspClient::GetUrl() const
{
    return rtspUrl_;
}

size_t RtspClient::GetSessionCount() const
{
    std::lock_guard<std::mutex> lock(sessionsMutex_);
    return sessions_.size();
}

// High-level interface implementation
bool RtspClient::Init(const std::string &url)
{
    try {
        // Parse and validate URL
        std::string host;
        uint16_t port;
        std::string path;
        ParseUrl(url, host, port, path);

        // Save URL for later use
        rtspUrl_ = url;
        serverIP_ = host;
        serverPort_ = port;
        baseUrl_ = url;

        LMRTSP_LOGI("RTSP Client initialized with URL: %s", url.c_str());
        return true;
    } catch (const std::exception &e) {
        LMRTSP_LOGE("Failed to initialize RTSP client: %s", e.what());
        return false;
    }
}

bool RtspClient::Start()
{
    if (rtspUrl_.empty()) {
        LMRTSP_LOGE("RTSP URL not initialized. Call Init() first.");
        return false;
    }

    if (playing_.load()) {
        LMRTSP_LOGW("Already playing. Stop first before starting again.");
        return false;
    }

    LMRTSP_LOGI("Starting RTSP stream: %s", rtspUrl_.c_str());

    // Step 1: Connect to server
    if (!Connect(rtspUrl_, timeoutMs_)) {
        LMRTSP_LOGE("Failed to connect to RTSP server");
        return false;
    }

    // Step 2: Create session (must be done before DESCRIBE so HandleResponse can find it)
    currentSession_ = CreateSession(rtspUrl_);
    if (!currentSession_) {
        LMRTSP_LOGE("Failed to create RTSP session");
        Disconnect();
        return false;
    }

    // Step 3: Perform RTSP handshake (OPTIONS -> DESCRIBE -> SETUP -> PLAY)
    // State machine will handle the handshake automatically
    if (!PerformRTSPHandshake()) {
        LMRTSP_LOGE("RTSP handshake failed");
        if (currentSession_) {
            RemoveSession(currentSession_->GetSessionId());
            currentSession_.reset();
        }
        Disconnect();
        return false;
    }

    // playing_ is set by state machine when handshake completes
    LMRTSP_LOGI("RTSP stream started successfully");
    return true;
}

bool RtspClient::Stop()
{
    if (!playing_.load()) {
        LMRTSP_LOGW("Not playing. Nothing to stop.");
        return true;
    }

    LMRTSP_LOGI("Stopping RTSP stream");

    // Step 1: Send TEARDOWN
    if (currentSession_ && connected_.load()) {
        std::string session_id = currentSession_->GetSessionId();
        SendTeardownRequest(rtspUrl_, session_id);
    }

    // Step 2: Remove session
    if (currentSession_) {
        RemoveSession(currentSession_->GetSessionId());
        currentSession_.reset();
    }

    // Step 3: Disconnect
    Disconnect();

    playing_.store(false);
    LMRTSP_LOGI("RTSP stream stopped");
    return true;
}

bool RtspClient::IsPlaying() const
{
    return playing_.load();
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
        LMRTSP_LOGI("Sending RTSP request:\n%.500s%s", request_str.c_str(), request_str.size() > 500 ? "..." : "");

        auto buffer = lmcore::DataBuffer::Create(request_str.length());
        buffer->Assign(request_str.data(), request_str.length());

        if (!tcpClient_->Send(buffer)) {
            LMRTSP_LOGE("Failed to send request");
            return false;
        }

        LMRTSP_LOGD("Request sent successfully (%zu bytes)", request_str.length());
        return true;
    } catch (const std::exception &e) {
        LMRTSP_LOGE("Exception sending request: %s", e.what());
        return false;
    }
}

void RtspClient::HandleResponse(const RtspResponse &response)
{
    LMRTSP_LOGI("Handling RTSP response: %d %s", static_cast<int>(response.status_),
                GetReasonPhrase(response.status_).c_str());

    if (response.status_ != StatusCode::OK) {
        // Error response
        LMRTSP_LOGE("RTSP response error: %d %s", static_cast<int>(response.status_),
                    GetReasonPhrase(response.status_).c_str());
        NotifyError(static_cast<int>(response.status_), GetReasonPhrase(response.status_));
        return;
    }

    LMRTSP_LOGI("Status is OK, proceeding to find session");
    std::lock_guard<std::mutex> lock(sessionsMutex_);

    // Find session - try by Session ID first, then use current session
    std::shared_ptr<RtspClientSession> session = nullptr;
    auto session_id_it = response.general_header_.find(SESSION);
    if (session_id_it != response.general_header_.end()) {
        std::string session_id = session_id_it->second;
        LMRTSP_LOGI("Response has Session ID: %s", session_id.c_str());
        auto it = sessions_.find(session_id);
        if (it != sessions_.end()) {
            session = it->second;
        }
    } else {
        LMRTSP_LOGI("Response has no Session header");
    }

    // If no session found by ID, use current session
    if (!session && currentSession_) {
        LMRTSP_LOGI("Using current session: %s", currentSession_->GetSessionId().c_str());
        session = currentSession_;
    }

    // If still no session, try to get first available session
    if (!session && !sessions_.empty()) {
        LMRTSP_LOGI("Using first available session from sessions_ map");
        session = sessions_.begin()->second;
    }

    if (!session) {
        LMRTSP_LOGE("No session available to handle response");
        NotifyError(-1, "No session available");
        return;
    }

    LMRTSP_LOGI("Session found: %s, getting state machine", session->GetSessionId().c_str());

    // Get current state machine
    auto state = session->GetCurrentState();
    if (!state) {
        LMRTSP_LOGE("Session has no state machine");
        NotifyError(-1, "Session has no state machine");
        return;
    }

    LMRTSP_LOGI("State machine found: %s", state->GetName().c_str());

    // Debug: print all headers
    LMRTSP_LOGI("Response headers count: %zu", response.general_header_.size());
    for (const auto &kv : response.general_header_) {
        LMRTSP_LOGI("  General Header: '%s' = '%s'", kv.first.c_str(), kv.second.c_str());
    }
    LMRTSP_LOGI("Response entity headers count: %zu", response.entity_header_.size());
    for (const auto &kv : response.entity_header_) {
        LMRTSP_LOGI("  Entity Header: '%s' = '%s'", kv.first.c_str(), kv.second.c_str());
    }

    // Route response to state machine based on content/headers
    ClientStateAction action = ClientStateAction::WAIT;

    // Check Content-Type in entity_header (not general_header)
    auto content_type_it = response.entity_header_.find(CONTENT_TYPE);
    if (content_type_it != response.entity_header_.end() && content_type_it->second == MIME_SDP) {
        // DESCRIBE response
        LMRTSP_LOGI("Identified as DESCRIBE response");
        if (response.messageBody_) {
            session->HandleDescribeResponse(*response.messageBody_);
        }
        action = state->OnDescribeResponse(session.get(), this, response);
    } else if (!response.responseHeader_.publicMethods_.empty()) {
        // OPTIONS response (has Public header with methods list)
        LMRTSP_LOGI("Identified as OPTIONS response, Public methods count: %zu",
                    response.responseHeader_.publicMethods_.size());
        action = state->OnOptionsResponse(session.get(), this, response);
    } else {
        auto transport_it = response.general_header_.find(TRANSPORT);
        if (transport_it != response.general_header_.end()) {
            // SETUP response
            LMRTSP_LOGI("Identified as SETUP response, Transport: %s", transport_it->second.c_str());
            std::string session_id = session_id_it != response.general_header_.end() ? session_id_it->second : "";
            session->HandleSetupResponse(session_id, transport_it->second);
            action = state->OnSetupResponse(session.get(), this, response);
        } else {
            auto rtp_info_it = response.general_header_.find(RTP_INFO);
            std::string rtp_info = (rtp_info_it != response.general_header_.end()) ? rtp_info_it->second : "";
            if (!rtp_info.empty() || session->GetState() == ClientSessionStateEnum::READY) {
                // PLAY response
                LMRTSP_LOGI("Identified as PLAY response, RTP-Info: %s", rtp_info.c_str());
                session->HandlePlayResponse(rtp_info);
                action = state->OnPlayResponse(session.get(), this, response);
            } else {
                // Unknown response type
                LMRTSP_LOGD("Unknown response type, might be OPTIONS or other stateless response");
            }
        }
    }

    // Handle state machine action
    switch (action) {
        case ClientStateAction::CONTINUE:
            // State machine will send next request automatically
            LMRTSP_LOGD("State machine continuing to next step");
            break;
        case ClientStateAction::SUCCESS:
            // Handshake completed
            LMRTSP_LOGI("RTSP handshake completed successfully");
            handshake_complete_.store(true);
            playing_.store(true);
            break;
        case ClientStateAction::FAIL:
            LMRTSP_LOGE("State machine reported failure");
            handshake_failed_.store(true);
            NotifyError(-1, "RTSP handshake failed");
            break;
        case ClientStateAction::WAIT:
            // Continue waiting
            break;
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

bool RtspClient::PerformRTSPHandshake()
{
    if (!currentSession_) {
        LMRTSP_LOGE("No session available for RTSP handshake");
        return false;
    }

    // Reset handshake flags
    handshake_complete_.store(false);
    handshake_failed_.store(false);

    // Initialize state machine to Init state
    currentSession_->ChangeState(&ClientInitialState::GetInstance());

    // Step 0: Send OPTIONS request (state machine will handle the rest)
    LMRTSP_LOGD("Sending OPTIONS request to start handshake");
    if (!SendOptionsRequest(rtspUrl_)) {
        LMRTSP_LOGE("Failed to send OPTIONS request");
        return false;
    }

    // Wait for handshake to complete (state machine will drive the process)
    // State machine will automatically send DESCRIBE -> SETUP -> PLAY
    int wait_count = 0;
    const int max_wait = 50; // 5 seconds (50 * 100ms)
    while (wait_count < max_wait && !handshake_complete_.load() && !handshake_failed_.load()) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        wait_count++;
    }

    if (handshake_failed_.load()) {
        LMRTSP_LOGE("RTSP handshake failed");
        return false;
    }

    if (!handshake_complete_.load()) {
        LMRTSP_LOGE("RTSP handshake timeout");
        return false;
    }

    LMRTSP_LOGI("RTSP handshake completed successfully");
    return true;
}

} // namespace lmshao::lmrtsp