/**
 * @author SHAO Liming <lmshao@163.com>
 * @copyright Copyright (c) 2025 SHAO Liming
 * @license MIT
 *
 * SPDX-License-Identifier: MIT
 */

#ifndef LMSHAO_LMRTSP_RTSP_SERVER_H
#define LMSHAO_LMRTSP_RTSP_SERVER_H

#include <lmcore/singleton.h>
#include <lmnet/iserver_listener.h>
#include <lmnet/tcp_server.h>

#include <atomic>
#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>

#include "lmrtsp/irtsp_server_callback.h"
#include "lmrtsp/media_stream_info.h"

namespace lmshao::lmrtsp {
using namespace lmshao::lmcore;
class RtspSession;
class RtspRequest;
class RtspServerListener;
class RtspServer : public std::enable_shared_from_this<RtspServer>, public ManagedSingleton<RtspServer> {
public:
    friend class ManagedSingleton<RtspServer>;
    friend class RtspServerListener;

    ~RtspServer() = default;

    // Basic server functionality
    bool Init(const std::string &ip, uint16_t port);
    bool Start();
    bool Stop();
    bool IsRunning() const;

    // Session management
    void HandleRequest(std::shared_ptr<RtspSession> session, const RtspRequest &request);
    void HandleStatelessRequest(std::shared_ptr<lmnet::Session> lmnetSession, const RtspRequest &request);
    void SendErrorResponse(std::shared_ptr<lmnet::Session> lmnetSession, const RtspRequest &request, int statusCode,
                           const std::string &reasonPhrase);
    std::shared_ptr<RtspSession> CreateSession(std::shared_ptr<lmnet::Session> lmnetSession);
    void RemoveSession(const std::string &sessionId);
    std::shared_ptr<RtspSession> GetSession(const std::string &sessionId);
    std::unordered_map<std::string, std::shared_ptr<RtspSession>> GetSessions();

    // Callback interface
    void SetCallback(std::shared_ptr<IRtspServerCallback> callback);
    std::shared_ptr<IRtspServerCallback> GetCallback() const;

    // Media stream management
    bool AddMediaStream(const std::string &stream_path, std::shared_ptr<MediaStreamInfo> stream_info);
    bool RemoveMediaStream(const std::string &stream_path);
    std::shared_ptr<MediaStreamInfo> GetMediaStream(const std::string &stream_path);
    std::vector<std::string> GetMediaStreamPaths() const;

    // Client management
    std::vector<std::string> GetConnectedClients() const;
    bool DisconnectClient(const std::string &client_ip);
    size_t GetClientCount() const;

    // SDP generation
    std::string GenerateSDP(const std::string &stream_path, const std::string &server_ip, uint16_t server_port);

    // Server information
    std::string GetServerIP() const;
    uint16_t GetServerPort() const;

protected:
    RtspServer();

private:
    // Network related
    std::shared_ptr<RtspServerListener> serverListener_;
    std::shared_ptr<lmnet::TcpServer> tcpServer_;
    std::string serverIP_;
    uint16_t serverPort_;
    std::atomic<bool> running_{false};

    // Session management
    mutable std::mutex sessionsMutex_;
    std::unordered_map<std::string, std::shared_ptr<RtspSession>> sessions_;

    // Callback interface
    mutable std::mutex callbackMutex_;
    std::shared_ptr<IRtspServerCallback> callback_;

    // Media stream management
    mutable std::mutex streamsMutex_;
    std::map<std::string, std::shared_ptr<MediaStreamInfo>> mediaStreams_;

    // Internal helper methods
    std::string GetClientIP(std::shared_ptr<RtspSession> session) const;
    void NotifyCallback(std::function<void(IRtspServerCallback *)> func);
};

} // namespace lmshao::lmrtsp

#endif // LMSHAO_LMRTSP_RTSP_SERVER_H