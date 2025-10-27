/**
 * @author SHAO Liming <lmshao@163.com>
 * @copyright Copyright (c) 2025 SHAO Liming
 * @license MIT
 *
 * SPDX-License-Identifier: MIT
 */

#include <lmnet/tcp_client.h>

#include <chrono>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <iostream>
#include <memory>
#include <string>
#include <thread>
#include <vector>

#include "lmrtsp/rtsp_request.h"
#include "lmrtsp/rtsp_response.h"

using namespace lmshao;

// Helper function to parse RTSP URL
struct RTSPUrlInfo {
    std::string server_ip;
    uint16_t server_port;
    std::string full_url;
    std::string path;
};

bool ParseRTSPUrl(const std::string &url, RTSPUrlInfo &info)
{
    // Expected format: rtsp://host:port/path or rtsp://host/path
    if (url.substr(0, 7) != "rtsp://") {
        std::cerr << "Invalid RTSP URL: must start with rtsp://" << std::endl;
        return false;
    }

    std::string remainder = url.substr(7); // Remove "rtsp://"

    // Find the first '/' to separate host:port from path
    size_t path_pos = remainder.find('/');
    std::string host_port;

    if (path_pos != std::string::npos) {
        host_port = remainder.substr(0, path_pos);
        info.path = "/" + remainder.substr(path_pos + 1);
    } else {
        host_port = remainder;
        info.path = "/";
    }

    // Parse host and port
    size_t colon_pos = host_port.find(':');
    if (colon_pos != std::string::npos) {
        info.server_ip = host_port.substr(0, colon_pos);
        std::string port_str = host_port.substr(colon_pos + 1);
        try {
            info.server_port = std::stoi(port_str);
        } catch (...) {
            std::cerr << "Invalid port number in URL: " << port_str << std::endl;
            return false;
        }
    } else {
        info.server_ip = host_port;
        info.server_port = 554; // Default RTSP port
    }

    info.full_url = url;
    return true;
}

class RTSPClient : public lmnet::IClientListener, public std::enable_shared_from_this<RTSPClient> {
public:
    RTSPClient() : is_playing_(false), rtp_packet_count_(0), rtcp_packet_count_(0) {}

    ~RTSPClient()
    {
        if (rtp_file_.is_open()) {
            rtp_file_.close();
        }
        if (rtcp_file_.is_open()) {
            rtcp_file_.close();
        }
    }

    void Start(const std::string &ip, uint16_t port, const std::string &stream_url, const std::string &output_prefix)
    {
        ip_ = ip;
        port_ = port;
        stream_url_ = stream_url;
        output_prefix_ = output_prefix;

        // Open output files
        rtp_file_.open(output_prefix_ + "_rtp.bin", std::ios::binary);
        rtcp_file_.open(output_prefix_ + "_rtcp.bin", std::ios::binary);

        if (!rtp_file_.is_open() || !rtcp_file_.is_open()) {
            std::cerr << "Failed to open output files" << std::endl;
            return;
        }

        tcp_client_ = lmnet::TcpClient::Create(ip, port);
        tcp_client_->SetListener(shared_from_this());
        tcp_client_->Init();
        tcp_client_->Connect();

        // Start with OPTIONS request
        SendOptions();
    }

