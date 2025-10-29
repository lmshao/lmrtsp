/**
 * @author SHAO Liming <lmshao@163.com>
 * @copyright Copyright (c) 2025 SHAO Liming
 * @license MIT
 *
 * SPDX-License-Identifier: MIT
 */

#include <signal.h>

#include <atomic>
#include <chrono>
#include <fstream>
#include <iostream>
#include <memory>
#include <string>
#include <thread>
#include <vector>

#include "lmcore/data_buffer.h"
#include "lmrtsp/media_types.h"
#include "lmrtsp/rtp_sink_session.h"
#include "lmrtsp/transport_config.h"

using namespace lmshao::lmrtsp;

// Global flag for graceful shutdown
std::atomic<bool> g_running{true};

void SignalHandler(int signal)
{
    std::cout << "\nReceived signal " << signal << ", shutting down..." << std::endl;
    g_running = false;
}

class H264FileReceiver : public RtpSinkSessionListener, public std::enable_shared_from_this<H264FileReceiver> {
public:
    H264FileReceiver(const std::string &output_file, uint16_t listen_port)
        : output_file_(output_file), listen_port_(listen_port), frames_received_(0), total_bytes_received_(0)
    {
    }

    ~H264FileReceiver() { Stop(); }

    bool Initialize()
    {
        // Open output file for writing
        output_file_stream_.open(output_file_, std::ios::binary);
        if (!output_file_stream_.is_open()) {
            std::cerr << "Failed to open output file: " << output_file_ << std::endl;
            return false;
        }

        // Configure RTP sink session
        RtpSinkSessionConfig config;
        config.session_id = "h264_receiver_session";
        config.expected_ssrc = 0; // Accept any SSRC
        config.video_type = MediaType::H264;
        config.video_payload_type = 96;

        // Configure transport for UDP sink mode
        config.transport.type = TransportConfig::Type::UDP;
        config.transport.mode = TransportConfig::Mode::SINK;
        config.transport.server_rtp_port = listen_port_;
        config.transport.server_rtcp_port = listen_port_ + 1;

        // Initialize RTP session
        rtp_session_ = std::make_unique<RtpSinkSession>();
        if (!rtp_session_->Initialize(config)) {
            std::cerr << "Failed to initialize RTP sink session" << std::endl;
            return false;
        }

        // Set frame listener
        rtp_session_->SetListener(std::static_pointer_cast<RtpSinkSessionListener>(shared_from_this()));

        std::cout << "RTP receiver initialized successfully" << std::endl;
        std::cout << "Listening on port: " << listen_port_ << std::endl;
        std::cout << "Output file: " << output_file_ << std::endl;

        return true;
    }

    bool Start()
    {
        if (!rtp_session_->Start()) {
            std::cerr << "Failed to start RTP session" << std::endl;
            return false;
        }

        std::cout << "Starting H.264 RTP receiver..." << std::endl;
        std::cout << "Waiting for RTP packets... (Press Ctrl+C to stop)" << std::endl;

        // Main loop - wait for frames
        auto last_stats_time = std::chrono::steady_clock::now();
        const auto stats_interval = std::chrono::seconds(5);

        while (g_running) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));

            // Print statistics every 5 seconds
            auto now = std::chrono::steady_clock::now();
            if (now - last_stats_time >= stats_interval) {
                PrintStatistics();
                last_stats_time = now;
            }
        }

        std::cout << "Stopping receiver..." << std::endl;
        return true;
    }

    void Stop()
    {
        if (rtp_session_) {
            rtp_session_->Stop();
        }
        if (output_file_stream_.is_open()) {
            output_file_stream_.close();
        }
        PrintFinalStatistics();
    }

    // RtpSinkSessionListener implementation
    void OnFrame(const std::shared_ptr<MediaFrame> &frame) override
    {
        std::cout << "OnFrame called" << std::endl;

        if (!frame) {
            std::cout << "Frame is null!" << std::endl;
            return;
        }

        std::cout << "Frame media_type: " << static_cast<int>(frame->media_type) << std::endl;
        std::cout << "Expected H264 type: " << static_cast<int>(MediaType::H264) << std::endl;

        if (!frame->data) {
            std::cout << "Frame data is null!" << std::endl;
            return;
        }

        if (frame->media_type != MediaType::H264) {
            std::cout << "Frame is not H264 type!" << std::endl;
            return;
        }

        // Write frame data to output file
        const uint8_t *data = frame->data->Data();
        size_t size = frame->data->Size();

        std::cout << "Frame data pointer: " << static_cast<const void *>(data) << std::endl;
        std::cout << "Frame data size: " << size << std::endl;

        if (output_file_stream_.is_open()) {
            std::cout << "Writing " << size << " bytes to file" << std::endl;
            output_file_stream_.write(reinterpret_cast<const char *>(data), size);
            output_file_stream_.flush();
            std::cout << "File write completed" << std::endl;
        } else {
            std::cout << "Output file stream is not open!" << std::endl;
        }

        frames_received_++;
        total_bytes_received_ += size;

        // Print frame info
        std::cout << "Frame " << frames_received_ << " received: " << size << " bytes"
                  << " (timestamp: " << frame->timestamp
                  << ", key frame: " << (frame->video_param.is_key_frame ? "yes" : "no") << ")" << std::endl;

        // Analyze NALU types in the frame
        if (size > 0) {
            AnalyzeFrame(data, size);
        } else {
            std::cout << "Skipping frame analysis due to zero size" << std::endl;
        }
    }

    void OnError(int code, const std::string &message) override
    {
        std::cerr << "RTP Depacketizer Error: " << code << " - " << message << std::endl;
    }

