/**
 * @author SHAO Liming <lmshao@163.com>
 * @copyright Copyright (c) 2025 SHAO Liming
 * @license MIT
 *
 * SPDX-License-Identifier: MIT
 */

#ifndef LMSHAO_LMRTSP_RTSP_CLIENT_H
#define LMSHAO_LMRTSP_RTSP_CLIENT_H

#include <lmnet/iclient_listener.h>
#include <lmnet/tcp_client.h>

#include <atomic>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <unordered_map>

#include "lmrtsp/irtsp_client_callback.h"
#include "lmrtsp/media_stream_info.h"
#include "lmrtsp/rtsp_request.h"
#include "lmrtsp/rtsp_response.h"

namespace lmshao::lmrtsp {

class RtspClientSession;

/**
 * @brief RTSP Client class for connecting to RTSP servers
 *
 * This class manages connections to RTSP servers and handles the RTSP protocol.
 * It supports basic RTSP methods: OPTIONS, DESCRIBE, SETUP, PLAY, PAUSE, TEARDOWN.
 */
class RtspClient : public std::enable_shared_from_this<RtspClient> {
public:
    RtspClient();
    explicit RtspClient(std::shared_ptr<IRtspClientCallback> callback);
    ~RtspClient();

    // Connection management
    bool Connect(const std::string &url, int timeout_ms = 5000);
    bool Disconnect();
    bool IsConnected() const;

    // RTSP methods
    bool Options(const std::string &url);
    bool Describe(const std::string &url);
    bool Setup(const std::string &url, const std::string &transport);
    bool Play(const std::string &url);
    bool Pause(const std::string &url);
    bool Teardown(const std::string &url);

    // Session management
    std::shared_ptr<RtspClientSession> CreateSession(const std::string &url);
    void RemoveSession(const std::string &session_id);
    std::shared_ptr<RtspClientSession> GetSession(const std::string &session_id);

    // Callback management
    void SetCallback(std::shared_ptr<IRtspClientCallback> callback);
    std::shared_ptr<IRtspClientCallback> GetCallback() const;

    // Configuration
    void SetUserAgent(const std::string &user_agent);
    std::string GetUserAgent() const;

    void SetTimeout(int timeout_ms);
    int GetTimeout() const;

    // Statistics
    std::string GetServerIP() const;
    uint16_t GetServerPort() const;
    size_t GetSessionCount() const;

private:
    // Network connection
    std::shared_ptr<lmnet::TcpClient> tcpClient_;
    std::string serverIP_;
    uint16_t serverPort_;
    std::string baseUrl_;
    std::atomic<bool> connected_{false};

    // Session management
    mutable std::mutex sessionsMutex_;
    std::unordered_map<std::string, std::shared_ptr<RtspClientSession>> sessions_;

    // Callback interface
    mutable std::mutex callbackMutex_;
    std::shared_ptr<IRtspClientCallback> callback_;

    // Configuration
    std::string userAgent_ = "lmrtsp-client/1.0";
    int timeoutMs_ = 5000;

    // Request handling
    std::atomic<uint32_t> cseq_{1}; // CSeq counter
    mutable std::mutex requestMutex_;

    // Internal helper methods
    std::string GenerateCSeq();
    bool SendRequest(const RtspRequest &request);
    void HandleResponse(const RtspResponse &response);
    void ParseUrl(const std::string &url, std::string &host, uint16_t &port, std::string &path);

    // Error handling
    void NotifyError(int error_code, const std::string &error_message);
    void NotifyCallback(std::function<void(IRtspClientCallback *)> func);

    // TCP client listener
    class TcpClientListener;
    std::shared_ptr<TcpClientListener> tcpListener_;
};

} // namespace lmshao::lmrtsp

#endif // LMSHAO_LMRTSP_RTSP_CLIENT_H