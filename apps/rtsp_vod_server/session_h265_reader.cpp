/**
 * @author SHAO Liming <lmshao@163.com>
 * @copyright Copyright (c) 2025 SHAO Liming
 * @license MIT
 *
 * SPDX-License-Identifier: MIT
 */

#include "session_h265_reader.h"

#include <algorithm>
#include <cstring>
#include <iomanip>
#include <iostream>

SessionH265Reader::SessionH265Reader(std::shared_ptr<lmshao::lmcore::MappedFile> mapped_file)
    : mapped_file_(mapped_file), current_offset_(0), current_frame_index_(0), current_timestamp_(0.0),
      index_built_(false), frame_rate_(25), parameter_sets_extracted_(false)
{
    if (!mapped_file_) {
        std::cout << "Invalid MappedFile instance" << std::endl;
        return;
    }

    std::cout << "SessionH265Reader created for file: " << mapped_file_->Path() << ", size: " << mapped_file_->Size()
              << " bytes" << std::endl;
}

bool SessionH265Reader::ReadNextFrame(LocalMediaFrameH265 &frame)
{
    std::vector<uint8_t> frame_data;
    if (!ReadNextFrame(frame_data)) {
        return false;
    }

    frame.data = std::move(frame_data);
    frame.timestamp = static_cast<uint64_t>(current_timestamp_ * 1000);
    frame.is_keyframe = false;

    // Check if this is a keyframe (IDR/CRA frame)
    if (!frame.data.empty() && frame.data.size() > 4) {
        // H.265 NAL unit type is in bits [1-6] of the first byte after start code
        uint8_t nalu_type = (frame.data[4] >> 1) & 0x3F;
        frame.is_keyframe = (nalu_type >= 19 && nalu_type <= 21); // IDR or CRA
    }

    return true;
}

bool SessionH265Reader::ReadNextFrame(std::vector<uint8_t> &frame_data)
{
    if (!mapped_file_ || IsEOF()) {
        return false;
    }

    size_t nalu_start, nalu_size;
    uint8_t nalu_type;
    if (!FindNextNALU(current_offset_, nalu_start, nalu_size, nalu_type)) {
        return false;
    }

    const uint8_t *data = mapped_file_->Data();
    frame_data.assign(data + nalu_start, data + nalu_start + nalu_size);

    current_offset_ = nalu_start + nalu_size;
    current_frame_index_++;
    current_timestamp_ = current_frame_index_ / static_cast<double>(frame_rate_);

    std::cout << "Session read frame " << current_frame_index_ << ", size: " << nalu_size << " bytes"
              << ", timestamp: " << std::fixed << std::setprecision(2) << current_timestamp_ << "s"
              << ", NALU type: " << static_cast<int>(nalu_type) << std::endl;

    return true;
}

bool SessionH265Reader::SeekToFrame(size_t frame_index)
{
    if (!index_built_) {
        BuildFrameIndex();
    }

    if (frame_index >= frame_index_.size()) {
        std::cout << "Frame index " << frame_index << " out of range (total: " << frame_index_.size() << ")"
                  << std::endl;
        return false;
    }

    const auto &frame_info = frame_index_[frame_index];
    current_offset_ = frame_info.offset_;
    current_frame_index_ = frame_index;
    current_timestamp_ = frame_info.timestamp_;

    std::cout << "Session seeked to frame " << frame_index << ", offset: " << current_offset_
              << ", timestamp: " << std::fixed << std::setprecision(2) << current_timestamp_ << "s" << std::endl;

    return true;
}

bool SessionH265Reader::SeekToTime(double timestamp)
{
    if (!index_built_) {
        BuildFrameIndex();
    }

    auto it = std::lower_bound(frame_index_.begin(), frame_index_.end(), timestamp,
                               [](const FrameInfo &frame, double ts) { return frame.timestamp_ < ts; });

    if (it == frame_index_.end()) {
        return false;
    }

    size_t frame_index = std::distance(frame_index_.begin(), it);
    return SeekToFrame(frame_index);
}

void SessionH265Reader::Reset()
{
    current_offset_ = 0;
    current_frame_index_ = 0;
    current_timestamp_ = 0.0;

    std::cout << "Session reset to beginning" << std::endl;
}

SessionH265Reader::PlaybackInfo SessionH265Reader::GetPlaybackInfo() const
{
    if (!index_built_) {
        BuildFrameIndex();
    }

    PlaybackInfo info;
    info.current_offset_ = current_offset_;
    info.current_frame_ = current_frame_index_;
    info.current_time_ = current_timestamp_;
    info.total_frames_ = frame_index_.size();
    info.total_duration_ = frame_index_.empty() ? 0.0 : frame_index_.back().timestamp_;

    return info;
}

bool SessionH265Reader::IsEOF() const
{
    return current_offset_ >= mapped_file_->Size();
}

std::vector<uint8_t> SessionH265Reader::GetVPS() const
{
    if (!parameter_sets_extracted_) {
        ExtractParameterSets();
    }
    return vps_;
}

std::vector<uint8_t> SessionH265Reader::GetSPS() const
{
    if (!parameter_sets_extracted_) {
        ExtractParameterSets();
    }
    return sps_;
}

std::vector<uint8_t> SessionH265Reader::GetPPS() const
{
    if (!parameter_sets_extracted_) {
        ExtractParameterSets();
    }
    return pps_;
}

uint32_t SessionH265Reader::GetFrameRate() const
{
    if (!parameter_sets_extracted_) {
        ExtractParameterSets();
    }
    return frame_rate_;
}

