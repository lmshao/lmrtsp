/**
 * @author SHAO Liming <lmshao@163.com>
 * @copyright Copyright (c) 2025 SHAO Liming
 * @license MIT
 *
 * SPDX-License-Identifier: MIT
 */

#include <fstream>
#include <iostream>
#include <map>
#include <memory>
#include <string>
#include <vector>

#include "rtsp/mpegts/ts_callback.h"
#include "rtsp/mpegts/ts_muxer.h"
#include "rtsp/mpegts/ts_parser.h"

using namespace lmshao::rtsp::mpegts;
using namespace lmshao::coreutils;

class TSDemuxerCallback : public TSPacketCallback {
private:
    std::ofstream video_file_;
    std::ofstream audio_file_;
    std::map<uint16_t, uint8_t> stream_types_; // PID -> StreamType
    bool video_file_opened_;
    bool audio_file_opened_;
    size_t video_packets_;
    size_t audio_packets_;
    size_t total_bytes_written_;
    std::string video_filename_;
    std::string audio_filename_;

public:
    TSDemuxerCallback()
        : video_file_opened_(false), audio_file_opened_(false), video_packets_(0), audio_packets_(0),
          total_bytes_written_(0)
    {
    }

    ~TSDemuxerCallback()
    {
        if (video_file_.is_open()) {
            video_file_.close();
        }
        if (audio_file_.is_open()) {
            audio_file_.close();
        }
    }

    std::string GetVideoExtension(uint8_t stream_type) const
    {
        switch (stream_type) {
            case static_cast<uint8_t>(StreamType::H264_VIDEO):
                return ".h264";
            case static_cast<uint8_t>(StreamType::H265_VIDEO):
                return ".h265";
            case static_cast<uint8_t>(StreamType::MPEG1_VIDEO):
                return ".m1v";
            case static_cast<uint8_t>(StreamType::MPEG2_VIDEO):
                return ".m2v";
            default:
                return ".video";
        }
    }

    std::string GetAudioExtension(uint8_t stream_type) const
    {
        switch (stream_type) {
            case static_cast<uint8_t>(StreamType::AAC_AUDIO):
                return ".aac";
            case static_cast<uint8_t>(StreamType::MPEG1_AUDIO):
                return ".mp1";
            case static_cast<uint8_t>(StreamType::MPEG2_AUDIO):
                return ".mp2";
            case static_cast<uint8_t>(StreamType::AC3_AUDIO):
                return ".ac3";
            case static_cast<uint8_t>(StreamType::EAC3_AUDIO):
                return ".eac3";
            default:
                return ".audio";
        }
    }

    void OnVideoData(uint16_t pid, const uint8_t *data, size_t size, uint64_t pcr) override
    {
        if (!video_file_opened_) {
            // Determine filename based on stream type
            auto it = stream_types_.find(pid);
            if (it != stream_types_.end()) {
                video_filename_ = "output" + GetVideoExtension(it->second);
            } else {
                video_filename_ = "output.video"; // Default extension
            }

            video_file_.open(video_filename_, std::ios::binary);
            if (video_file_.is_open()) {
                video_file_opened_ = true;
                std::cout << "Video file opened: " << video_filename_ << std::endl;
            } else {
                std::cerr << "Failed to open video file: " << video_filename_ << std::endl;
                return;
            }
        }

        if (video_file_.is_open() && data && size > 0) {
            video_file_.write(reinterpret_cast<const char *>(data), size);
            video_packets_++;
            total_bytes_written_ += size;

            if (video_packets_ % 100 == 0) {
                std::cout << "Video packets: " << video_packets_ << ", Total bytes: " << total_bytes_written_
                          << std::endl;
            }
        }
    }

    void OnAudioData(uint16_t pid, const uint8_t *data, size_t size, uint64_t pcr) override
    {
        if (!audio_file_opened_) {
            // Determine filename based on stream type
            auto it = stream_types_.find(pid);
            if (it != stream_types_.end()) {
                audio_filename_ = "output" + GetAudioExtension(it->second);
            } else {
                audio_filename_ = "output.audio"; // Default extension
            }

            audio_file_.open(audio_filename_, std::ios::binary);
            if (audio_file_.is_open()) {
                audio_file_opened_ = true;
                std::cout << "Audio file opened: " << audio_filename_ << std::endl;
            } else {
                std::cerr << "Failed to open audio file: " << audio_filename_ << std::endl;
                return;
            }
        }

        if (audio_file_.is_open() && data && size > 0) {
            audio_file_.write(reinterpret_cast<const char *>(data), size);
            audio_packets_++;
            total_bytes_written_ += size;

            if (audio_packets_ % 100 == 0) {
                std::cout << "Audio packets: " << audio_packets_ << ", Total bytes: " << total_bytes_written_
                          << std::endl;
            }
        }
    }

    void OnPATReceived(const std::vector<uint16_t> &program_pids) override
    {
        std::cout << "PAT received with " << program_pids.size() << " programs:" << std::endl;
        for (uint16_t pid : program_pids) {
            std::cout << "  Program PID: " << pid << std::endl;
        }
        if (program_pids.empty()) {
            std::cout << "WARNING: No programs found in PAT!" << std::endl;
        }
    }