    void OnReceive(lmnet::socket_t fd, std::shared_ptr<lmcore::DataBuffer> buffer) override
    {
        // Append received data to buffer
        recv_buffer_.insert(recv_buffer_.end(), buffer->Data(), buffer->Data() + buffer->Size());

        // Process all complete messages in buffer
        while (!recv_buffer_.empty()) {
            if (is_playing_) {
                // When playing, check for RTP interleaved data
                if (recv_buffer_[0] == '$') {
                    // RTP Interleaved frame format: $ + channel(1) + length(2) + data
                    if (recv_buffer_.size() < 4) {
                        break; // Need more data for header
                    }

                    uint8_t channel = recv_buffer_[1];
                    uint16_t length = (static_cast<uint16_t>(recv_buffer_[2]) << 8) | recv_buffer_[3];

                    if (recv_buffer_.size() < 4 + length) {
                        break; // Need more data
                    }

                    // Extract RTP/RTCP packet
                    std::vector<uint8_t> packet(recv_buffer_.begin() + 4, recv_buffer_.begin() + 4 + length);

                    if (channel == rtp_channel_) {
                        // RTP packet
                        HandleRTPPacket(packet);
                    } else if (channel == rtcp_channel_) {
                        // RTCP packet
                        HandleRTCPPacket(packet);
                    }

                    // Remove processed data
                    recv_buffer_.erase(recv_buffer_.begin(), recv_buffer_.begin() + 4 + length);
                } else {
                    // RTSP response (e.g., TEARDOWN response during playing)
                    if (!TryParseRTSPResponse()) {
                        break; // Need more data
                    }
                }
            } else {
                // Not playing yet, only expect RTSP responses
                if (!TryParseRTSPResponse()) {
                    break; // Need more data
                }
            }
        }
    }

    void OnClose(lmnet::socket_t fd) override
    {
        std::cout << "Disconnected from server" << std::endl;
        std::cout << "Total RTP packets received: " << rtp_packet_count_ << std::endl;
        std::cout << "Total RTCP packets received: " << rtcp_packet_count_ << std::endl;
    }

    void OnError(lmnet::socket_t fd, const std::string &error) override
    {
        std::cerr << "Error: " << error << std::endl;
    }

private:
    bool TryParseRTSPResponse()
    {
        // Look for end of headers (CRLF CRLF)
        std::string buffer_str(recv_buffer_.begin(), recv_buffer_.end());
        size_t header_end = buffer_str.find("\r\n\r\n");

        if (header_end == std::string::npos) {
            return false; // Need more data
        }

        // Check if we have Content-Length
        size_t content_length = 0;
        size_t content_length_pos = buffer_str.find("Content-Length:");
        if (content_length_pos != std::string::npos && content_length_pos < header_end) {
            size_t colon_pos = buffer_str.find(':', content_length_pos);
            size_t value_start = buffer_str.find_first_not_of(" \t", colon_pos + 1);
            size_t value_end = buffer_str.find("\r\n", value_start);
            if (value_start != std::string::npos && value_end != std::string::npos) {
                std::string length_str = buffer_str.substr(value_start, value_end - value_start);
                try {
                    content_length = std::stoul(length_str);
                } catch (...) {
                    content_length = 0;
                }
            }
        }

        // Check if we have complete message
        size_t total_length = header_end + 4 + content_length;
        if (recv_buffer_.size() < total_length) {
            return false; // Need more data
        }

        // Extract and parse response
        std::string response_str(recv_buffer_.begin(), recv_buffer_.begin() + total_length);
        auto response = lmrtsp::RTSPResponse::FromString(response_str);

        std::cout << "Received response:\n" << response.ToString() << std::endl;

        // Handle response based on CSeq
        HandleRTSPResponse(response);

        // Remove processed data
        recv_buffer_.erase(recv_buffer_.begin(), recv_buffer_.begin() + total_length);

        return true;
    }

