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
#include <unordered_map>

#include "lmrtsp/irtsp_client_listener.h"
#include "lmrtsp/rtsp_request.h"
#include "lmrtsp/rtsp_response.h"

namespace lmshao::lmrtsp {

class RtspClientSession;

/**
 * @brief RTSP Client class for connecting to RTSP servers
 *
 * This class provides a high-level interface for RTSP streaming, similar to RtspServer.
 * It automatically handles all RTSP protocol details internally.
 */
// Forward declaration for state machine
class RtspClientStateMachine;

class RtspClient : public std::enable_shared_from_this<RtspClient> {
public:
    RtspClient();
    explicit RtspClient(std::shared_ptr<IRtspClientListener> listener);
    ~RtspClient();

    // High-level interface (symmetric with RtspServer)
    bool Init(const std::string &url); // Initialize with RTSP URL
    bool Start();                      // Start streaming (auto-completes RTSP handshake)
    bool Stop();                       // Stop streaming (auto-completes cleanup)
    bool IsPlaying() const;            // Check if currently playing

    // Listener management
    void SetListener(std::shared_ptr<IRtspClientListener> listener);
    std::shared_ptr<IRtspClientListener> GetListener() const;

    // Configuration
    void SetUserAgent(const std::string &user_agent);
    std::string GetUserAgent() const;

    void SetTimeout(int timeout_ms);
    int GetTimeout() const;

    // Statistics
    std::string GetServerIP() const;
    uint16_t GetServerPort() const;
    std::string GetUrl() const; // Get initialized URL

private:
    // Forward declaration for private nested class
    class TcpClientListener;

    // RTSP protocol methods (internal use only)
    bool Connect(const std::string &url, int timeout_ms = 5000);
    bool Disconnect();
    bool IsConnected() const;
    bool SendOptionsRequest(const std::string &url);
    bool SendDescribeRequest(const std::string &url);
    bool SendSetupRequest(const std::string &url, const std::string &transport);
    bool SendPlayRequest(const std::string &url, const std::string &session_id = "");
    bool SendPauseRequest(const std::string &url, const std::string &session_id = "");
    bool SendTeardownRequest(const std::string &url, const std::string &session_id = "");

    // Session management (internal use only)
    std::shared_ptr<RtspClientSession> CreateSession(const std::string &url);
    void RemoveSession(const std::string &session_id);
    std::shared_ptr<RtspClientSession> GetSession(const std::string &session_id);
    size_t GetSessionCount() const;

    // Network connection
    std::shared_ptr<lmnet::TcpClient> tcpClient_;
    std::shared_ptr<TcpClientListener> tcpListener_; // Keep listener alive
    std::string serverIP_;
    uint16_t serverPort_;
    std::string baseUrl_;
    std::string rtspUrl_; // Initialized URL
    std::atomic<bool> connected_{false};
    std::atomic<bool> playing_{false};            // Playing state
    std::atomic<bool> handshake_complete_{false}; // Handshake completion flag
    std::atomic<bool> handshake_failed_{false};   // Handshake failure flag

    // Session management
    mutable std::mutex sessionsMutex_;
    std::unordered_map<std::string, std::shared_ptr<RtspClientSession>> sessions_;
    std::shared_ptr<RtspClientSession> currentSession_; // Current active session

    // Listener interface
    mutable std::mutex listenerMutex_;
    std::shared_ptr<IRtspClientListener> listener_;

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
    bool PerformRTSPHandshake(); // Internal RTSP handshake

    // Error handling
    void NotifyError(int error_code, const std::string &error_message);
    void NotifyListener(std::function<void(IRtspClientListener *)> func);

    // Friend classes for state machine to access private methods
    friend class RtspClientStateMachine;
    friend class ClientInitState;
    friend class ClientOptionsSentState;
    friend class ClientDescribeSentState;
    friend class ClientSetupSentState;
    friend class ClientPlaySentState;
    friend class ClientPlayingState;
};

} // namespace lmshao::lmrtsp

#endif // LMSHAO_LMRTSP_RTSP_CLIENT_H