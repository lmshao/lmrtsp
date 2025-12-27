/**
 * @author SHAO Liming <lmshao@163.com>
 * @copyright Copyright (c) 2025 SHAO Liming
 * @license MIT
 *
 * SPDX-License-Identifier: MIT
 *
 * @brief RTSP Recorder
 * A tool for pulling RTSP streams and recording to file
 */

#include <signal.h>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <ctime>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <memory>
#include <sstream>
#include <string>
#include <thread>

#include "lmcore/lmcore_logger.h"
#include "lmnet/lmnet_logger.h"
#include "lmrtsp/irtsp_client_listener.h"
#include "lmrtsp/media_types.h"
#include "lmrtsp/rtsp_client.h"

using namespace lmshao::lmnet;
using namespace lmshao::lmrtsp;

// Global flag for graceful shutdown
std::atomic<bool> g_running{true};

void SignalHandler(int signal)
{
    std::cout << "\nReceived signal " << signal << ", shutting down..." << std::endl;
    g_running = false;
}

class SimpleRTSPClient : public IRtspClientListener, public std::enable_shared_from_this<SimpleRTSPClient> {
public:
    SimpleRTSPClient() : frames_received_(0), total_bytes_received_(0), codec_detected_(false) {}

    ~SimpleRTSPClient()
    {
        if (output_file_.is_open()) {
            output_file_.close();
        }
        // Print filename on destruction (e.g., Ctrl+C)
        if (codec_detected_ && !output_filename_.empty()) {
            std::cout << "\nRecording saved to: " << output_filename_ << std::endl;
        }
    }

    bool Initialize(const std::string &rtsp_url)
    {
        rtsp_url_ = rtsp_url;

        // Create RTSP client
        client_ = std::make_shared<RtspClient>();
        client_->SetUserAgent("lmrtsp-recorder/1.0");

        // Set listener (now that shared_ptr is created)
        client_->SetListener(shared_from_this());

        // Initialize with URL
        if (!client_->Init(rtsp_url_)) {
            std::cerr << "Failed to initialize RTSP client" << std::endl;
            return false;
        }

        std::cout << "RTSP Client initialized" << std::endl;
        std::cout << "RTSP URL: " << rtsp_url_ << std::endl;

        return true;
    }

    bool Start()
    {
        std::cout << "Starting RTSP client..." << std::endl;

        // Start streaming (automatically handles Connect -> DESCRIBE -> SETUP -> PLAY)
        if (!client_->Start()) {
            std::cerr << "Failed to start RTSP stream" << std::endl;
            return false;
        }

        std::cout << "RTSP stream started successfully" << std::endl;
        std::cout << "Receiving media stream... (Press Ctrl+C to stop)" << std::endl;

        // Main loop - wait for frames
        auto last_stats_time = std::chrono::steady_clock::now();
        const auto stats_interval = std::chrono::seconds(5);

        while (g_running && client_->IsPlaying()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));

            // Print statistics every 5 seconds
            auto now = std::chrono::steady_clock::now();
            if (now - last_stats_time >= stats_interval) {
                PrintStatistics();
                last_stats_time = now;
            }
        }

        // Stop streaming (automatically handles TEARDOWN -> Disconnect)
        client_->Stop();

        // Print output filename on exit
        if (codec_detected_ && !output_filename_.empty()) {
            std::cout << "\nRecording saved to: " << output_filename_ << std::endl;
        }

        std::cout << "RTSP client stopped successfully" << std::endl;

        return true;
    }

    // Get output filename (public for main function)
    std::string GetOutputFilename() const { return output_filename_; }

