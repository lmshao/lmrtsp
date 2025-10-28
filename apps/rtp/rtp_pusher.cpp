/**
 * @author SHAO Liming <lmshao@163.com>
 * @copyright Copyright (c) 2025 SHAO Liming
 * @license MIT
 *
 * SPDX-License-Identifier: MIT
 */

#include <chrono>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <memory>
#include <string>
#include <thread>
#include <vector>

#include "lmcore/data_buffer.h"
#include "lmrtsp/media_types.h"
#include "lmrtsp/rtp_source_session.h"
#include "lmrtsp/transport_config.h"

using namespace lmshao::lmrtsp;

class H264FileSender {
public:
    H264FileSender(const std::string &file_path, const std::string &dest_ip, uint16_t dest_port, uint32_t fps = 24)
        : file_path_(file_path), dest_ip_(dest_ip), dest_port_(dest_port), frame_rate_(fps)
    {
    }

    bool Initialize()
    {
        // Open H.264 file
        file_.open(file_path_, std::ios::binary);
        if (!file_.is_open()) {
            std::cerr << "Failed to open H.264 file: " << file_path_ << std::endl;
            return false;
        }

        // Configure RTP source session
        RtpSourceSessionConfig config;
        config.session_id = "h264_sender_session";
        config.ssrc = 0; // Auto-generate
        config.video_type = MediaType::H264;
        config.video_payload_type = 96;
        config.mtu_size = 1400;
        config.enable_rtcp = false;

        // Configure UDP transport
        config.transport.type = TransportConfig::Type::UDP;
        config.transport.client_ip = dest_ip_;
        config.transport.client_rtp_port = dest_port_;

        // Initialize RTP session
        rtp_session_ = std::make_unique<RtpSourceSession>();
        if (!rtp_session_->Initialize(config)) {
            std::cerr << "Failed to initialize RTP source session" << std::endl;
            return false;
        }

        std::cout << "RTP sender initialized successfully" << std::endl;
        std::cout << "Destination: " << dest_ip_ << ":" << dest_port_ << std::endl;
        std::cout << "H.264 file: " << file_path_ << std::endl;

        return true;
    }

    bool Start()
    {
        if (!rtp_session_->Start()) {
            std::cerr << "Failed to start RTP session" << std::endl;
            return false;
        }

        std::cout << "Starting H.264 file streaming..." << std::endl;
        return SendFile();
    }

    void Stop()
    {
        if (rtp_session_) {
            rtp_session_->Stop();
        }
        if (file_.is_open()) {
            file_.close();
        }
    }

private:
    bool SendFile()
    {
        const uint32_t frame_interval_ms = 1000 / frame_rate_;
        uint32_t timestamp = 0;
        const uint32_t timestamp_increment = 90000 / frame_rate_; // 90kHz clock

        std::vector<uint8_t> sps_data, pps_data; // Store SPS/PPS for reuse with video frames
        std::vector<uint8_t> access_unit_buffer; // Buffer for building complete access unit
        size_t frames_sent = 0;
        size_t total_nalus_read = 0;

        // Use absolute time points for accurate frame timing
        auto start_time = std::chrono::steady_clock::now();
        auto next_frame_time = start_time;

        while (true) {
            // Read next NALU from file
            std::vector<uint8_t> nalu_data;
            bool read_result = ReadNextNALU(nalu_data);

            if (!read_result) {
                std::cout << "End of file reached" << std::endl;
                break;
            }

            if (nalu_data.empty()) {
                continue;
            }

            total_nalus_read++;
            uint8_t nalu_type = nalu_data[0] & 0x1F;

            // Handle different NALU types according to H.264 standard
            if (nalu_type == 7) { // SPS (Sequence Parameter Set)
                sps_data = nalu_data;
                std::cout << "Stored SPS (" << sps_data.size() << " bytes)" << std::endl;
                continue;
            } else if (nalu_type == 8) { // PPS (Picture Parameter Set)
                pps_data = nalu_data;
                std::cout << "Stored PPS (" << pps_data.size() << " bytes)" << std::endl;
                continue;
            } else if (nalu_type == 6) { // SEI
                continue;
            } else if (nalu_type == 5 || nalu_type == 1) { // IDR or non-IDR frame
                // Build complete access unit with proper H.264 structure
                access_unit_buffer.clear();

                // For IDR frames, prepend SPS and PPS to create complete access unit
                if (nalu_type == 5 && !sps_data.empty() && !pps_data.empty()) {
                    uint8_t start_code[] = {0x00, 0x00, 0x00, 0x01};
                    access_unit_buffer.insert(access_unit_buffer.end(), start_code, start_code + 4);
                    access_unit_buffer.insert(access_unit_buffer.end(), sps_data.begin(), sps_data.end());
                    access_unit_buffer.insert(access_unit_buffer.end(), start_code, start_code + 4);
                    access_unit_buffer.insert(access_unit_buffer.end(), pps_data.begin(), pps_data.end());
                }

                // Add video frame with H.264 start code
                uint8_t start_code[] = {0x00, 0x00, 0x00, 0x01};
                access_unit_buffer.insert(access_unit_buffer.end(), start_code, start_code + 4);
                access_unit_buffer.insert(access_unit_buffer.end(), nalu_data.begin(), nalu_data.end());

                // Create MediaFrame with complete access unit for RTP transmission
                auto data_buffer = lmshao::lmcore::DataBuffer::Create(access_unit_buffer.size());
                data_buffer->Append(access_unit_buffer.data(), access_unit_buffer.size());
                auto media_frame = std::make_shared<MediaFrame>();
                media_frame->data = data_buffer;
                media_frame->timestamp = timestamp;
                media_frame->media_type = MediaType::H264;
                media_frame->video_param.is_key_frame = (nalu_type == 5);

                // Send frame via RTP
                if (!rtp_session_->SendFrame(media_frame)) {
                    std::cerr << "Failed to send frame " << frames_sent << std::endl;
                    break;
                }

                frames_sent++;
                if (frames_sent % 100 == 0) {
                    std::cout << "Sent " << frames_sent << " frames (" << (nalu_type == 5 ? "IDR" : "P") << ")"
                              << std::endl;
                }

                // Update timestamp for next frame (90kHz clock)
                timestamp += timestamp_increment;

                // Calculate next frame time and sleep until then
                next_frame_time += std::chrono::milliseconds(frame_interval_ms);
                std::this_thread::sleep_until(next_frame_time);
            }
        }

        std::cout << "Total NALUs read: " << total_nalus_read << std::endl;
        std::cout << "Total frames sent: " << frames_sent << std::endl;
        return true;
    }