private:
    void AnalyzeFrame(const uint8_t *data, size_t size)
    {
        std::vector<uint8_t> nalu_types;

        // Find all NALU start codes and extract types
        for (size_t i = 0; i < size - 4; ++i) {
            // Look for 4-byte start code (0x00000001)
            if (data[i] == 0x00 && data[i + 1] == 0x00 && data[i + 2] == 0x00 && data[i + 3] == 0x01) {
                if (i + 4 < size) {
                    uint8_t nalu_type = data[i + 4] & 0x1F;
                    nalu_types.push_back(nalu_type);
                }
                i += 3; // Skip ahead
            }
            // Look for 3-byte start code (0x000001)
            else if (data[i] == 0x00 && data[i + 1] == 0x00 && data[i + 2] == 0x01) {
                if (i + 3 < size) {
                    uint8_t nalu_type = data[i + 3] & 0x1F;
                    nalu_types.push_back(nalu_type);
                }
                i += 2; // Skip ahead
            }
        }

        if (!nalu_types.empty()) {
            std::cout << "  NALUs: ";
            for (uint8_t type : nalu_types) {
                std::cout << GetNALUTypeName(type) << "(" << static_cast<int>(type) << ") ";
            }
            std::cout << std::endl;
        }
    }

    std::string GetNALUTypeName(uint8_t nalu_type)
    {
        switch (nalu_type) {
            case 1:
                return "P-frame";
            case 5:
                return "IDR";
            case 6:
                return "SEI";
            case 7:
                return "SPS";
            case 8:
                return "PPS";
            case 9:
                return "AUD";
            default:
                return "Unknown";
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
        std::cout << "==================\n" << std::endl;
    }

    void PrintFinalStatistics()
    {
        std::cout << "\n=== Final Statistics ===" << std::endl;
        std::cout << "Total frames received: " << frames_received_ << std::endl;
        std::cout << "Total bytes received: " << total_bytes_received_ << " bytes" << std::endl;
        if (frames_received_ > 0) {
            std::cout << "Average frame size: " << (total_bytes_received_ / frames_received_) << " bytes" << std::endl;
        }
        std::cout << "Output file: " << output_file_ << std::endl;
        std::cout << "=========================" << std::endl;
    }

private:
    std::string output_file_;
    uint16_t listen_port_;
    std::ofstream output_file_stream_;
    std::unique_ptr<RtpSinkSession> rtp_session_;
    std::atomic<size_t> frames_received_;
    std::atomic<size_t> total_bytes_received_;
};

void PrintUsage(const char *program_name)
{
    std::cout << "Usage: " << program_name << " <output_h264_file> <listen_port>" << std::endl;
    std::cout << "Example: " << program_name << " received.h264 5006" << std::endl;
}

int main(int argc, char *argv[])
{
    try {
        if (argc != 3) {
            PrintUsage(argv[0]);
            return 1;
        }

        std::string output_file = argv[1];
        uint16_t listen_port = static_cast<uint16_t>(std::stoi(argv[2]));

        // Set up signal handlers for graceful shutdown
        signal(SIGINT, SignalHandler);
        signal(SIGTERM, SignalHandler);

        std::cout << "RTP H.264 File Receiver" << std::endl;
        std::cout << "=======================" << std::endl;

        auto receiver = std::make_shared<H264FileReceiver>(output_file, listen_port);

        if (!receiver->Initialize()) {
            std::cerr << "Failed to initialize receiver" << std::endl;
            return 1;
        }

        if (!receiver->Start()) {
            std::cerr << "Failed to start receiver" << std::endl;
            return 1;
        }

        receiver->Stop();
        std::cout << "Receiver stopped successfully" << std::endl;

        return 0;
    } catch (const std::exception &e) {
        std::cerr << "Exception in main: " << e.what() << std::endl;
        return 1;
    } catch (...) {
        std::cerr << "Unknown exception in main" << std::endl;
        return 1;
    }
}