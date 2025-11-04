/**
 * @author SHAO Liming <lmshao@163.com>
 * @copyright Copyright (c) 2025 SHAO Liming
 * @license MIT
 *
 * SPDX-License-Identifier: MIT
 */

#include "ts_file_reader.h"

#include <algorithm>
#include <cstring>
#include <iostream>

TSFileReader::TSFileReader(std::shared_ptr<lmshao::lmcore::MappedFile> mapped_file)
    : mapped_file_(mapped_file), current_offset_(0), total_packets_(0), estimated_bitrate_(2000000) // 2 Mbps default
{
    if (mapped_file_) {
        std::cout << "TSFileReader created for file: " << mapped_file_->Path() << ", size: " << mapped_file_->Size()
                  << " bytes" << std::endl;
        CalculateTotalPackets();
    }
}

bool TSFileReader::ReadNextPacket(std::vector<uint8_t> &packet_data)
{
    if (!mapped_file_ || IsEOF()) {
        return false;
    }

    const uint8_t *data = mapped_file_->Data();
    size_t file_size = mapped_file_->Size();

    // Find next sync byte if not aligned
    while (current_offset_ < file_size && data[current_offset_] != TS_SYNC_BYTE) {
        current_offset_++;
    }

    // Check if we have enough data for a complete packet
    if (current_offset_ + TS_PACKET_SIZE > file_size) {
        return false; // EOF
    }

    // Verify sync byte
    if (data[current_offset_] != TS_SYNC_BYTE) {
        std::cerr << "Warning: TS sync byte not found at offset " << current_offset_ << std::endl;
        return false;
    }

    // Read packet
    packet_data.resize(TS_PACKET_SIZE);
    std::memcpy(packet_data.data(), data + current_offset_, TS_PACKET_SIZE);

    current_offset_ += TS_PACKET_SIZE;

    return true;
}

void TSFileReader::Reset()
{
    current_offset_ = 0;
    std::cout << "TSFileReader reset to beginning" << std::endl;
}

bool TSFileReader::IsEOF() const
{
    if (!mapped_file_) {
        return true;
    }
    return current_offset_ >= mapped_file_->Size();
}

TSFileReader::PlaybackInfo TSFileReader::GetPlaybackInfo() const
{
    PlaybackInfo info;
    info.current_offset_ = current_offset_;
    info.total_packets_ = total_packets_;

    // Estimate duration based on bitrate
    // TS packet = 188 bytes * 8 bits/byte = 1504 bits
    // Duration = (total_bits) / bitrate
    if (estimated_bitrate_ > 0 && mapped_file_) {
        size_t total_bits = mapped_file_->Size() * 8;
        info.total_duration_ = static_cast<double>(total_bits) / estimated_bitrate_;
    } else {
        info.total_duration_ = 0.0;
    }

    return info;
}

uint32_t TSFileReader::GetBitrate() const
{
    return estimated_bitrate_;
}

void TSFileReader::CalculateTotalPackets()
{
    if (!mapped_file_) {
        total_packets_ = 0;
        return;
    }

    size_t file_size = mapped_file_->Size();

    // Find first sync byte
    const uint8_t *data = mapped_file_->Data();
    size_t offset = 0;
    while (offset < file_size && data[offset] != TS_SYNC_BYTE) {
        offset++;
    }

    if (offset >= file_size) {
        std::cerr << "Warning: No TS sync byte found in file" << std::endl;
        total_packets_ = 0;
        return;
    }

    // Calculate total packets from first sync byte to end
    size_t remaining = file_size - offset;
    total_packets_ = remaining / TS_PACKET_SIZE;

    std::cout << "TS file analysis: " << total_packets_
              << " packets, estimated size: " << (total_packets_ * TS_PACKET_SIZE) << " bytes" << std::endl;
}
