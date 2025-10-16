/**
 * @author SHAO Liming <lmshao@163.com>
 * @copyright Copyright (c) 2025 SHAO Liming
 * @license MIT
 *
 * SPDX-License-Identifier: MIT
 */

#include "lmrtsp/h264_file_reader.h"

#include <algorithm>
#include <cstring>
#include <fstream>
#include <iostream>

namespace lmshao::lmrtsp {

H264FileReader::H264FileReader()
    : current_nal_index_(0), width_(0), height_(0), frame_rate_(25) // Default 25fps
      ,
      duration_(0.0), is_opened_(false), loop_mode_(true)
{
}

H264FileReader::~H264FileReader()
{
    Close();
}

bool H264FileReader::Open(const std::string &filename)
{
    std::lock_guard<std::mutex> lock(mutex_);

    if (is_opened_) {
        Close();
    }

    filename_ = filename;

    // Read file into memory
    std::ifstream file(filename, std::ios::binary | std::ios::ate);
    if (!file.is_open()) {
        std::cerr << "Failed to open file: " << filename << std::endl;
        return false;
    }

    std::streamsize size = file.tellg();
    file.seekg(0, std::ios::beg);

    file_data_.resize(size);
    if (!file.read(reinterpret_cast<char *>(file_data_.data()), size)) {
        std::cerr << "Failed to read file: " << filename << std::endl;
        return false;
    }

    // Parse NAL units
    ParseFile();

    is_opened_ = true;
    current_nal_index_ = 0;

    std::cout << "Successfully opened H.264 file: " << filename << std::endl;
    std::cout << "  File size: " << size << " bytes" << std::endl;
    std::cout << "  NAL units: " << nal_units_.size() << std::endl;
    std::cout << "  Resolution: " << width_ << "x" << height_ << std::endl;
    std::cout << "  Frame rate: " << frame_rate_ << " fps" << std::endl;
    std::cout << "  Duration: " << duration_ << " seconds" << std::endl;

    return true;
}

void H264FileReader::Close()
{
    std::lock_guard<std::mutex> lock(mutex_);
    is_opened_ = false;
    file_data_.clear();
    nal_units_.clear();
    sps_.clear();
    pps_.clear();
    current_nal_index_ = 0;
}

bool H264FileReader::IsOpened() const
{
    return is_opened_.load();
}

bool H264FileReader::GetNextFrame(std::vector<uint8_t> &frame)
{
    NalUnit nal_unit;
    if (!GetNextNalUnit(nal_unit)) {
        return false;
    }

    frame = nal_unit.data;
    return true;
}

bool H264FileReader::GetNextNalUnit(NalUnit &nal_unit)
{
    std::lock_guard<std::mutex> lock(mutex_);

    if (!is_opened_ || nal_units_.empty()) {
        return false;
    }

    if (current_nal_index_ >= nal_units_.size()) {
        if (loop_mode_) {
            current_nal_index_ = 0;
        } else {
            return false;
        }
    }

    nal_unit = nal_units_[current_nal_index_++];
    return true;
}

void H264FileReader::Reset()
{
    std::lock_guard<std::mutex> lock(mutex_);
    current_nal_index_ = 0;
}

void H264FileReader::SetLoopMode(bool enable)
{
    loop_mode_ = enable;
}

std::vector<uint8_t> H264FileReader::GetSPS() const
{
    std::lock_guard<std::mutex> lock(mutex_);
    return sps_;
}

std::vector<uint8_t> H264FileReader::GetPPS() const
{
    std::lock_guard<std::mutex> lock(mutex_);
    return pps_;
}

bool H264FileReader::GetResolution(uint32_t &width, uint32_t &height) const
{
    std::lock_guard<std::mutex> lock(mutex_);
    if (width_ > 0 && height_ > 0) {
        width = width_;
        height = height_;
        return true;
    }
    return false;
}

uint32_t H264FileReader::GetFrameRate() const
{
    return frame_rate_;
}

double H264FileReader::GetDuration() const
{
    return duration_;
}

int H264FileReader::FindNalUnitStart(const uint8_t *data, size_t size, size_t start_pos)
{
    for (size_t i = start_pos; i < size - 3; ++i) {
        if (data[i] == 0x00 && data[i + 1] == 0x00 && data[i + 2] == 0x01) {
            return static_cast<int>(i);
        }
        if (i < size - 4 && data[i] == 0x00 && data[i + 1] == 0x00 && data[i + 2] == 0x00 && data[i + 3] == 0x01) {
            return static_cast<int>(i);
        }
    }
    return -1;
}

void H264FileReader::ParseSPS(const std::vector<uint8_t> &sps)
{
    if (sps.size() < 4) {
        return;
    }

    // Simple SPS parsing - this is a basic implementation
    // A complete SPS parser would be much more complex
    // For now, we'll try to extract basic resolution info

    // Look for common resolution patterns in SPS
    // This is a simplified approach - real SPS parsing would need bit-level operations
    sps_ = sps;

    // Set some default values for common resolutions
    // In a real implementation, you would parse the SPS properly
    if (sps.size() >= 8) {
        // Try to estimate resolution based on SPS size and common patterns
        if (sps.size() >= 12) {
            width_ = 1280; // Default to 720p
            height_ = 720;
        } else {
            width_ = 640; // Default to 480p
            height_ = 480;
        }
    }
}

void H264FileReader::ParseFile()
{
    if (file_data_.empty()) {
        return;
    }

    const uint8_t *data = file_data_.data();
    size_t size = file_data_.size();
    size_t pos = 0;
    uint32_t frame_count = 0;

    while (true) {
        int start_pos = FindNalUnitStart(data, size, pos);
        if (start_pos == -1) {
            break;
        }

        // Find start code length (3 or 4 bytes)
        int start_code_length = 3;
        if (start_pos > 0 && data[start_pos - 1] == 0x00) {
            start_code_length = 4;
        }

        // Find next NAL unit
        int next_start = FindNalUnitStart(data, size, start_pos + start_code_length);
        if (next_start == -1) {
            next_start = static_cast<int>(size);
        }

        // Extract NAL unit data (excluding start code)
        int nal_data_start = start_pos + start_code_length;
        int nal_data_size = next_start - nal_data_start;

        if (nal_data_size > 0) {
            NalUnit nal_unit;
            nal_unit.data.resize(nal_data_size);
            std::memcpy(nal_unit.data.data(), data + nal_data_start, nal_data_size);

            // Get NAL unit type from first byte (after removing forbidden_zero_bit)
            uint8_t nal_header = nal_unit.data[0];
            NalUnitType type = static_cast<NalUnitType>(nal_header & 0x1F);
            nal_unit.type = type;

            // Check if this is a keyframe (IDR slice)
            nal_unit.is_keyframe = (type == NalUnitType::SLICE_IDR);

            // Set timestamp (simple incremental approach)
            nal_unit.timestamp = frame_count * 90000 / frame_rate_; // 90kHz clock

            // Store SPS and PPS
            if (type == NalUnitType::SPS) {
                sps_ = nal_unit.data;
                ParseSPS(sps_);
            } else if (type == NalUnitType::PPS) {
                pps_ = nal_unit.data;
            }

            nal_units_.push_back(nal_unit);

            // Count frame boundaries (slices)
            if (type == NalUnitType::SLICE || type == NalUnitType::SLICE_IDR) {
                frame_count++;
            }
        }

        pos = next_start;
    }

    // Estimate duration
    if (frame_count > 0) {
        duration_ = static_cast<double>(frame_count) / frame_rate_;
    }

    std::cout << "Parsed " << nal_units_.size() << " NAL units, " << frame_count << " frames" << std::endl;
}

std::vector<uint8_t> H264FileReader::ConvertToLengthPrefix(const uint8_t *data, size_t size)
{
    std::vector<uint8_t> result;

    // Convert start codes to length prefixes
    size_t pos = 0;
    while (pos < size) {
        int start_pos = FindNalUnitStart(data, size, pos);
        if (start_pos == -1) {
            // Copy remaining data
            result.insert(result.end(), data + pos, data + size);
            break;
        }

        // Copy data before start code
        result.insert(result.end(), data + pos, data + start_pos);

        // Find end of NAL unit
        int start_code_length = 3;
        if (start_pos > 0 && data[start_pos - 1] == 0x00) {
            start_code_length = 4;
        }

        int nal_data_start = start_pos + start_code_length;
        int next_start = FindNalUnitStart(data, size, nal_data_start);
        if (next_start == -1) {
            next_start = static_cast<int>(size);
        }

        int nal_data_size = next_start - nal_data_start;

        // Write length prefix (4 bytes, big endian)
        uint32_t length = static_cast<uint32_t>(nal_data_size);
        result.push_back((length >> 24) & 0xFF);
        result.push_back((length >> 16) & 0xFF);
        result.push_back((length >> 8) & 0xFF);
        result.push_back(length & 0xFF);

        // Write NAL unit data
        result.insert(result.end(), data + nal_data_start, data + next_start);

        pos = next_start;
    }

    return result;
}

} // namespace lmshao::lmrtsp