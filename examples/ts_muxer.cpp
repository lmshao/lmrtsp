/**
 * @author SHAO Liming <lmshao@163.com>
 * @copyright Copyright (c) 2025 SHAO Liming
 * @license MIT
 *
 * SPDX-License-Identifier: MIT
 */

#include "rtsp/mpegts/ts_muxer.h"

#include <chrono>
#include <climits>
#include <fstream>
#include <iostream>
#include <memory>
#include <thread>
#include <vector>

#include "rtsp/mpegts/adts_header.h"
#include "rtsp/mpegts/ts_types.h"

using namespace lmshao::rtsp::mpegts;

// Configuration constants
constexpr uint16_t VIDEO_PID = 0x0100;        // 256
constexpr uint16_t AUDIO_PID = 0x0101;        // 257
constexpr uint16_t PCR_PID = VIDEO_PID;       // Use video PID for PCR
constexpr uint32_t VIDEO_FRAMERATE = 30;      // 30 fps - correct framerate for the source video
constexpr uint32_t AUDIO_SAMPLE_RATE = 48000; // 48kHz
constexpr uint32_t AAC_FRAME_SIZE = 1024;     // AAC frame samples
constexpr size_t MAX_FRAME_SIZE = 64 * 1024;  // 64KB max frame size

// Calculate timing intervals
constexpr uint32_t VIDEO_INTERVAL_MS = 1000 / VIDEO_FRAMERATE;                      // 33.33ms per frame
constexpr uint32_t AUDIO_INTERVAL_MS = (AAC_FRAME_SIZE * 1000) / AUDIO_SAMPLE_RATE; // ~21.33ms per AAC frame

/**
 * H.264 NALU parser for frame detection
 */
class H264FrameParser {
public:
    struct Frame {
        std::vector<uint8_t> data;
        bool is_keyframe;
        uint64_t timestamp_ms;
    };