int SessionH265Reader::FindStartCode(const uint8_t *data, size_t start_pos, size_t data_size)
{
    if (start_pos + 3 >= data_size) {
        return -1;
    }

    for (size_t i = start_pos; i <= data_size - 4; ++i) {
        if (data[i] == 0x00 && data[i + 1] == 0x00) {
            if (data[i + 2] == 0x00 && data[i + 3] == 0x01) {
                return static_cast<int>(i);
            } else if (data[i + 2] == 0x01) {
                return static_cast<int>(i);
            }
        }
    }

    return -1;
}

bool SessionH265Reader::FindNextNALU(size_t start_offset, size_t &nalu_start, size_t &nalu_size, uint8_t &nalu_type)
{
    const uint8_t *data = mapped_file_->Data();
    size_t file_size = mapped_file_->Size();

    if (start_offset >= file_size) {
        return false;
    }

    int start_code_pos = FindStartCode(data, start_offset, file_size);
    if (start_code_pos == -1) {
        return false;
    }

    nalu_start = static_cast<size_t>(start_code_pos);
    size_t start_code_len = (data[nalu_start + 2] == 0x00) ? 4 : 3;

    if (nalu_start + start_code_len + 1 < file_size) {
        // H.265 NAL unit type is in bits [1-6] of the first byte
        nalu_type = (data[nalu_start + start_code_len] >> 1) & 0x3F;
    } else {
        return false;
    }

    int next_start_pos = FindStartCode(data, nalu_start + start_code_len, file_size);
    if (next_start_pos == -1) {
        nalu_size = file_size - nalu_start;
    } else {
        nalu_size = static_cast<size_t>(next_start_pos) - nalu_start;
    }

    return true;
}

void SessionH265Reader::BuildFrameIndex() const
{
    if (index_built_) {
        return;
    }

    std::cout << "Building frame index for file: " << mapped_file_->Path() << std::endl;

    frame_index_.clear();
    size_t offset = 0;
    size_t frame_count = 0;

    while (offset < mapped_file_->Size()) {
        size_t nalu_start, nalu_size;
        uint8_t nalu_type;

        SessionH265Reader *non_const_this = const_cast<SessionH265Reader *>(this);
        if (!non_const_this->FindNextNALU(offset, nalu_start, nalu_size, nalu_type)) {
            break;
        }

        // H.265 frame NALU types: 0-9 (VCL NAL units), 16-21 (IDR/CRA/BLA)
        if ((nalu_type >= 0 && nalu_type <= 9) || (nalu_type >= 16 && nalu_type <= 21)) {
            FrameInfo frame_info;
            frame_info.offset_ = nalu_start;
            frame_info.size_ = nalu_size;
            frame_info.timestamp_ = frame_count / static_cast<double>(frame_rate_);
            frame_info.is_keyframe_ = (nalu_type >= 19 && nalu_type <= 21);
            frame_info.nalu_type_ = nalu_type;

            frame_index_.push_back(frame_info);
            frame_count++;
        }

        offset = nalu_start + nalu_size;
    }

    index_built_ = true;
    std::cout << "Frame index built: " << frame_index_.size() << " frames, duration: " << std::fixed
              << std::setprecision(2) << (frame_index_.empty() ? 0.0 : frame_index_.back().timestamp_) << "s"
              << std::endl;
}

void SessionH265Reader::ExtractParameterSets() const
{
    if (parameter_sets_extracted_) {
        return;
    }

    std::cout << "Extracting parameter sets from file: " << mapped_file_->Path() << std::endl;

    const uint8_t *data = mapped_file_->Data();
    size_t file_size = mapped_file_->Size();
    size_t offset = 0;

    while (offset < std::min(file_size, static_cast<size_t>(64 * 1024))) {
        size_t nalu_start, nalu_size;
        uint8_t nalu_type;

        SessionH265Reader *non_const_this = const_cast<SessionH265Reader *>(this);
        if (!non_const_this->FindNextNALU(offset, nalu_start, nalu_size, nalu_type)) {
            break;
        }

        if (nalu_type == 32) { // VPS
            vps_.assign(data + nalu_start, data + nalu_start + nalu_size);
            std::cout << "Found VPS, size: " << nalu_size << " bytes" << std::endl;
        } else if (nalu_type == 33) { // SPS
            sps_.assign(data + nalu_start, data + nalu_start + nalu_size);
            std::cout << "Found SPS, size: " << nalu_size << " bytes" << std::endl;
        } else if (nalu_type == 34) { // PPS
            pps_.assign(data + nalu_start, data + nalu_start + nalu_size);
            std::cout << "Found PPS, size: " << nalu_size << " bytes" << std::endl;
        }

        offset = nalu_start + nalu_size;

        if (!vps_.empty() && !sps_.empty() && !pps_.empty()) {
            break;
        }
    }

    parameter_sets_extracted_ = true;

    if (vps_.empty()) {
        std::cout << "No VPS found in file: " << mapped_file_->Path() << std::endl;
    }
    if (sps_.empty()) {
        std::cout << "No SPS found in file: " << mapped_file_->Path() << std::endl;
    }
    if (pps_.empty()) {
        std::cout << "No PPS found in file: " << mapped_file_->Path() << std::endl;
    }
}

bool SessionH265Reader::SeekToOffset(size_t offset)
{
    if (offset >= mapped_file_->Size()) {
        return false;
    }

    current_offset_ = offset;

    if (!index_built_) {
        BuildFrameIndex();
    }

    for (size_t i = 0; i < frame_index_.size(); ++i) {
        if (frame_index_[i].offset_ >= offset) {
            current_frame_index_ = i;
            current_timestamp_ = frame_index_[i].timestamp_;
            break;
        }
    }

    return true;
}
