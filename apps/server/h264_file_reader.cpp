/**
 * @author SHAO Liming <lmshao@163.com>
 * @copyright Copyright (c) 2025 SHAO Liming
 * @license MIT
 *
 * SPDX-License-Identifier: MIT
 */

#include "h264_file_reader.h"

#include <iostream>

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

    frame.data.clear();

    // Find next NAL unit start code
    while (true) {
        // Ensure we have enough data in buffer
        if (buffer_end_ - buffer_pos_ < 4 && !eof_reached_) {
            // Move remaining data to beginning of buffer
            if (buffer_pos_ < buffer_end_) {
                std::memmove(read_buffer_.data(), read_buffer_.data() + buffer_pos_, buffer_end_ - buffer_pos_);
                buffer_end_ -= buffer_pos_;
            } else {
                buffer_end_ = 0;
            }
            buffer_pos_ = 0;

            // Read more data
            file_.read(reinterpret_cast<char *>(read_buffer_.data() + buffer_end_), BUFFER_SIZE - buffer_end_);
            size_t bytes_read = file_.gcount();
            buffer_end_ += bytes_read;

            if (bytes_read == 0) {
                eof_reached_ = true;
                if (buffer_end_ == 0) {
                    return false;
                }
            }
        }

        if (buffer_end_ - buffer_pos_ < 4) {
            return false;
        }

        // Look for start code (0x00000001 or 0x000001)
        int start_code_pos = FindStartCode(read_buffer_.data(), buffer_pos_, buffer_end_);
        if (start_code_pos < 0) {
            // No start code found, move to end of buffer
            buffer_pos_ = buffer_end_;
            if (eof_reached_) {
                return false;
            }
            continue;
        }

        // Skip the current start code if we're at it
        if (start_code_pos == static_cast<int>(buffer_pos_)) {
            // Determine start code length (3 or 4 bytes)
            int start_code_len = 3;
            if (start_code_pos > 0 && read_buffer_[start_code_pos - 1] == 0x00) {
                start_code_len = 4;
            }
            buffer_pos_ = start_code_pos + start_code_len;

            // If this is the first frame, continue to find the actual NALU data
            if (frame.data.empty()) {
                continue;
            }
        }

        // Find next start code to determine NALU end
        int next_start_code_pos = FindStartCode(read_buffer_.data(), buffer_pos_, buffer_end_);

        size_t nalu_end;
        if (next_start_code_pos >= 0) {
            nalu_end = next_start_code_pos;
        } else {
            nalu_end = buffer_end_;
        }

        // Copy NALU data to frame
        if (nalu_end > buffer_pos_) {
            size_t nalu_size = nalu_end - buffer_pos_;
            frame.data.reserve(frame.data.size() + nalu_size);
            frame.data.insert(frame.data.end(), read_buffer_.data() + buffer_pos_, read_buffer_.data() + nalu_end);
        }

        if (next_start_code_pos >= 0) {
            // Found complete NALU
            buffer_pos_ = next_start_code_pos;
            break;
        } else {
            // Need more data
            buffer_pos_ = buffer_end_;
            if (eof_reached_) {
                // This is the last NALU
                break;
            }
        }
    }

    if (frame.data.empty()) {
        return false;
    }

    // Set marker bit for complete frame (simplified logic)
    frame.marker = true;

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

int H264FileReader::FindStartCode(const uint8_t *buffer, size_t start_pos, size_t buffer_size)
{
    if (buffer_size - start_pos < 3) {
        return -1;
    }

    for (size_t i = start_pos; i <= buffer_size - 3; ++i) {
        if (buffer[i] == 0x00 && buffer[i + 1] == 0x00 && buffer[i + 2] == 0x01) {
            return i;
        }
        if (i > 0 && buffer[i - 1] == 0x00 && buffer[i] == 0x00 && buffer[i + 1] == 0x00 && buffer[i + 2] == 0x01) {
            return i - 1;
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

    // Look for SPS and PPS
    for (size_t i = 0; i < bytes_read - 4; ++i) {
        if (temp_buffer[i] == 0x00 && temp_buffer[i + 1] == 0x00 && temp_buffer[i + 2] == 0x00 &&
            temp_buffer[i + 3] == 0x01) {

            uint8_t nalu_type = temp_buffer[i + 4] & 0x1F;

            // Find end of this NALU
            size_t nalu_start = i + 4;
            size_t nalu_end = bytes_read;

            for (size_t j = nalu_start + 1; j < bytes_read - 3; ++j) {
                if (temp_buffer[j] == 0x00 && temp_buffer[j + 1] == 0x00 && temp_buffer[j + 2] == 0x00 &&
                    temp_buffer[j + 3] == 0x01) {
                    nalu_end = j;
                    break;
                }
            }

            if (nalu_type == 7) { // SPS
                sps_.assign(temp_buffer.begin() + nalu_start, temp_buffer.begin() + nalu_end);
                std::cout << "Found SPS, size: " << sps_.size() << " bytes" << std::endl;
            } else if (nalu_type == 8) { // PPS
                pps_.assign(temp_buffer.begin() + nalu_start, temp_buffer.begin() + nalu_end);
                std::cout << "Found PPS, size: " << pps_.size() << " bytes" << std::endl;
            }
        }
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

    // Simple frame counting by looking for I-frames and P-frames
    std::vector<uint8_t> temp_buffer(BUFFER_SIZE);
    size_t total_frames = 0;

    while (file_.good()) {
        file_.read(reinterpret_cast<char *>(temp_buffer.data()), BUFFER_SIZE);
        size_t bytes_read = file_.gcount();

        if (bytes_read == 0)
            break;

        for (size_t i = 0; i < bytes_read - 4; ++i) {
            if (temp_buffer[i] == 0x00 && temp_buffer[i + 1] == 0x00 && temp_buffer[i + 2] == 0x00 &&
                temp_buffer[i + 3] == 0x01) {

                uint8_t nalu_type = temp_buffer[i + 4] & 0x1F;

                // Count I-frames and P-frames
                if (nalu_type == 1 || nalu_type == 5) { // P-frame or I-frame
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