    std::vector<Frame> ParseFile(const std::string &filename)
    {
        std::vector<Frame> frames;
        std::ifstream file(filename, std::ios::binary);
        if (!file.is_open()) {
            std::cerr << "Error: Cannot open H.264 file: " << filename << std::endl;
            return frames;
        }

        // Read entire file
        file.seekg(0, std::ios::end);
        size_t file_size = file.tellg();
        file.seekg(0, std::ios::beg);

        std::vector<uint8_t> data(file_size);
        file.read(reinterpret_cast<char *>(data.data()), file_size);
        file.close();

        // Parse NAL units
        size_t offset = 0;
        uint64_t frame_count = 0;
        std::vector<uint8_t> current_frame_data;
        bool has_video_data = false;

        while (offset < data.size()) {
            // Find start code (0x00000001 or 0x000001)
            size_t start_pos = FindStartCode(data, offset);
            if (start_pos == std::string::npos) {
                break;
            }

            // Find next start code
            size_t next_pos = FindStartCode(data, start_pos + 4);
            if (next_pos == std::string::npos) {
                next_pos = data.size();
            }

            // Extract NAL unit
            std::vector<uint8_t> nal_data(data.begin() + start_pos, data.begin() + next_pos);

            // Check NAL unit type (after start code)
            size_t nal_start = (data[start_pos + 2] == 0x01) ? start_pos + 3 : start_pos + 4;
            if (nal_start < data.size()) {
                uint8_t nal_type = data[nal_start] & 0x1F;

                // Handle different NAL unit types
                if (nal_type == 9) { // AUD - starts a new frame
                    // If we have accumulated frame data, create a frame
                    if (has_video_data && !current_frame_data.empty()) {
                        Frame frame;
                        frame.data = std::move(current_frame_data);
                        frame.is_keyframe = false; // Will be determined by slice type
                        frame.timestamp_ms = (uint64_t)(frame_count * 1000.0 / VIDEO_FRAMERATE);

                        // Check if this frame contains IDR slice
                        for (size_t i = 0; i < frame.data.size() - 4; i++) {
                            if (frame.data[i] == 0x00 && frame.data[i + 1] == 0x00 &&
                                (frame.data[i + 2] == 0x01 ||
                                 (frame.data[i + 2] == 0x00 && frame.data[i + 3] == 0x01))) {
                                size_t nal_pos = (frame.data[i + 2] == 0x01) ? i + 3 : i + 4;
                                if (nal_pos < frame.data.size()) {
                                    uint8_t slice_nal_type = frame.data[nal_pos] & 0x1F;
                                    if (slice_nal_type == 5) {
                                        frame.is_keyframe = true;
                                        break;
                                    }
                                }
                            }
                        }

                        frames.push_back(std::move(frame));
                        frame_count++;

                        if (frames.size() % 100 == 0) {
                            std::cout << "Parsed " << frames.size() << " video frames..." << std::endl;
                        }
                    }

                    // Start new frame with AUD
                    current_frame_data = nal_data;
                    has_video_data = false;
                } else if (nal_type == 7 || nal_type == 8) { // SPS/PPS
                    // Add SPS/PPS to current frame
                    current_frame_data.insert(current_frame_data.end(), nal_data.begin(), nal_data.end());
                } else if (nal_type == 1 || nal_type == 5) { // Slice data
                    // Add slice data to current frame
                    current_frame_data.insert(current_frame_data.end(), nal_data.begin(), nal_data.end());
                    has_video_data = true;

                    // Update keyframe status for IDR
                    if (!frames.empty()) {
                        frames.back().is_keyframe = (nal_type == 5);
                    } else if (nal_type == 5) {
                        // Mark current frame as keyframe (will be set when frame is created)
                        has_video_data = true; // Flag for keyframe
                    }
                }
            }

            offset = next_pos;
        }

        // Handle any remaining frame data
        if (has_video_data && !current_frame_data.empty()) {
            Frame frame;
            frame.data = std::move(current_frame_data);
            frame.is_keyframe = false;
            frame.timestamp_ms = (uint64_t)(frame_count * 1000.0 / VIDEO_FRAMERATE);

            // Check if this frame contains IDR slice
            for (size_t i = 0; i < frame.data.size() - 4; i++) {
                if (frame.data[i] == 0x00 && frame.data[i + 1] == 0x00 &&
                    (frame.data[i + 2] == 0x01 || (frame.data[i + 2] == 0x00 && frame.data[i + 3] == 0x01))) {
                    size_t nal_pos = (frame.data[i + 2] == 0x01) ? i + 3 : i + 4;
                    if (nal_pos < frame.data.size()) {
                        uint8_t slice_nal_type = frame.data[nal_pos] & 0x1F;
                        if (slice_nal_type == 5) {
                            frame.is_keyframe = true;
                            break;
                        }
                    }
                }
            }

            frames.push_back(std::move(frame));
            frame_count++;
        }

        std::cout << "H.264: Found " << frames.size()
                  << " video frames, duration: " << (frames.empty() ? 0 : frames.back().timestamp_ms / 1000.0)
                  << " seconds" << std::endl;
        return frames;
    }

private:
    size_t FindStartCode(const std::vector<uint8_t> &data, size_t start)
    {
        for (size_t i = start; i + 3 < data.size(); ++i) {
            if (data[i] == 0x00 && data[i + 1] == 0x00) {
                if (data[i + 2] == 0x01) {
                    return i; // 3-byte start code
                } else if (data[i + 2] == 0x00 && i + 4 < data.size() && data[i + 3] == 0x01) {
                    return i; // 4-byte start code
                }
            }
        }
        return std::string::npos;
    }
};

/**
 * Simple AAC frame reader (handles ADTS format)
 */
class AACFrameReader {
public:
    struct Frame {
        std::vector<uint8_t> data;
        uint64_t timestamp_ms;
    };

    std::vector<Frame> ReadFile(const std::string &filename)
    {
        std::vector<Frame> frames;
        std::ifstream file(filename, std::ios::binary);
        if (!file.is_open()) {
            std::cerr << "Error: Cannot open AAC file: " << filename << std::endl;
            return frames;
        }

        // Read entire file
        file.seekg(0, std::ios::end);
        size_t file_size = file.tellg();
        file.seekg(0, std::ios::beg);

        std::vector<uint8_t> buffer(file_size);
        file.read(reinterpret_cast<char *>(buffer.data()), file_size);
        file.close();

        // Find ADTS frames
        size_t offset = 0;
        uint64_t frame_count = 0;
        while (offset + 7 < buffer.size()) {
            ADTSHeader adts_header;
            if (adts_header.Parse(buffer.data() + offset, buffer.size() - offset)) {
                int frame_length = adts_header.GetFrameLength();
                if (frame_length == 0) {
                    offset++;
                    continue;
                }
                std::cout << "AAC Frame: channels=" << adts_header.GetChannels() << std::endl;

                if (offset + frame_length <= buffer.size()) {
                    Frame frame;
                    // Keep the ADTS header in the frame data
                    frame.data.assign(buffer.begin() + offset, buffer.begin() + offset + frame_length);
                    frame.timestamp_ms = (uint64_t)(frame_count * 1000.0 * AAC_FRAME_SIZE / AUDIO_SAMPLE_RATE);
                    frames.push_back(std::move(frame));
                    offset += frame_length;
                    frame_count++;
                } else {
                    break; // Incomplete frame
                }
            } else {
                offset++; // Sync word not found, move to the next byte
            }
        }

        std::cout << "AAC: Found " << frames.size()
                  << " frames, duration: " << (frames.empty() ? 0 : frames.back().timestamp_ms / 1000.0) << " seconds"
                  << std::endl;
        return frames;
    }
};

