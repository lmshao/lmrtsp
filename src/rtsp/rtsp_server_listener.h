/**
 * @author SHAO Liming <lmshao@163.com>
 * @copyright Copyright (c) 2025 SHAO Liming
 * @license MIT
 *
 * SPDX-License-Identifier: MIT
 */

#ifndef LMSHAO_LMRTSP_RTSP_SERVER_LISTENER_H
#define LMSHAO_LMRTSP_RTSP_SERVER_LISTENER_H

#include <lmnet/common.h>
#include <lmnet/iserver_listener.h>

#include <memory>
#include <string>
#include <unordered_map>

namespace lmshao::lmrtsp {

class RtspServer;

// Observer pattern: RTSP server listener
class RtspServerListener : public lmnet::IServerListener {
public:
    explicit RtspServerListener(std::shared_ptr<RtspServer> server);

    // Implement IServerListener interface
    void OnError(std::shared_ptr<lmnet::Session> session, const std::string &errorInfo) override;
    void OnClose(std::shared_ptr<lmnet::Session> session) override;
    void OnAccept(std::shared_ptr<lmnet::Session> session) override;
    void OnReceive(std::shared_ptr<lmnet::Session> session, std::shared_ptr<lmcore::DataBuffer> buffer) override;

private:
    // Parse RTSP request
    bool ParseRTSPRequest(const std::string &data, std::shared_ptr<lmnet::Session> session);

    // Handle incomplete request data
    void HandleIncompleteData(std::shared_ptr<lmnet::Session> session, const std::string &data);

    // Handle TCP interleaved data (RTP/RTCP over RTSP)
    void HandleInterleavedData(std::shared_ptr<lmnet::Session> session, const std::string &data);

    std::weak_ptr<RtspServer> rtspServer_;

    // Store incomplete request data
    std::unordered_map<lmnet::socket_t, std::string> incompleteRequests_;
};

} // namespace lmshao::lmrtsp

#endif // LMSHAO_LMRTSP_RTSP_SERVER_LISTENER_H