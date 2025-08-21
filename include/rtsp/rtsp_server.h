/**
 * @author SHAO Liming <lmshao@163.com>
 * @copyright Copyright (c) 2025 SHAO Liming
 * @license MIT
 *
 * SPDX-License-Identifier: MIT
 */

#ifndef RTSP_SERVER_H
#define RTSP_SERVER_H

#include <coreutils/singleton.h>
#include <network/iserver_listener.h>
#include <network/tcp_server.h>

#include <memory>
#include <string>
#include <unordered_map>

#include "rtsp_request.h"
#include "rtsp_server_listener.h"
#include "rtsp_session.h"

namespace lmshao::rtsp {
using namespace lmshao::coreutils;
class RTSPServer : public std::enable_shared_from_this<RTSPServer>, public Singleton<RTSPServer> {
public:
    friend class Singleton<RTSPServer>;

    ~RTSPServer() = default;
    bool Init(const std::string &ip, uint16_t port);
    bool Start();
    bool Stop();

    void HandleRequest(std::shared_ptr<RTSPSession> session, const RTSPRequest &request);
    std::shared_ptr<RTSPSession> CreateSession(std::shared_ptr<network::Session> networkSession);
    void RemoveSession(const std::string &sessionId);
    std::shared_ptr<RTSPSession> GetSession(const std::string &sessionId);

protected:
    RTSPServer();

private:
    std::shared_ptr<RTSPServerListener> serverListener_;
    std::shared_ptr<network::TcpServer> tcpServer_;
    std::unordered_map<std::string, std::shared_ptr<RTSPSession>> sessions_;
};

} // namespace lmshao::rtsp

#endif // RTSP_SERVER_H