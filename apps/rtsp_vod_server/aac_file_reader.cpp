/**
 * @author SHAO Liming <lmshao@163.com>
 * @copyright Copyright (c) 2025 SHAO Liming
 * @license MIT
 *
 * SPDX-License-Identifier: MIT
 */

#include "aac_file_reader.h"

#include <cstring>
#include <iostream>

using namespace lmshao::lmrtsp;

AacFileReader::AacFileReader(std::shared_ptr<lmshao::lmcore::MappedFile> mapped_file)
    : mapped_file_(mapped_file), current_offset_(0)
{
    if (!mapped_file_ || !mapped_file_->IsValid()) {
        std::cerr << "Invalid mapped file for AAC reader" << std::endl;
        return;
    }

    file_size_ = mapped_file_->Size();
    if (file_size_ < AdtsParser::ADTS_HEADER_SIZE) {
        std::cerr << "File too small to be valid AAC/ADTS: " << file_size_ << " bytes" << std::endl;
        return;
    }

    // Analyze file to extract metadata and count frames
    AnalyzeFile();
}

void AacFileReader::AnalyzeFile()
{
    playback_info_.total_frames_ = 0;
    playback_info_.total_duration_ = 0.0;
    playback_info_.current_frame_ = 0;

    size_t offset = 0;
    bool first_frame = true;

    while (offset < file_size_) {
        AdtsHeader header;
        const uint8_t *data = mapped_file_->Data() + offset;
        size_t remaining = file_size_ - offset;

        if (!AdtsParser::ParseHeader(data, remaining, header)) {
            // Try to find next sync word
            size_t next_sync = AdtsParser::FindSyncWord(data, remaining, 1);
            if (next_sync >= remaining) {
                break;
            }
            offset += next_sync;
            continue;
        }

        if (first_frame) {
            sample_rate_ = AdtsParser::GetSamplingFrequency(header.sampling_frequency_index);
            channels_ = header.channel_configuration;
            profile_ = header.profile;
            first_frame = false;

            if (sample_rate_ == 0) {
                std::cerr << "Invalid sampling frequency index: " << (int)header.sampling_frequency_index << std::endl;
                return;
            }
        }

        playback_info_.total_frames_++;
        offset += header.aac_frame_length;
    }

    if (playback_info_.total_frames_ > 0 && sample_rate_ > 0) {
        // Calculate duration using AdtsParser utility
        playback_info_.total_duration_ =
            (double)(playback_info_.total_frames_ * AdtsParser::SAMPLES_PER_AAC_FRAME) / sample_rate_;
        is_valid_ = true;

        std::cout << "AAC file analysis:" << std::endl;
        std::cout << "  Sample rate: " << sample_rate_ << " Hz" << std::endl;
        std::cout << "  Channels: " << (int)channels_ << std::endl;
        std::cout << "  Profile: " << AdtsParser::GetProfileName(profile_) << std::endl;
        std::cout << "  Total frames: " << playback_info_.total_frames_ << std::endl;
        std::cout << "  Duration: " << playback_info_.total_duration_ << " seconds" << std::endl;
    } else {
        std::cerr << "Failed to analyze AAC file: no valid frames found" << std::endl;
    }
}

bool AacFileReader::ReadNextFrame(std::vector<uint8_t> &frame_data)
{
    if (current_offset_ >= file_size_) {
        return false; // EOF
    }

    AdtsHeader header;
    const uint8_t *data = mapped_file_->Data() + current_offset_;
    size_t remaining = file_size_ - current_offset_;

    if (!AdtsParser::ParseHeader(data, remaining, header)) {
        std::cerr << "Failed to parse ADTS header at offset " << current_offset_ << std::endl;
        return false;
    }

    size_t frame_length = header.aac_frame_length;
    if (current_offset_ + frame_length > file_size_) {
        std::cerr << "Frame extends beyond file boundary" << std::endl;
        return false;
    }

    // Copy frame data (including ADTS header)
    frame_data.resize(frame_length);
    std::memcpy(frame_data.data(), data, frame_length);

    current_offset_ += frame_length;
    playback_info_.current_frame_++;

    return true;
}

void AacFileReader::Reset()
{
    current_offset_ = 0;
    playback_info_.current_frame_ = 0;
}

uint32_t AacFileReader::GetBitrate() const
{
    if (playback_info_.total_duration_ == 0.0) {
        return 0;
    }

    // Calculate average bitrate: (file_size * 8) / duration
    return static_cast<uint32_t>((file_size_ * 8.0) / playback_info_.total_duration_);
}
