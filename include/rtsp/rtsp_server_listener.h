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

// 观察者模式：RTSP服务器监听器
class RTSPServerListener : public network::IServerListener {
public:
    explicit RTSPServerListener(std::shared_ptr<RTSPServer> server);

    // 实现IServerListener接口
    void OnError(std::shared_ptr<network::Session> session, const std::string &errorInfo) override;
    void OnClose(std::shared_ptr<network::Session> session) override;
    void OnAccept(std::shared_ptr<network::Session> session) override;
    void OnReceive(std::shared_ptr<network::Session> session, std::shared_ptr<network::DataBuffer> buffer) override;

private:
    // 解析RTSP请求
    bool ParseRTSPRequest(const std::string &data, std::shared_ptr<network::Session> session);

    // 处理不完整的请求数据
    void HandleIncompleteData(std::shared_ptr<network::Session> session, const std::string &data);

    std::weak_ptr<RTSPServer> rtspServer_;

    // 存储不完整的请求数据
    std::unordered_map<lmshao::network::socket_t, std::string> incompleteRequests_;
};

} // namespace lmshao::rtsp

#endif // RTSP_SERVER_LISTENER_H