    void HandleRTSPResponse(const lmrtsp::RTSPResponse &response)
    {
        if (response.status_ != lmrtsp::StatusCode::OK) {
            std::cerr << "RTSP request failed with status: " << static_cast<int>(response.status_) << std::endl;
            return;
        }

        // Get CSeq to determine which response this is
        int response_cseq = 0;
        if (response.general_header_.find("CSeq") != response.general_header_.end()) {
            try {
                response_cseq = std::stoi(response.general_header_.at("CSeq"));
            } catch (...) {
                std::cerr << "Failed to parse CSeq" << std::endl;
                return;
            }
        }

        if (response_cseq == 1) {
            // OPTIONS response
            SendDescribe();
        } else if (response_cseq == 2) {
            // DESCRIBE response - parse SDP to get track info
            if (response.message_body_.has_value()) {
                std::cout << "SDP:\n" << response.message_body_.value() << std::endl;
            }
            SendSetup();
        } else if (response_cseq == 3) {
            // SETUP response - parse transport and session
            if (response.general_header_.find("Session") != response.general_header_.end()) {
                session_id_ = response.general_header_.at("Session");
                // Remove timeout if present
                size_t semicolon = session_id_.find(';');
                if (semicolon != std::string::npos) {
                    session_id_ = session_id_.substr(0, semicolon);
                }
                std::cout << "Session ID: " << session_id_ << std::endl;
            }

            if (response.general_header_.find("Transport") != response.general_header_.end()) {
                std::string transport = response.general_header_.at("Transport");
                ParseTransport(transport);
            }

            SendPlay();
        } else if (response_cseq == 4) {
            // PLAY response
            is_playing_ = true;
            std::cout << "Started playing, receiving RTP data..." << std::endl;

            // Play for 10 seconds, then teardown
            std::thread([this]() {
                std::this_thread::sleep_for(std::chrono::seconds(10));
                SendTeardown();
            }).detach();
        } else if (response_cseq == 5) {
            // TEARDOWN response
            is_playing_ = false;
            std::cout << "Playback stopped" << std::endl;
            tcp_client_->Close();
        }
    }

    void ParseTransport(const std::string &transport)
    {
        std::cout << "Transport: " << transport << std::endl;

        // Parse interleaved channels
        size_t interleaved_pos = transport.find("interleaved=");
        if (interleaved_pos != std::string::npos) {
            size_t channel_start = interleaved_pos + 12; // "interleaved="
            size_t channel_end = transport.find_first_of(";", channel_start);
            std::string channels_str = transport.substr(
                channel_start, channel_end == std::string::npos ? std::string::npos : channel_end - channel_start);

            size_t dash_pos = channels_str.find('-');
            if (dash_pos != std::string::npos) {
                try {
                    rtp_channel_ = std::stoi(channels_str.substr(0, dash_pos));
                    rtcp_channel_ = std::stoi(channels_str.substr(dash_pos + 1));
                    std::cout << "RTP channel: " << static_cast<int>(rtp_channel_)
                              << ", RTCP channel: " << static_cast<int>(rtcp_channel_) << std::endl;
                } catch (...) {
                    std::cerr << "Failed to parse interleaved channels" << std::endl;
                }
            }
        }
    }

    void HandleRTPPacket(const std::vector<uint8_t> &packet)
    {
        rtp_packet_count_++;

        if (rtp_packet_count_ % 100 == 0) {
            std::cout << "Received " << rtp_packet_count_ << " RTP packets" << std::endl;
        }

        // Save to file
        if (rtp_file_.is_open()) {
            rtp_file_.write(reinterpret_cast<const char *>(packet.data()), packet.size());
        }

        // Parse RTP header (basic info)
        if (packet.size() >= 12) {
            uint8_t pt = packet[1] & 0x7F;
            uint16_t seq = (static_cast<uint16_t>(packet[2]) << 8) | packet[3];
            uint32_t timestamp = (static_cast<uint32_t>(packet[4]) << 24) | (static_cast<uint32_t>(packet[5]) << 16) |
                                 (static_cast<uint32_t>(packet[6]) << 8) | packet[7];

            if (rtp_packet_count_ <= 5 || rtp_packet_count_ % 100 == 0) {
                std::cout << "  RTP: PT=" << static_cast<int>(pt) << ", Seq=" << seq << ", TS=" << timestamp
                          << ", Size=" << packet.size() << std::endl;
            }
        }
    }

    void HandleRTCPPacket(const std::vector<uint8_t> &packet)
    {
        rtcp_packet_count_++;

        std::cout << "Received RTCP packet #" << rtcp_packet_count_ << ", size=" << packet.size() << std::endl;

        // Save to file
        if (rtcp_file_.is_open()) {
            rtcp_file_.write(reinterpret_cast<const char *>(packet.data()), packet.size());
        }
    }

    void SendOptions()
    {
        cseq_ = 1;
        auto req = lmrtsp::RTSPRequestFactory::CreateOptions(cseq_, stream_url_).Build();
        std::cout << "Sending request:\n" << req.ToString() << std::endl;
        tcp_client_->Send(req.ToString().c_str(), req.ToString().length());
    }