/**
 * TS output callback - saves TS packets to file
 */
class TSFileWriter : public TSMuxerListener {
public:
    TSFileWriter(const std::string &filename) : filename_(filename), packet_count_(0), total_bytes_(0)
    {
        output_file_.open(filename, std::ios::binary);
        if (!output_file_.is_open()) {
            std::cerr << "Error: Cannot create TS file: " << filename << std::endl;
        }
    }

    ~TSFileWriter()
    {
        if (output_file_.is_open()) {
            output_file_.close();
            std::cout << "TS muxing completed:" << std::endl;
            std::cout << "  Output file: " << filename_ << std::endl;
            std::cout << "  TS packets: " << packet_count_ << std::endl;
            std::cout << "  Total size: " << total_bytes_ << " bytes" << std::endl;
        }
    }

    // Override callback method to capture TS packet data from muxer
    void OnTSPacket(const uint8_t *data, size_t size) override { WriteRawData(data, size); }

private:
    void WriteRawData(const uint8_t *data, size_t size)
    {
        if (!output_file_.is_open())
            return;

        // Debug: check packet size
        if (size != 188) {
            std::cerr << "Warning: TS packet size is " << size << " bytes (expected 188)" << std::endl;
        }

        output_file_.write(reinterpret_cast<const char *>(data), size);
        total_bytes_ += size;

        // Estimate packet count (assuming 188-byte TS packets)
        packet_count_ = total_bytes_ / 188;

        if (packet_count_ % 1000 == 0 && packet_count_ > 0) {
            std::cout << "Written ~" << packet_count_ << " TS packets..." << std::endl;
        }
    }

    std::string filename_;
    std::ofstream output_file_;
    size_t packet_count_;
    size_t total_bytes_;
};

/**
 * TS Muxer Controller - orchestrates the muxing process
 */
class TSMuxerController {
public:
    TSMuxerController(const std::string &h264_file, const std::string &aac_file, const std::string &ts_file)
        : h264_file_(h264_file), aac_file_(aac_file), ts_writer_(std::make_shared<TSFileWriter>(ts_file)),
          has_audio_(!aac_file.empty())
    {
        // Initialize muxer
        muxer_.SetCallback(ts_writer_);
        muxer_.SetVideoPID(VIDEO_PID);
        muxer_.SetAudioPID(AUDIO_PID);
        muxer_.SetPCRPID(PCR_PID);
        muxer_.SetBitrate(2000000); // 2 Mbps
        muxer_.SetPCRInterval(40);  // PCR every 40 packets
    }

    bool ParseInputFiles()
    {
        std::cout << "Parsing input files..." << std::endl;

        // Parse H.264 file
        H264FrameParser h264_parser;
        video_frames_ = h264_parser.ParseFile(h264_file_);
        if (video_frames_.empty()) {
            std::cerr << "Error: No video frames found" << std::endl;
            return false;
        }

        // Parse AAC file (optional)
        if (has_audio_) {
            AACFrameReader aac_reader;
            audio_frames_ = aac_reader.ReadFile(aac_file_);
            if (audio_frames_.empty()) {
                std::cout << "Warning: No audio frames found, creating video-only stream" << std::endl;
                has_audio_ = false;
            }
        }

        // Use video duration as primary timeline (pad or loop audio if needed)
        uint64_t video_duration = video_frames_.back().timestamp_ms;
        uint64_t audio_duration = has_audio_ ? audio_frames_.back().timestamp_ms : video_duration;
        end_time_ms_ = video_duration; // Use full video duration

        std::cout << "Stream durations:" << std::endl;
        std::cout << "  Video: " << video_duration / 1000.0 << " seconds" << std::endl;
        if (has_audio_) {
            std::cout << "  Audio: " << audio_duration / 1000.0 << " seconds" << std::endl;
            if (audio_duration < video_duration) {
                std::cout << "  Audio will be padded with silence to match video duration" << std::endl;
            }
        }
        std::cout << "  Output: " << end_time_ms_ / 1000.0 << " seconds (full video)" << std::endl;

        return true;
    }

