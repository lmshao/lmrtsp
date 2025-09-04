/**
 * @author SHAO Liming <lmshao@163.com>
 * @copyright Copyright (c) 2025 SHAO Liming
 * @license MIT
 *
 * SPDX-License-Identifier: MIT
 */

#ifndef LMSHAO_RTSP_RTSP_SERVER_H
#define LMSHAO_RTSP_RTSP_SERVER_H

#include <coreutils/singleton.h>
#include <network/iserver_listener.h>
#include <network/tcp_server.h>

#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>

#include "irtsp_server_callback.h"
#include "media_stream_info.h"

// Forward declarations
namespace lmshao::rtsp {
class RTSPSession;
class RTSPRequest;
class RTSPServerListener;
} // namespace lmshao::rtsp

namespace lmshao::rtsp {
using namespace lmshao::coreutils;
class RTSPServer : public std::enable_shared_from_this<RTSPServer>, public Singleton<RTSPServer> {
public:
    friend class Singleton<RTSPServer>;
    friend class RTSPServerListener;

    ~RTSPServer() = default;

    // Basic server functionality
    bool Init(const std::string &ip, uint16_t port);
    bool Start();
    bool Stop();
    bool IsRunning() const;

    // Session management
    void HandleRequest(std::shared_ptr<RTSPSession> session, const RTSPRequest &request);
    void HandleStatelessRequest(std::shared_ptr<network::Session> networkSession, const RTSPRequest &request);
    void SendErrorResponse(std::shared_ptr<network::Session> networkSession, const RTSPRequest &request, int statusCode,
                           const std::string &reasonPhrase);
    std::shared_ptr<RTSPSession> CreateSession(std::shared_ptr<network::Session> networkSession);
    void RemoveSession(const std::string &sessionId);
    std::shared_ptr<RTSPSession> GetSession(const std::string &sessionId);
    std::unordered_map<std::string, std::shared_ptr<RTSPSession>> GetSessions();

    // Callback interface
    void SetCallback(std::shared_ptr<IRTSPServerCallback> callback);
    std::shared_ptr<IRTSPServerCallback> GetCallback() const;

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
    RTSPServer();

private:
    // Network related
    std::shared_ptr<RTSPServerListener> serverListener_;
    std::shared_ptr<network::TcpServer> tcpServer_;
    std::string serverIP_;
    uint16_t serverPort_;
    std::atomic<bool> running_{false};

    // Session management
    mutable std::mutex sessionsMutex_;
    std::unordered_map<std::string, std::shared_ptr<RTSPSession>> sessions_;

    // Callback interface
    mutable std::mutex callbackMutex_;
    std::shared_ptr<IRTSPServerCallback> callback_;

    // Media stream management
    mutable std::mutex streamsMutex_;
    std::map<std::string, std::shared_ptr<MediaStreamInfo>> mediaStreams_;

    // Internal helper methods
    std::string GetClientIP(std::shared_ptr<RTSPSession> session) const;
    void NotifyCallback(std::function<void(IRTSPServerCallback *)> func);
};

} // namespace lmshao::rtsp

#endif // LMSHAO_RTSP_RTSP_SERVER_H