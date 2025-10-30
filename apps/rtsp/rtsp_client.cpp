/**
 * @author SHAO Liming <lmshao@163.com>
 * @copyright Copyright (c) 2025 SHAO Liming
 * @license MIT
 *
 * SPDX-License-Identifier: MIT
 */

#include "lmrtsp/rtsp_client.h"

#include <signal.h>

#include <atomic>
#include <chrono>
#include <fstream>
#include <iostream>
#include <memory>
#include <string>
#include <thread>

#include "lmrtsp/irtsp_client_callback.h"
#include "lmrtsp/media_types.h"
#include "lmrtsp/rtsp_client_session.h"

using namespace lmshao::lmrtsp;

// Global flag for graceful shutdown
std::atomic<bool> g_running{true};

void SignalHandler(int signal)
{
    std::cout << "\nReceived signal " << signal << ", shutting down..." << std::endl;
    g_running = false;
}

class SimpleRTSPClient : public IRTSPClientCallback, public std::enable_shared_from_this<SimpleRTSPClient> {
public:
    explicit SimpleRTSPClient(const std::string &output_file)
        : output_filename_(output_file), frames_received_(0), total_bytes_received_(0)
    {
    }

    ~SimpleRTSPClient()
    {
        if (output_file_.is_open()) {
            output_file_.close();
        }
    }

    bool Initialize(const std::string &rtsp_url)
    {
        rtsp_url_ = rtsp_url;

        // Open output file for writing
        output_file_.open(output_filename_, std::ios::binary);
        if (!output_file_.is_open()) {
            std::cerr << "Failed to open output file: " << output_filename_ << std::endl;
            return false;
        }

        // Create RTSP client - callback will be set after initialization
        client_ = std::make_shared<RTSPClient>();
        client_->SetUserAgent("lmrtsp-client-demo/1.0");
        // Note: callback set in main after shared_ptr is created

        std::cout << "RTSP Client initialized" << std::endl;
        std::cout << "RTSP URL: " << rtsp_url_ << std::endl;
        std::cout << "Output file: " << output_filename_ << std::endl;

        return true;
    }

    void SetCallbackToClient()
    {
        if (client_) {
            client_->SetCallback(shared_from_this());
        }
    }

    bool Start()
    {
        try {
            std::cout << "Starting RTSP client..." << std::endl;

            // Connect to server
            if (!client_->Connect(rtsp_url_)) {
                std::cerr << "Failed to connect to RTSP server" << std::endl;
                return false;
            }

            std::cout << "Connected to RTSP server" << std::endl;

            // Start RTSP handshake
            if (!PerformRTSPHandshake()) {
                std::cerr << "RTSP handshake failed" << std::endl;
                return false;
            }

            std::cout << "RTSP handshake completed successfully" << std::endl;
            std::cout << "Receiving media stream... (Press Ctrl+C to stop)" << std::endl;

            // Main loop - wait for frames
            auto last_stats_time = std::chrono::steady_clock::now();
            const auto stats_interval = std::chrono::seconds(5);

            while (g_running && session_ && session_->GetState() == RTSPClientSessionState::PLAYING) {
                std::this_thread::sleep_for(std::chrono::milliseconds(100));

                // Print statistics every 5 seconds
                auto now = std::chrono::steady_clock::now();
                if (now - last_stats_time >= stats_interval) {
                    PrintStatistics();
                    last_stats_time = now;
                }
            }

            // Cleanup
            if (session_) {
                client_->Teardown(rtsp_url_);
                client_->RemoveSession(session_->GetSessionId());
            }

            client_->Disconnect();
            std::cout << "RTSP client stopped successfully" << std::endl;

            return true;
        } catch (const std::exception &e) {
            std::cerr << "Exception in Start: " << e.what() << std::endl;
            return false;
        }
    }

private:
    bool PerformRTSPHandshake()
    {
        try {
            // Step 1: DESCRIBE
            std::cout << "Sending DESCRIBE request..." << std::endl;
            if (!client_->Describe(rtsp_url_)) {
                std::cerr << "Failed to send DESCRIBE request" << std::endl;
                return false;
            }

            // Wait for DESCRIBE response
            std::this_thread::sleep_for(std::chrono::milliseconds(500));

            // Step 2: SETUP
            if (!session_) {
                std::cerr << "No session created after DESCRIBE" << std::endl;
                return false;
            }

            std::cout << "Sending SETUP request..." << std::endl;
            std::string setup_url = rtsp_url_;
            if (setup_url.back() != '/') {
                setup_url += "/trackID=1"; // Common track ID for H.264
            } else {
                setup_url += "trackID=1";
            }

            std::string transport = session_->GetTransportInfo();
            if (transport.empty()) {
                transport = "RTP/AVP;unicast;client_port=5000-5001";
            }

            if (!client_->Setup(setup_url, transport)) {
                std::cerr << "Failed to send SETUP request" << std::endl;
                return false;
            }

            // Wait for SETUP response
            std::this_thread::sleep_for(std::chrono::milliseconds(500));

            // Step 3: PLAY
            std::cout << "Sending PLAY request..." << std::endl;
            if (!client_->Play(rtsp_url_)) {
                std::cerr << "Failed to send PLAY request" << std::endl;
                return false;
            }

            // Wait for PLAY response
            std::this_thread::sleep_for(std::chrono::milliseconds(500));

            return true;
        } catch (const std::exception &e) {
            std::cerr << "Exception in RTSP handshake: " << e.what() << std::endl;
            return false;
        }
    }

