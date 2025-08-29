/**
 * @author SHAO Liming <lmshao@163.com>
 * @copyright Copyright (c) 2025 SHAO Liming
 * @license MIT
 *
 * SPDX-License-Identifier: MIT
 */

#include <iostream>
#include <string>
#include <thread>
#include <vector>

#include <network/tcp_client.h>
#include "rtsp/rtsp_request.h"
#include "rtsp/rtsp_response.h"

using namespace lmshao;

class RTSPClient : public network::IClientListener, public std::enable_shared_from_this<RTSPClient> {
public:
    void Start(const std::string& ip, uint16_t port, const std::string& stream_url) {
        ip_ = ip;
        port_ = port;
        stream_url_ = stream_url;

        tcp_client_ = network::TcpClient::Create(ip, port);
        tcp_client_->SetListener(shared_from_this());
        tcp_client_->Init();
        tcp_client_->Connect();
        
        // Start with OPTIONS request
        SendOptions();
    }

    void OnReceive(network::socket_t fd, std::shared_ptr<coreutils::DataBuffer> buffer) override {
        std::string response_str(reinterpret_cast<const char*>(buffer->Data()), buffer->Size());
        auto response = rtsp::RTSPResponse::FromString(response_str);

        std::cout << "Received response:\n" << response.ToString() << std::endl;

        if (response.status_ == rtsp::StatusCode::OK) {
            if (cseq_ == 1) { // OPTIONS response
                SendDescribe();
            } else if (cseq_ == 2) { // DESCRIBE response
                SendSetup();
            } else if (cseq_ == 3) { // SETUP response
                session_id_ = response.general_header_.at("Session");
                SendPlay();
            } else if (cseq_ == 4) { // PLAY response
                std::this_thread::sleep_for(std::chrono::seconds(5));
                SendTeardown();
            } else if (cseq_ == 5) { // TEARDOWN response
                tcp_client_->Close();
            }
        }
    }

    void OnClose(network::socket_t fd) override {
        std::cout << "Disconnected from server" << std::endl;
    }

    void OnError(network::socket_t fd, const std::string& error) override {
        std::cerr << "Error: " << error << std::endl;
    }

private:
    void SendOptions() {
        cseq_ = 1;
        auto req = rtsp::RTSPRequestFactory::CreateOptions(cseq_, stream_url_).Build();
        std::cout << "Sending request:\n" << req.ToString() << std::endl;
        tcp_client_->Send(req.ToString().c_str(), req.ToString().length());
    }

    void SendDescribe() {
        cseq_ = 2;
        auto req = rtsp::RTSPRequestFactory::CreateDescribe(cseq_, stream_url_).Build();
        std::cout << "Sending request:\n" << req.ToString() << std::endl;
        tcp_client_->Send(req.ToString().c_str(), req.ToString().length());
    }

    void SendSetup() {
        cseq_ = 3;
        auto req = rtsp::RTSPRequestFactory::CreateSetup(cseq_, stream_url_ + "/track1")
                       .SetTransport("RTP/AVP;unicast;client_port=1234-1235")
                       .Build();
        std::cout << "Sending request:\n" << req.ToString() << std::endl;
        tcp_client_->Send(req.ToString().c_str(), req.ToString().length());
    }

    void SendPlay() {
        cseq_ = 4;
        auto req = rtsp::RTSPRequestFactory::CreatePlay(cseq_, stream_url_).SetSession(session_id_).Build();
        std::cout << "Sending request:\n" << req.ToString() << std::endl;
        tcp_client_->Send(req.ToString().c_str(), req.ToString().length());
    }

    void SendTeardown() {
        cseq_ = 5;
        auto req = rtsp::RTSPRequestFactory::CreateTeardown(cseq_, stream_url_).SetSession(session_id_).Build();
        std::cout << "Sending request:\n" << req.ToString() << std::endl;
        tcp_client_->Send(req.ToString().c_str(), req.ToString().length());
    }

    std::string ip_;
    uint16_t port_;
    std::string stream_url_;
    std::shared_ptr<network::TcpClient> tcp_client_;
    int cseq_ = 0;
    std::string session_id_;
};

int main(int argc, char* argv[]) {
    if (argc != 4) {
        std::cerr << "Usage: " << argv[0] << " <server_ip> <server_port> <stream_url>" << std::endl;
        return 1;
    }

    std::string server_ip = argv[1];
    uint16_t server_port = std::stoi(argv[2]);
    std::string stream_url = argv[3];

    auto client = std::make_shared<RTSPClient>();
    client->Start(server_ip, server_port, stream_url);

    while (true) {
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }

    return 0;
}