    void OnPMTReceived(uint16_t program_pid, const std::map<uint16_t, uint8_t> &stream_pids) override
    {
        std::cout << "PMT received for program " << program_pid << " with " << stream_pids.size()
                  << " streams:" << std::endl;
        for (const auto &[pid, type] : stream_pids) {
            stream_types_[pid] = type;
            std::cout << "  Stream PID: " << pid << ", Type: " << (int)type;

            if (type == static_cast<uint8_t>(StreamType::H264_VIDEO) ||
                type == static_cast<uint8_t>(StreamType::MPEG2_VIDEO)) {
                std::cout << " (Video)";
            } else if (type == static_cast<uint8_t>(StreamType::AAC_AUDIO) ||
                       type == static_cast<uint8_t>(StreamType::MPEG1_AUDIO) ||
                       type == static_cast<uint8_t>(StreamType::MPEG2_AUDIO) ||
                       type == static_cast<uint8_t>(StreamType::AC3_AUDIO)) {
                std::cout << " (Audio)";
            }
            std::cout << std::endl;
        }
        if (stream_pids.empty()) {
            std::cout << "WARNING: No streams found in PMT!" << std::endl;
        }
    }

    void OnParseError(const std::string &error) override { std::cerr << "Parse error: " << error << std::endl; }

    void OnSyncLoss() override { std::cout << "Sync loss detected" << std::endl; }

    size_t GetVideoPackets() const { return video_packets_; }
    size_t GetAudioPackets() const { return audio_packets_; }
    size_t GetTotalBytes() const { return total_bytes_written_; }
    const std::map<uint16_t, uint8_t> &GetStreamTypes() const { return stream_types_; }
    const std::string &GetVideoFilename() const { return video_filename_; }
    const std::string &GetAudioFilename() const { return audio_filename_; }
};

std::vector<uint8_t> ReadFile(const std::string &filename)
{
    std::ifstream file(filename, std::ios::binary | std::ios::ate);
    if (!file.is_open()) {
        std::cerr << "Failed to open file: " << filename << std::endl;
        return {};
    }

    std::streamsize size = file.tellg();
    file.seekg(0, std::ios::beg);

    std::vector<uint8_t> buffer(size);
    if (!file.read(reinterpret_cast<char *>(buffer.data()), size)) {
        std::cerr << "Failed to read file: " << filename << std::endl;
        return {};
    }

    std::cout << "Read " << size << " bytes from " << filename << std::endl;
    return buffer;
}

void AutoConfigureParser(TSStreamParser &parser, const std::map<uint16_t, uint8_t> &stream_types)
{
    for (const auto &[pid, type] : stream_types) {
        std::cout << "Configuring PID " << pid << " with type " << (int)type << std::endl;
        if (type == static_cast<uint8_t>(StreamType::H264_VIDEO) ||
            type == static_cast<uint8_t>(StreamType::MPEG2_VIDEO)) {
            parser.EnableVideoStream(pid, true);
            std::cout << "Enabled video stream PID: " << pid << std::endl;
        } else if (type == static_cast<uint8_t>(StreamType::AAC_AUDIO) ||
                   type == static_cast<uint8_t>(StreamType::MPEG1_AUDIO) ||
                   type == static_cast<uint8_t>(StreamType::MPEG2_AUDIO) ||
                   type == static_cast<uint8_t>(StreamType::AC3_AUDIO)) {
            parser.EnableAudioStream(pid, true);
            std::cout << "Enabled audio stream PID: " << pid << std::endl;
        }
    }
}

int main(int argc, char *argv[])
{
    std::cout << "MPEG-TS Demuxer" << std::endl;

    if (argc < 2) {
        std::cout << "Usage: " << argv[0] << " <ts_file>" << std::endl;
        std::cout << "Example: " << argv[0] << " input.ts" << std::endl;
        std::cout << "This tool demuxes MPEG-TS files and extracts audio/video streams." << std::endl;
        return 1;
    }

    std::string ts_filename = argv[1];

    std::cout << "Reading TS file: " << ts_filename << std::endl;
    auto ts_data = ReadFile(ts_filename);
    if (ts_data.empty()) {
        std::cerr << "Failed to read TS file" << std::endl;
        return 1;
    }

    auto callback = std::make_shared<TSDemuxerCallback>();

    TSStreamParser parser;
    parser.SetCallback(callback);

    parser.Start();
    std::cout << "Starting TS demuxing..." << std::endl;

    parser.ParseData(ts_data.data(), ts_data.size());

    std::cout << "Configuring demuxer with " << callback->GetStreamTypes().size() << " stream types..." << std::endl;
    AutoConfigureParser(parser, callback->GetStreamTypes());

    std::cout << "Re-parsing with stream extraction..." << std::endl;
    parser.Reset();
    parser.Start();
    parser.ParseData(ts_data.data(), ts_data.size());

    parser.Stop();

    std::cout << "\n=== Demuxing Summary ===" << std::endl;
    std::cout << "Total TS packets parsed: " << parser.GetParsedPackets() << std::endl;
    std::cout << "Video packets extracted: " << callback->GetVideoPackets() << std::endl;
    std::cout << "Audio packets extracted: " << callback->GetAudioPackets() << std::endl;
    std::cout << "Total bytes written: " << callback->GetTotalBytes() << std::endl;

    if (callback->GetVideoPackets() > 0) {
        std::cout << "Video stream saved to: " << callback->GetVideoFilename() << std::endl;
    }
    if (callback->GetAudioPackets() > 0) {
        std::cout << "Audio stream saved to: " << callback->GetAudioFilename() << std::endl;
    }

    std::cout << "\nTS demuxing completed successfully" << std::endl;
    return 0;
}
