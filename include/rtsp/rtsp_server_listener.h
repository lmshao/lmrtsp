/**
 * @author SHAO Liming <lmshao@163.com>
 * @copyright Copyright (c) 2025 SHAO Liming
 * @license MIT
 *
 * SPDX-License-Identifier: MIT
 */

#ifndef RTSP_SERVER_LISTENER_H
#define RTSP_SERVER_LISTENER_H

#include <network/common.h>
#include <network/iserver_listener.h>

#include <memory>
#include <string>
#include <unordered_map>

namespace lmshao::rtsp {

class RTSPServer;

// Observer pattern: RTSP server listener
class RTSPServerListener : public network::IServerListener {
public:
    explicit RTSPServerListener(std::shared_ptr<RTSPServer> server);

    // Implement IServerListener interface
    void OnError(std::shared_ptr<network::Session> session, const std::string &errorInfo) override;
    void OnClose(std::shared_ptr<network::Session> session) override;
    void OnAccept(std::shared_ptr<network::Session> session) override;
    void OnReceive(std::shared_ptr<network::Session> session, std::shared_ptr<coreutils::DataBuffer> buffer) override;

private:
    // Parse RTSP request
    bool ParseRTSPRequest(const std::string &data, std::shared_ptr<network::Session> session);

    // Handle incomplete request data
    void HandleIncompleteData(std::shared_ptr<network::Session> session, const std::string &data);

    std::weak_ptr<RTSPServer> rtspServer_;

    // Store incomplete request data
    std::unordered_map<network::socket_t, std::string> incompleteRequests_;
};

} // namespace lmshao::rtsp

#endif // RTSP_SERVER_LISTENER_H