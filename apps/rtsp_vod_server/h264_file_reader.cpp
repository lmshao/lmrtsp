/**
 * @author SHAO Liming <lmshao@163.com>
 * @copyright Copyright (c) 2025 SHAO Liming
 * @license MIT
 *
 * SPDX-License-Identifier: MIT
 */

#include "h264_file_reader.h"

#include <iostream>

#include "lmrtsp/h264_parser.h"

H264FileReader::H264FileReader(const std::string &filename)
    : filename_(filename), frame_rate_(25) // Default 25 fps
      ,
      frame_count_(0), parameter_sets_extracted_(false), read_buffer_(BUFFER_SIZE), buffer_pos_(0), buffer_end_(0),
      eof_reached_(false)
{
}

H264FileReader::~H264FileReader()
{
    Close();
}

bool H264FileReader::Open()
{
    if (file_.is_open()) {
        return true;
    }

    file_.open(filename_, std::ios::binary);
    if (!file_.is_open()) {
        std::cerr << "Failed to open H.264 file: " << filename_ << std::endl;
        return false;
    }

    // Reset state
    buffer_pos_ = 0;
    buffer_end_ = 0;
    eof_reached_ = false;

    // Extract parameter sets and analyze file
    ExtractParameterSets();
    AnalyzeFile();

    // Reset to beginning for actual reading
    Reset();

    std::cout << "Opened H.264 file: " << filename_ << ", Frame rate: " << frame_rate_
              << " fps, Estimated frames: " << frame_count_ << std::endl;

    return true;
}

void H264FileReader::Close()
{
    if (file_.is_open()) {
        file_.close();
    }
    buffer_pos_ = 0;
    buffer_end_ = 0;
    eof_reached_ = false;
}

bool H264FileReader::IsOpen() const
{
    return file_.is_open();
}

bool H264FileReader::ReadFrame(MediaFrame &frame)
{
    if (!file_.is_open() || eof_reached_) {
        return false;
    }

    // Read pure NALU data (without start code) using stream-based approach
    // Reference: rtp_pusher.cpp ReadNextNALU implementation
    std::vector<uint8_t> nalu_data;
    uint8_t byte;
    uint32_t start_code_candidate = 0;
    bool found_start = false;

    // Look for the next start code
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
        eof_reached_ = true;
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

        nalu_data.push_back(byte);
    }

    if (nalu_data.empty()) {
        return false;
    }

    // Prepend 4-byte start code (0x00000001) before NALU data
    // Reference: rtp_pusher.cpp uses the same approach
    uint8_t start_code[] = {0x00, 0x00, 0x00, 0x01};
    std::vector<uint8_t> frame_with_startcode;
    frame_with_startcode.reserve(4 + nalu_data.size());
    frame_with_startcode.insert(frame_with_startcode.end(), start_code, start_code + 4);
    frame_with_startcode.insert(frame_with_startcode.end(), nalu_data.begin(), nalu_data.end());

    // Assign frame data with start code into DataBuffer
    frame.data = lmshao::lmcore::DataBuffer::Create(frame_with_startcode.size());
    frame.data->Assign(frame_with_startcode.data(), frame_with_startcode.size());

    return true;
}

void H264FileReader::Reset()
{
    if (file_.is_open()) {
        file_.clear();
        file_.seekg(0, std::ios::beg);
        buffer_pos_ = 0;
        buffer_end_ = 0;
        eof_reached_ = false;
    }
}

std::vector<uint8_t> H264FileReader::GetSPS() const
{
    return sps_;
}

std::vector<uint8_t> H264FileReader::GetPPS() const
{
    return pps_;
}

uint32_t H264FileReader::GetFrameRate() const
{
    return frame_rate_;
}

size_t H264FileReader::GetFrameCount() const
{
    return frame_count_;
}

bool H264FileReader::GetResolution(uint32_t &width, uint32_t &height) const
{
    if (sps_.empty()) {
        width = 1280;
        height = 720;
        return false;
    }

    // Use H264Parser to extract resolution from SPS
    auto sps_buffer = lmshao::lmcore::DataBuffer::Create(sps_.size());
    sps_buffer->Assign(sps_.data(), sps_.size());

    int32_t w = 0, h = 0;
    if (lmshao::lmrtsp::H264Parser::GetResolution(sps_buffer, w, h)) {
        width = static_cast<uint32_t>(w);
        height = static_cast<uint32_t>(h);
        return true;
    }

    // Return default values if parsing fails
    width = 1280;
    height = 720;
    return false;
}