private:
    void PrintStatistics()
    {
        std::cout << "\n=== Statistics ===" << std::endl;
        std::cout << "Frames received: " << frames_received_ << std::endl;
        std::cout << "Total bytes: " << total_bytes_received_ << " bytes" << std::endl;
        if (frames_received_ > 0) {
            std::cout << "Average frame size: " << (total_bytes_received_ / frames_received_) << " bytes" << std::endl;
        }
        std::cout << "Playing: " << (client_->IsPlaying() ? "yes" : "no") << std::endl;
        std::cout << "==================\n" << std::endl;
    }

    // IRtspClientListener implementation
    void OnConnected(const std::string &server_url) override
    {
        std::cout << "Connected to: " << server_url << std::endl;
    }

    void OnDisconnected(const std::string &server_url) override
    {
        std::cout << "Disconnected from: " << server_url << std::endl;
        g_running = false;
    }

    void OnDescribeReceived(const std::string &server_url, const std::string &sdp) override
    {
        std::cout << "DESCRIBE response received" << std::endl;
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

        // Detect codec type from SDP and generate output filename
        std::string detected_codec = DetectCodecFromSDP(sdp);
        if (detected_codec.empty()) {
            std::cerr << "Warning: Could not detect codec from SDP, using default .h264" << std::endl;
            detected_codec = "h264";
        }

        // Generate filename with timestamp
        output_filename_ = GenerateFilename(detected_codec);

        // Open output file for writing
        output_file_.open(output_filename_, std::ios::binary);
        if (!output_file_.is_open()) {
            std::cerr << "Failed to open output file: " << output_filename_ << std::endl;
            return;
        }

        std::cout << "Detected codec: " << detected_codec << std::endl;
        std::cout << "Output file: " << output_filename_ << std::endl;
        codec_detected_ = true;
    }

    void OnSetupReceived(const std::string &server_url, const std::string &session_id,
                         const std::string &transport) override
    {
        std::cout << "SETUP response received" << std::endl;
        std::cout << "Session ID: " << session_id << std::endl;
        std::cout << "Transport: " << transport << std::endl;
    }

    void OnPlayReceived(const std::string &server_url, const std::string &session_id,
                        const std::string &rtp_info) override
    {
        std::cout << "PLAY response received" << std::endl;
        std::cout << "Session ID: " << session_id << std::endl;
        if (!rtp_info.empty()) {
            std::cout << "RTP Info: " << rtp_info << std::endl;
        }
    }

    void OnPauseReceived(const std::string &server_url, const std::string &session_id) override
    {
        std::cout << "PAUSE response received" << std::endl;
        std::cout << "Session ID: " << session_id << std::endl;
    }

    void OnTeardownReceived(const std::string &server_url, const std::string &session_id) override
    {
        std::cout << "TEARDOWN response received" << std::endl;
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
        std::cerr << "Error from " << server_url << ": " << error_code << " - " << error_message << std::endl;
        g_running = false;
    }

    void OnStateChanged(const std::string &server_url, const std::string &old_state,
                        const std::string &new_state) override
    {
        std::cout << "State changed: " << old_state << " -> " << new_state << std::endl;
    }