    void StartMuxing()
    {
        if (video_frames_.empty()) {
            std::cerr << "Error: Input files not parsed" << std::endl;
            return;
        }

        std::cout << "Starting TS muxing..." << std::endl;
        muxer_.Start();

        // Note: PAT and PMT are automatically sent by the muxer when started

        size_t video_index = 0;
        size_t audio_index = 0;

        // Process frames in timestamp order without real-time constraint
        while (video_index < video_frames_.size() || (has_audio_ && audio_index < audio_frames_.size())) {
            bool sent_frame = false;

            // Determine next frame to send based on timestamp
            uint64_t next_video_time =
                (video_index < video_frames_.size()) ? video_frames_[video_index].timestamp_ms : UINT64_MAX;
            uint64_t next_audio_time = (has_audio_ && audio_index < audio_frames_.size())
                                           ? audio_frames_[audio_index].timestamp_ms
                                           : UINT64_MAX;

            // Force monotonic audio timestamps
            if (has_audio_ && audio_index < audio_frames_.size() && audio_index > 0) {
                uint64_t expected_audio_time = (audio_index * AAC_FRAME_SIZE * 1000) / AUDIO_SAMPLE_RATE;
                if (expected_audio_time > next_audio_time) {
                    next_audio_time = expected_audio_time;
                    audio_frames_[audio_index].timestamp_ms = expected_audio_time;
                }
            }

            // Send video frame if available
            if (video_index < video_frames_.size() && next_video_time <= next_audio_time) {
                muxer_.MuxVideoData(VIDEO_PID, video_frames_[video_index].data.data(),
                                    video_frames_[video_index].data.size(), video_frames_[video_index].timestamp_ms);
                video_index++;
                sent_frame = true;
            }
            // Send audio frame if available and its time hasn't passed
            else if (has_audio_ && audio_index < audio_frames_.size()) {
                muxer_.MuxAudioData(AUDIO_PID, audio_frames_[audio_index].data.data(),
                                    audio_frames_[audio_index].data.size(), audio_frames_[audio_index].timestamp_ms);
                audio_index++;
                sent_frame = true;
            }

            if (!sent_frame) {
                break; // No more frames to process
            }
        }

        // Finalize the muxing process
        muxer_.Finalize();

        printf("Muxing completed. Processed %zu video frames and %zu audio frames.\n", video_index, audio_index);
    }

private:
    std::string h264_file_;
    std::string aac_file_;
    std::shared_ptr<TSFileWriter> ts_writer_;
    TSStreamMuxer muxer_;
    bool has_audio_;

    std::vector<H264FrameParser::Frame> video_frames_;
    std::vector<AACFrameReader::Frame> audio_frames_;
    uint64_t end_time_ms_;

    uint64_t GetCurrentTimeMs()
    {
        auto now = std::chrono::steady_clock::now();
        auto duration = now.time_since_epoch();
        return std::chrono::duration_cast<std::chrono::milliseconds>(duration).count();
    }
};

int main(int argc, char *argv[])
{
    if (argc < 3 || argc > 4) {
        std::cout << "TS Muxer Demo - Mux H.264 and optional AAC files into MPEG-TS stream" << std::endl;
        std::cout << "Usage: " << argv[0] << " <input.h264> [input.aac] <output.ts>" << std::endl;
        std::cout << std::endl;
        std::cout << "Parameters:" << std::endl;
        std::cout << "  input.h264  - H.264 elementary stream file (Annex-B format)" << std::endl;
        std::cout << "  input.aac   - AAC audio file (ADTS format or raw) [optional]" << std::endl;
        std::cout << "  output.ts   - Output MPEG-TS file" << std::endl;
        std::cout << std::endl;
        std::cout << "Examples:" << std::endl;
        std::cout << "  " << argv[0] << " test_video.h264 output.ts                # Video only" << std::endl;
        std::cout << "  " << argv[0] << " test_video.h264 test_audio.aac output.ts # Video + Audio" << std::endl;
        return 1;
    }

    std::string h264_file = argv[1];
    std::string aac_file;
    std::string ts_file;

    if (argc == 3) {
        // Video only mode
        ts_file = argv[2];
    } else {
        // Video + Audio mode
        aac_file = argv[2];
        ts_file = argv[3];
    }

    try {
        TSMuxerController controller(h264_file, aac_file, ts_file);

        if (!controller.ParseInputFiles()) {
            std::cerr << "Error: Failed to parse input files" << std::endl;
            return 1;
        }

        controller.StartMuxing();
        std::cout << "Success! TS file created: " << ts_file << std::endl;

    } catch (const std::exception &e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}