    bool ReadNextNALU(std::vector<uint8_t> &nalu_data)
    {
        nalu_data.clear();
        uint8_t byte;
        std::vector<uint8_t> buffer;

        // Look for the next start code
        uint32_t start_code_candidate = 0;
        bool found_start = false;

        while (file_.read(reinterpret_cast<char *>(&byte), 1)) {
            start_code_candidate = (start_code_candidate << 8) | byte;

            // Check for 4-byte start code (0x00000001)
            if (start_code_candidate == 0x00000001) {
                found_start = true;
                break;
            }
            // Check for 3-byte start code (0x000001)
            else if ((start_code_candidate & 0x00FFFFFF) == 0x000001) {
                found_start = true;
                break;
            }
        }

        if (!found_start) {
            return false;
        }

        // Now read NALU data until next start code or EOF
        uint32_t next_start_candidate = 0;

        while (file_.read(reinterpret_cast<char *>(&byte), 1)) {
            next_start_candidate = (next_start_candidate << 8) | byte;

            // Check for next start code
            if (next_start_candidate == 0x00000001) {
                file_.seekg(-4, std::ios::cur);
                break;
            } else if ((next_start_candidate & 0x00FFFFFF) == 0x000001) {
                file_.seekg(-3, std::ios::cur);
                break;
            }

            buffer.push_back(byte);
        }

        nalu_data = buffer;
        return !nalu_data.empty();
    }

private:
    std::string file_path_;
    std::string dest_ip_;
    uint16_t dest_port_;
    uint32_t frame_rate_;
    std::ifstream file_;
    std::unique_ptr<RtpSourceSession> rtp_session_;
};

void PrintUsage(const char *program_name)
{
    std::cout << "Usage: " << program_name << " <h264_file> <dest_ip> <dest_port> [fps]" << std::endl;
    std::cout << "  fps: Frame rate (default: 24)" << std::endl;
    std::cout << "Example: " << program_name << " test.h264 192.168.1.100 5006 30" << std::endl;
}

int main(int argc, char *argv[])
{
    try {
        if (argc < 4 || argc > 5) {
            PrintUsage(argv[0]);
            return 1;
        }

        std::string h264_file = argv[1];
        std::string dest_ip = argv[2];
        uint16_t dest_port = static_cast<uint16_t>(std::stoi(argv[3]));
        uint32_t fps = (argc == 5) ? static_cast<uint32_t>(std::stoi(argv[4])) : 24;

        std::cout << "RTP H.264 File Sender" << std::endl;
        std::cout << "=====================" << std::endl;
        std::cout << "Frame rate: " << fps << " FPS" << std::endl;

        H264FileSender sender(h264_file, dest_ip, dest_port, fps);

        if (!sender.Initialize()) {
            std::cerr << "Failed to initialize sender" << std::endl;
            return 1;
        }

        std::cout << "About to start streaming..." << std::endl;
        if (!sender.Start()) {
            std::cerr << "Failed to start streaming" << std::endl;
            return 1;
        }

        std::cout << "Streaming completed, about to stop sender..." << std::endl;
        sender.Stop();
        std::cout << "Sender stopped, waiting for callbacks to complete..." << std::endl;

        // Sleep to ensure all callbacks complete before main process exits
        std::this_thread::sleep_for(std::chrono::seconds(3));
        std::cout << "Wait completed, about to exit main..." << std::endl;

        return 0;
    } catch (const std::exception &e) {
        std::cerr << "Exception in main: " << e.what() << std::endl;
        return 1;
    } catch (...) {
        std::cerr << "Unknown exception in main" << std::endl;
        return 1;
    }
}