double H264FileReader::GetDuration() const
{
    if (frame_rate_ > 0) {
        return static_cast<double>(frame_count_) / frame_rate_;
    }
    return 0.0;
}

bool H264FileReader::GetNextFrame(std::vector<uint8_t> &frame_data)
{
    MediaFrame frame;
    if (ReadFrame(frame)) {
        frame_data.clear();
        frame_data.insert(frame_data.end(), frame.data->Data(), frame.data->Data() + frame.data->Size());
        return true;
    }
    return false;
}

int H264FileReader::FindStartCode(const uint8_t *buffer, size_t start_pos, size_t buffer_size)
{
    if (buffer_size - start_pos < 3) {
        return -1;
    }

    for (size_t i = start_pos; i <= buffer_size - 3; ++i) {
        if (buffer[i] == 0x00 && buffer[i + 1] == 0x00 && buffer[i + 2] == 0x01) {
            return static_cast<int>(i);
        }
        if (i > 0 && buffer[i - 1] == 0x00 && buffer[i] == 0x00 && buffer[i + 1] == 0x00 && buffer[i + 2] == 0x01) {
            return static_cast<int>(i - 1);
        }
    }

    return -1;
}

void H264FileReader::ExtractParameterSets()
{
    if (!file_.is_open() || parameter_sets_extracted_) {
        return;
    }

    // Save current position
    auto current_pos = file_.tellg();

    // Read from beginning
    file_.seekg(0, std::ios::beg);

    std::vector<uint8_t> temp_buffer(BUFFER_SIZE);
    file_.read(reinterpret_cast<char *>(temp_buffer.data()), BUFFER_SIZE);
    size_t bytes_read = file_.gcount();

    // Use H264Parser to extract SPS and PPS with DataBuffer
    auto data_buffer = lmshao::lmcore::DataBuffer::Create(bytes_read);
    data_buffer->Assign(temp_buffer.data(), bytes_read);

    std::shared_ptr<lmshao::lmcore::DataBuffer> sps_buffer, pps_buffer;
    if (lmshao::lmrtsp::H264Parser::ExtractSPSPPS(data_buffer, sps_buffer, pps_buffer)) {
        sps_.assign(sps_buffer->Data(), sps_buffer->Data() + sps_buffer->Size());
        pps_.assign(pps_buffer->Data(), pps_buffer->Data() + pps_buffer->Size());
        std::cout << "Found SPS, size: " << sps_.size() << " bytes" << std::endl;
        std::cout << "Found PPS, size: " << pps_.size() << " bytes" << std::endl;
    }

    // Restore position
    file_.seekg(current_pos);
    parameter_sets_extracted_ = true;
}

void H264FileReader::AnalyzeFile()
{
    if (!file_.is_open()) {
        return;
    }

    // Save current position
    auto current_pos = file_.tellg();

    // Get file size
    file_.seekg(0, std::ios::end);
    size_t file_size = file_.tellg();
    file_.seekg(0, std::ios::beg);

    // Simple frame counting using H264Parser
    std::vector<uint8_t> temp_buffer(BUFFER_SIZE);
    size_t total_frames = 0;

    while (file_.good()) {
        file_.read(reinterpret_cast<char *>(temp_buffer.data()), BUFFER_SIZE);
        size_t bytes_read = file_.gcount();

        if (bytes_read == 0)
            break;

        // Create DataBuffer for parsing
        auto data_buffer = lmshao::lmcore::DataBuffer::Create(bytes_read);
        data_buffer->Assign(temp_buffer.data(), bytes_read);

        for (size_t i = 0; i < bytes_read - 4; ++i) {
            if (temp_buffer[i] == 0x00 && temp_buffer[i + 1] == 0x00 && temp_buffer[i + 2] == 0x00 &&
                temp_buffer[i + 3] == 0x01) {

                // Create a sub-buffer for this NALU
                auto nalu_buffer = lmshao::lmcore::DataBuffer::Create(bytes_read - i);
                nalu_buffer->Assign(temp_buffer.data() + i, bytes_read - i);

                int32_t nalu_type = lmshao::lmrtsp::H264Parser::GetNaluType(nalu_buffer);

                // Count I-frames and P-frames (NALU type 1 and 5)
                if (nalu_type == 1 || nalu_type == 5) {
                    total_frames++;
                }
            }
        }
    }

    frame_count_ = total_frames;

    // Estimate frame rate (default to 25fps for most videos)
    frame_rate_ = 25;

    // Restore position
    file_.seekg(current_pos);

    std::cout << "File analysis complete. File size: " << file_size << " bytes, Estimated frames: " << frame_count_
              << std::endl;
}