    void SendDescribe()
    {
        cseq_ = 2;
        auto req = lmrtsp::RTSPRequestFactory::CreateDescribe(cseq_, stream_url_).SetAccept("application/sdp").Build();
        std::cout << "Sending request:\n" << req.ToString() << std::endl;
        tcp_client_->Send(req.ToString().c_str(), req.ToString().length());
    }

    void SendSetup()
    {
        cseq_ = 3;
        // Use TCP interleaved transport
        auto req = lmrtsp::RTSPRequestFactory::CreateSetup(cseq_, stream_url_ + "/track1")
                       .SetTransport("RTP/AVP/TCP;unicast;interleaved=0-1")
                       .Build();
        std::cout << "Sending request:\n" << req.ToString() << std::endl;
        tcp_client_->Send(req.ToString().c_str(), req.ToString().length());
    }

    void SendPlay()
    {
        cseq_ = 4;
        auto req = lmrtsp::RTSPRequestFactory::CreatePlay(cseq_, stream_url_).SetSession(session_id_).Build();
        std::cout << "Sending request:\n" << req.ToString() << std::endl;
        tcp_client_->Send(req.ToString().c_str(), req.ToString().length());
    }

    void SendTeardown()
    {
        cseq_ = 5;
        auto req = lmrtsp::RTSPRequestFactory::CreateTeardown(cseq_, stream_url_).SetSession(session_id_).Build();
        std::cout << "Sending request:\n" << req.ToString() << std::endl;
        tcp_client_->Send(req.ToString().c_str(), req.ToString().length());
    }

    // Member variables
    std::string ip_;
    uint16_t port_;
    std::string stream_url_;
    std::string output_prefix_;
    std::shared_ptr<lmnet::TcpClient> tcp_client_;
    int cseq_ = 0;
    std::string session_id_;

    // RTP/RTCP channels
    uint8_t rtp_channel_ = 0;
    uint8_t rtcp_channel_ = 1;

    // State
    bool is_playing_;

    // Receive buffer for handling partial packets
    std::vector<uint8_t> recv_buffer_;

    // Output files
    std::ofstream rtp_file_;
    std::ofstream rtcp_file_;

    // Statistics
    uint64_t rtp_packet_count_;
    uint64_t rtcp_packet_count_;
};

int main(int argc, char *argv[])
{
    if (argc != 2 && argc != 3) {
        std::cerr << "Usage: " << argv[0] << " <rtsp_url> [output_prefix]" << std::endl;
        std::cerr << "Examples:" << std::endl;
        std::cerr << "  " << argv[0] << " rtsp://127.0.0.1:8554/live" << std::endl;
        std::cerr << "  " << argv[0] << " rtsp://192.168.1.100/stream output" << std::endl;
        std::cerr << "  " << argv[0] << " rtsp://example.com:554/media/video1 my_output" << std::endl;
        return 1;
    }

    std::string rtsp_url = argv[1];
    std::string output_prefix = argc == 3 ? argv[2] : "client_output";

    // Parse RTSP URL
    RTSPUrlInfo url_info;
    if (!ParseRTSPUrl(rtsp_url, url_info)) {
        std::cerr << "Failed to parse RTSP URL: " << rtsp_url << std::endl;
        return 1;
    }

    std::cout << "RTSP Client starting..." << std::endl;
    std::cout << "Server: " << url_info.server_ip << ":" << url_info.server_port << std::endl;
    std::cout << "Stream: " << url_info.full_url << std::endl;
    std::cout << "Path: " << url_info.path << std::endl;
    std::cout << "Output: " << output_prefix << "_rtp.bin, " << output_prefix << "_rtcp.bin" << std::endl;

    auto client = std::make_shared<RTSPClient>();
    client->Start(url_info.server_ip, url_info.server_port, url_info.full_url, output_prefix);

    // Keep main thread alive
    while (true) {
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }

    return 0;
}