private:
    // Detect codec type from SDP description
    std::string DetectCodecFromSDP(const std::string &sdp)
    {
        std::istringstream sdp_stream(sdp);
        std::string line;
        bool in_media_block = false;
        std::string media_type;
        std::string codec_name;

        while (std::getline(sdp_stream, line)) {
            // Remove carriage return if present
            line.erase(line.find_last_not_of("\r") + 1);
            if (line.empty())
                continue;

            // Check for media description line (m=)
            if (line[0] == 'm' && line[1] == '=') {
                in_media_block = true;
                // Parse: m=video 0 RTP/AVP 96
                // Parse: m=video 0 RTP/AVP 33  (MPEG-2 TS)
                std::istringstream media_line(line.substr(2));
                media_line >> media_type; // video or audio
                // Skip port, protocol
                std::string temp;
                media_line >> temp >> temp;
                // Get payload type
                std::string payload_type_str;
                media_line >> payload_type_str;

                // Check for standard payload types that don't need rtpmap
                if (payload_type_str == "33") {
                    // MPEG-2 Transport Stream (RFC 2250)
                    return "ts";
                }

                codec_name.clear();
            }
            // Check for rtpmap attribute
            else if (in_media_block && line[0] == 'a' && line[1] == '=') {
                if (line.find("rtpmap:") != std::string::npos) {
                    // Parse: a=rtpmap:96 H264/90000
                    size_t colon_pos = line.find(':');
                    if (colon_pos != std::string::npos) {
                        std::string rtpmap = line.substr(colon_pos + 1);
                        std::istringstream rtpmap_stream(rtpmap);
                        std::string payload_type;
                        rtpmap_stream >> payload_type;
                        std::string codec_info;
                        rtpmap_stream >> codec_info;

                        // Extract codec name (before /)
                        size_t slash_pos = codec_info.find('/');
                        if (slash_pos != std::string::npos) {
                            codec_name = codec_info.substr(0, slash_pos);
                        } else {
                            codec_name = codec_info;
                        }

                        // Normalize codec name
                        if (codec_name == "H264" || codec_name == "h264") {
                            return "h264";
                        } else if (codec_name == "H265" || codec_name == "h265" || codec_name == "HEVC" ||
                                   codec_name == "hevc") {
                            return "h265";
                        } else if (codec_name == "MP2T" || codec_name == "mp2t") {
                            return "ts";
                        } else if (codec_name == "MPEG4-GENERIC" || codec_name == "mpeg4-generic" ||
                                   codec_name == "AAC" || codec_name == "aac") {
                            return "aac";
                        }
                    }
                }
                // Check for MP2T (MPEG-TS)
                else if (line.find("MP2T") != std::string::npos || line.find("mp2t") != std::string::npos) {
                    return "ts";
                }
                // Check for AAC in fmtp
                else if (line.find("fmtp:") != std::string::npos) {
                    if (line.find("mpeg4-generic") != std::string::npos || line.find("AAC") != std::string::npos ||
                        line.find("aac") != std::string::npos) {
                        return "aac";
                    }
                }
            }
        }

        // If codec_name was found but not matched above, try to use it
        if (!codec_name.empty()) {
            // Convert to lowercase
            std::string lower_codec = codec_name;
            std::transform(lower_codec.begin(), lower_codec.end(), lower_codec.begin(), ::tolower);
            if (lower_codec == "h264")
                return "h264";
            if (lower_codec == "h265" || lower_codec == "hevc")
                return "h265";
            if (lower_codec == "aac" || lower_codec == "mpeg4-generic")
                return "aac";
        }

        // Default: try to detect from media type
        if (media_type == "video") {
            return "h264"; // Default video codec
        } else if (media_type == "audio") {
            return "aac"; // Default audio codec
        }

        return ""; // Unknown
    }

    // Generate filename with timestamp
    std::string GenerateFilename(const std::string &codec_extension)
    {
        auto now = std::chrono::system_clock::now();
        auto time_t = std::chrono::system_clock::to_time_t(now);
        auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()) % 1000;

        std::tm *tm_info = std::localtime(&time_t);
        std::ostringstream filename;
        filename << std::put_time(tm_info, "%Y%m%d_%H%M%S");
        filename << "_" << std::setfill('0') << std::setw(3) << ms.count();
        filename << "." << codec_extension;

        return filename.str();
    }

private:
    std::string rtsp_url_;
    std::string output_filename_;
    std::ofstream output_file_;
    std::shared_ptr<RtspClient> client_;
    std::atomic<size_t> frames_received_{0};
    std::atomic<size_t> total_bytes_received_{0};
    std::atomic<bool> codec_detected_{false};
};

void PrintUsage(const char *program_name)
{
    std::cout << "Usage: " << program_name << " <rtsp_url>" << std::endl;
    std::cout << "Example: " << program_name << " rtsp://127.0.0.1:8554/live" << std::endl;
    std::cout << "\nNote: Output filename will be auto-generated based on:" << std::endl;
    std::cout << "  - Detected codec type (H264/H265/AAC/TS)" << std::endl;
    std::cout << "  - Current timestamp (format: YYYYMMDD_HHMMSS_MMM.ext)" << std::endl;
}

int main(int argc, char *argv[])
{
    if (argc != 2) {
        PrintUsage(argv[0]);
        return 1;
    }

    std::string rtsp_url = argv[1];

    // Set up signal handlers for graceful shutdown
    signal(SIGINT, SignalHandler);
    signal(SIGTERM, SignalHandler);

    std::cout << "RTSP Recorder" << std::endl;
    std::cout << "=============" << std::endl;

    // Initialize LMNet logger with DEBUG level
    InitLmnetLogger(lmshao::lmcore::LogLevel::kDebug);

    auto client = std::make_shared<SimpleRTSPClient>();

    if (!client->Initialize(rtsp_url)) {
        std::cerr << "Failed to initialize RTSP client" << std::endl;
        return 1;
    }

    if (!client->Start()) {
        std::cerr << "Failed to start RTSP client" << std::endl;
        return 1;
    }

    // Print final statistics and filename
    std::cout << "\nFinal Statistics:" << std::endl;
    std::string output_file = client->GetOutputFilename();
    if (!output_file.empty()) {
        std::cout << "Output file: " << output_file << std::endl;
    }
    std::cout << "Client completed successfully" << std::endl;

    return 0;
}