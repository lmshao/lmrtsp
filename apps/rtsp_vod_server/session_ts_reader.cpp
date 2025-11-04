/**
 * @author SHAO Liming <lmshao@163.com>
 * @copyright Copyright (c) 2025 SHAO Liming
 * @license MIT
 *
 * SPDX-License-Identifier: MIT
 */

#include "session_ts_reader.h"

#include <iostream>

SessionTSReader::SessionTSReader(std::shared_ptr<lmshao::lmcore::MappedFile> mapped_file) : current_packet_index_(0)
{
    ts_reader_ = std::make_unique<TSFileReader>(mapped_file);
    std::cout << "SessionTSReader created" << std::endl;
}

bool SessionTSReader::ReadNextPacket(std::vector<uint8_t> &packet_data)
{
    if (!ts_reader_) {
        return false;
    }

    if (ts_reader_->ReadNextPacket(packet_data)) {
        current_packet_index_++;
        return true;
    }

    return false;
}

void SessionTSReader::Reset()
{
    if (ts_reader_) {
        ts_reader_->Reset();
        current_packet_index_ = 0;
        std::cout << "SessionTSReader reset to beginning" << std::endl;
    }
}

SessionTSReader::PlaybackInfo SessionTSReader::GetPlaybackInfo() const
{
    PlaybackInfo info;
    info.current_packet_ = current_packet_index_;

    if (ts_reader_) {
        auto reader_info = ts_reader_->GetPlaybackInfo();
        info.total_packets_ = reader_info.total_packets_;
        info.total_duration_ = reader_info.total_duration_;
    } else {
        info.total_packets_ = 0;
        info.total_duration_ = 0.0;
    }

    return info;
}

bool SessionTSReader::IsEOF() const
{
    if (!ts_reader_) {
        return true;
    }
    return ts_reader_->IsEOF();
}

uint32_t SessionTSReader::GetBitrate() const
{
    if (!ts_reader_) {
        return 0;
    }
    return ts_reader_->GetBitrate();
}