    void PrintStatistics()
    {
        std::cout << "\n=== Statistics ===" << std::endl;
        std::cout << "Frames received: " << frames_received_ << std::endl;
        std::cout << "Total bytes: " << total_bytes_received_ << " bytes" << std::endl;
        if (frames_received_ > 0) {
            std::cout << "Average frame size: " << (total_bytes_received_ / frames_received_) << " bytes" << std::endl;
        }
        if (session_) {
            std::cout << "Session state: " << session_->GetStateString() << std::endl;
        }
        std::cout << "==================\n" << std::endl;
    }

    // IRTSPClientCallback implementation
    void OnConnected(const std::string &server_url) override
    {
        std::cout << "✓ Connected to: " << server_url << std::endl;
    }

    void OnDisconnected(const std::string &server_url) override
    {
        std::cout << "✗ Disconnected from: " << server_url << std::endl;
        g_running = false;
    }

    void OnDescribeReceived(const std::string &server_url, const std::string &sdp) override
    {
        std::cout << "✓ DESCRIBE response received" << std::endl;
        std::cout << "SDP Content (" << sdp.length() << " bytes):" << std::endl;

        // Print first few lines of SDP
        std::istringstream sdp_stream(sdp);
        std::string line;
        int line_count = 0;
        while (std::getline(sdp_stream, line) && line_count < 10) {
            std::cout << "  " << line << std::endl;
            line_count++;
        }
        if (line_count >= 10) {
            std::cout << "  ... (truncated)" << std::endl;
        }

        // Create session after receiving SDP
        session_ = client_->CreateSession(server_url);
    }

    void OnSetupReceived(const std::string &server_url, const std::string &session_id,
                         const std::string &transport) override
    {
        std::cout << "✓ SETUP response received" << std::endl;
        std::cout << "Session ID: " << session_id << std::endl;
        std::cout << "Transport: " << transport << std::endl;
    }

    void OnPlayReceived(const std::string &server_url, const std::string &session_id,
                        const std::string &rtp_info) override
    {
        std::cout << "✓ PLAY response received" << std::endl;
        std::cout << "Session ID: " << session_id << std::endl;
        if (!rtp_info.empty()) {
            std::cout << "RTP Info: " << rtp_info << std::endl;
        }
    }

    void OnPauseReceived(const std::string &server_url, const std::string &session_id) override
    {
        std::cout << "✓ PAUSE response received" << std::endl;
        std::cout << "Session ID: " << session_id << std::endl;
    }

    void OnTeardownReceived(const std::string &server_url, const std::string &session_id) override
    {
        std::cout << "✓ TEARDOWN response received" << std::endl;
        std::cout << "Session ID: " << session_id << std::endl;
        g_running = false;
    }

    void OnFrame(const std::shared_ptr<MediaFrame> &frame) override
    {
        if (!frame || !frame->data) {
            return;
        }

        // Write frame data to output file
        const uint8_t *data = frame->data->Data();
        size_t size = frame->data->Size();

        if (output_file_.is_open()) {
            output_file_.write(reinterpret_cast<const char *>(data), size);
            output_file_.flush();
        }

        frames_received_++;
        total_bytes_received_ += size;

        // Print frame info (only first frame and every 30 frames)
        if (frames_received_ == 1 || frames_received_ % 30 == 0) {
            std::cout << "Frame " << frames_received_ << " received: " << size << " bytes"
                      << " (timestamp: " << frame->timestamp
                      << ", key frame: " << (frame->video_param.is_key_frame ? "yes" : "no") << ")" << std::endl;
        }
    }

    void OnError(const std::string &server_url, int error_code, const std::string &error_message) override
    {
        std::cerr << "✗ Error from " << server_url << ": " << error_code << " - " << error_message << std::endl;
        g_running = false;
    }

    void OnStateChanged(const std::string &server_url, const std::string &old_state,
                        const std::string &new_state) override
    {
        std::cout << "State changed: " << old_state << " -> " << new_state << std::endl;
    }

private:
    std::string rtsp_url_;
    std::string output_filename_;
    std::ofstream output_file_;
    std::shared_ptr<RTSPClient> client_;
    std::shared_ptr<RTSPClientSession> session_;
    std::atomic<size_t> frames_received_{0};
    std::atomic<size_t> total_bytes_received_{0};
};

void PrintUsage(const char *program_name)
{
    std::cout << "Usage: " << program_name << " <rtsp_url> [output_h264_file]" << std::endl;
    std::cout << "Example: " << program_name << " rtsp://127.0.0.1:8554/live received.h264" << std::endl;
}

int main(int argc, char *argv[])
{
    try {
        if (argc != 2 && argc != 3) {
            PrintUsage(argv[0]);
            return 1;
        }

        std::string rtsp_url = argv[1];
        std::string output_file = (argc == 3) ? argv[2] : "received.h264";

        // Set up signal handlers for graceful shutdown
        signal(SIGINT, SignalHandler);
        signal(SIGTERM, SignalHandler);

        std::cout << "RTSP H.264 Client" << std::endl;
        std::cout << "==================" << std::endl;

        // No logger initialization needed - using std::cout directly

        auto client = std::make_shared<SimpleRTSPClient>(output_file);

        if (!client->Initialize(rtsp_url)) {
            std::cerr << "Failed to initialize RTSP client" << std::endl;
            return 1;
        }

        // Set callback now that shared_ptr exists
        client->SetCallbackToClient();

        if (!client->Start()) {
            std::cerr << "Failed to start RTSP client" << std::endl;
            return 1;
        }

        std::cout << "\nFinal Statistics:" << std::endl;
        std::cout << "Output file: " << output_file << std::endl;
        std::cout << "Client completed successfully" << std::endl;

        return 0;
    } catch (const std::exception &e) {
        std::cerr << "Exception in main: " << e.what() << std::endl;
        return 1;
    } catch (...) {
        std::cerr << "Unknown exception in main" << std::endl;
        return 1;
    }
}