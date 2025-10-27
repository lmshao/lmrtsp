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
    H264FileSender(const std::string &file_path, const std::string &dest_ip, uint16_t dest_port)
        : file_path_(file_path), dest_ip_(dest_ip), dest_port_(dest_port)
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
        const uint32_t frame_rate = 24; // 24 FPS
        const uint32_t frame_interval_ms = 1000 / frame_rate;
        uint32_t timestamp = 0;
        const uint32_t timestamp_increment = 90000 / frame_rate; // 90kHz clock

        std::vector<uint8_t> sps_data, pps_data; // Store SPS/PPS for reuse with video frames
        std::vector<uint8_t> access_unit_buffer; // Buffer for building complete access unit
        size_t frames_sent = 0;
        size_t total_nalus_read = 0;

        while (true) {
            // Read next NALU from file
            std::vector<uint8_t> nalu_data;
            std::cout << "About to call ReadNextNALU..." << std::endl;
            bool read_result = ReadNextNALU(nalu_data);
            std::cout << "ReadNextNALU returned: " << (read_result ? "true" : "false") << std::endl;

            if (!read_result) {
                std::cout << "End of file reached or read error" << std::endl;
                break;
            }

            if (nalu_data.empty()) {
                std::cout << "Warning: Empty NALU data, skipping..." << std::endl;
                continue;
            }

            total_nalus_read++;

            // Debug: Print NALU info
            uint8_t nalu_type = nalu_data[0] & 0x1F;
            std::cout << "Read NALU #" << total_nalus_read << ": size=" << nalu_data.size()
                      << ", type=" << static_cast<int>(nalu_type) << ", first_bytes=[" << std::hex;
            for (size_t i = 0; i < std::min(nalu_data.size(), size_t(8)); ++i) {
                std::cout << "0x" << static_cast<int>(nalu_data[i]) << " ";
            }
            std::cout << "]" << std::dec << std::endl;

            // Handle different NALU types according to H.264 standard
            if (nalu_type == 7) { // SPS (Sequence Parameter Set)
                sps_data = nalu_data;
                std::cout << "Stored SPS data, size=" << sps_data.size() << std::endl;
                continue;                // Don't send SPS alone, wait for video frame
            } else if (nalu_type == 8) { // PPS (Picture Parameter Set)
                pps_data = nalu_data;
                std::cout << "Stored PPS data, size=" << pps_data.size() << std::endl;
                continue;                // Don't send PPS alone, wait for video frame
            } else if (nalu_type == 6) { // SEI (Supplemental Enhancement Information)
                std::cout << "Skipping SEI NALU" << std::endl;
                continue;                                  // Skip SEI for now
            } else if (nalu_type == 5 || nalu_type == 1) { // IDR or non-IDR frame
                // Build complete access unit with proper H.264 structure
                access_unit_buffer.clear();

                // For IDR frames, prepend SPS and PPS to create complete access unit
                if (nalu_type == 5 && !sps_data.empty() && !pps_data.empty()) {
                    // Add SPS with H.264 start code
                    uint8_t start_code[] = {0x00, 0x00, 0x00, 0x01};
                    access_unit_buffer.insert(access_unit_buffer.end(), start_code, start_code + 4);
                    access_unit_buffer.insert(access_unit_buffer.end(), sps_data.begin(), sps_data.end());

                    // Add PPS with H.264 start code
                    access_unit_buffer.insert(access_unit_buffer.end(), start_code, start_code + 4);
                    access_unit_buffer.insert(access_unit_buffer.end(), pps_data.begin(), pps_data.end());

                    std::cout << "Added SPS (" << sps_data.size() << " bytes) and PPS (" << pps_data.size()
                              << " bytes) to IDR frame" << std::endl;
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
                media_frame->video_param.is_key_frame = (nalu_type == 5); // IDR frame is key frame

                // Debug: Verify MediaFrame data
                std::cout << "MediaFrame created: data_size=" << media_frame->data->Size()
                          << ", timestamp=" << timestamp << ", is_key_frame=" << media_frame->video_param.is_key_frame
                          << ", access_unit_nalus=" << (nalu_type == 5 ? "SPS+PPS+IDR" : "P-frame") << std::endl;

                // Send frame via RTP
                std::cout << "About to send frame via RTP..." << std::endl;
                bool send_result = false;
                try {
                    send_result = rtp_session_->SendFrame(media_frame);
                    std::cout << "SendFrame returned: " << (send_result ? "true" : "false") << std::endl;
                } catch (const std::exception &e) {
                    std::cerr << "Exception in SendFrame: " << e.what() << std::endl;
                    send_result = false;
                } catch (...) {
                    std::cerr << "Unknown exception in SendFrame" << std::endl;
                    send_result = false;
                }

                if (!send_result) {
                    std::cerr << "Failed to send frame " << frames_sent << std::endl;
                    break;
                } else {
                    frames_sent++;
                    std::cout << "Successfully sent frame " << frames_sent << std::endl;
                }

                // Update timestamp for next frame (90kHz clock)
                timestamp += timestamp_increment;

                // Maintain frame rate timing and ensure async RTP packet transmission
                std::cout << "Sleeping for " << frame_interval_ms << "ms..." << std::endl;
                std::this_thread::sleep_for(std::chrono::milliseconds(frame_interval_ms));

                // Additional wait to ensure all async RTP packets are sent
                std::cout << "Additional wait for async sending completion..." << std::endl;
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
                std::cout << "Sleep completed, continuing loop..." << std::endl;
            } else {
                std::cout << "Skipping unsupported NALU type: " << static_cast<int>(nalu_type) << std::endl;
                continue;
            }
        }

        std::cout << "Total NALUs read: " << total_nalus_read << std::endl;
        std::cout << "Total frames sent: " << frames_sent << std::endl;
        return true;
    }

    bool ReadNextNALU(std::vector<uint8_t> &nalu_data)
    {
        nalu_data.clear();

        static int call_count = 0;
        call_count++;

        std::cout << "ReadNextNALU call #" << call_count << ", file position=" << file_.tellg() << std::endl;

        uint8_t byte;
        std::vector<uint8_t> buffer;

        // Look for the next start code
        uint32_t start_code_candidate = 0;
        int bytes_read = 0;
        bool found_start = false;

        while (file_.read(reinterpret_cast<char *>(&byte), 1)) {
            bytes_read++;
            start_code_candidate = (start_code_candidate << 8) | byte;

            // Debug: Print every 100 bytes
            if (bytes_read % 100 == 0) {
                std::cout << "  Searching for start code, read " << bytes_read << " bytes, current candidate=0x"
                          << std::hex << start_code_candidate << std::dec << std::endl;
            }

            // Check for 4-byte start code (0x00000001)
            if (start_code_candidate == 0x00000001) {
                std::cout << "  Found 4-byte start code at position " << (file_.tellg() - std::streampos(4))
                          << std::endl;
                found_start = true;
                break;
            }
            // Check for 3-byte start code (0x000001) - need to check last 24 bits
            else if ((start_code_candidate & 0x00FFFFFF) == 0x000001) {
                std::cout << "  Found 3-byte start code at position " << (file_.tellg() - std::streampos(3))
                          << std::endl;
                found_start = true;
                break;
            }
        }

        std::cout << "  Start code search completed, bytes_read=" << bytes_read << ", found_start=" << found_start
                  << std::endl;

        if (!found_start) {
            std::cout << "  No more start codes found, returning false" << std::endl;
            return false; // No more start codes found
        }

        // Now read NALU data until next start code or EOF
        uint32_t next_start_candidate = 0;
        bool found_next_start = false;
        int nalu_bytes_read = 0;

        std::cout << "  Reading NALU data from position " << file_.tellg() << std::endl;

        while (file_.read(reinterpret_cast<char *>(&byte), 1)) {
            nalu_bytes_read++;
            next_start_candidate = (next_start_candidate << 8) | byte;

            // Check for next start code
            if (next_start_candidate == 0x00000001) {
                // Found 4-byte start code, rewind 4 bytes
                file_.seekg(-4, std::ios::cur);
                std::cout << "  Found next 4-byte start code, NALU size=" << nalu_bytes_read - 4 << std::endl;
                found_next_start = true;
                break;
            } else if ((next_start_candidate & 0x00FFFFFF) == 0x000001) {
                // Found 3-byte start code, rewind 3 bytes
                file_.seekg(-3, std::ios::cur);
                std::cout << "  Found next 3-byte start code, NALU size=" << nalu_bytes_read - 3 << std::endl;
                found_next_start = true;
                break;
            }

            buffer.push_back(byte);
        }

        std::cout << "  NALU reading completed, nalu_bytes_read=" << nalu_bytes_read
                  << ", found_next_start=" << found_next_start << ", buffer_size=" << buffer.size() << std::endl;

        nalu_data = buffer;
        std::cout << "  Returning NALU with size=" << nalu_data.size() << std::endl;
        return !nalu_data.empty();
    }

private:
    std::string file_path_;
    std::string dest_ip_;
    uint16_t dest_port_;
    std::ifstream file_;
    std::unique_ptr<RtpSourceSession> rtp_session_;
};

void PrintUsage(const char *program_name)
{
    std::cout << "Usage: " << program_name << " <h264_file> <dest_ip> <dest_port>" << std::endl;
    std::cout << "Example: " << program_name << " test.h264 192.168.1.100 5004" << std::endl;
}

int main(int argc, char *argv[])
{
    try {
        if (argc != 4) {
            PrintUsage(argv[0]);
            return 1;
        }

        std::string h264_file = argv[1];
        std::string dest_ip = argv[2];
        uint16_t dest_port = static_cast<uint16_t>(std::stoi(argv[3]));

        std::cout << "RTP H.264 File Sender" << std::endl;
        std::cout << "=====================" << std::endl;

        H264FileSender sender(h264_file, dest_ip, dest_port);

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