/**
 * @author SHAO Liming <lmshao@163.com>
 * @copyright Copyright (c) 2025 SHAO Liming
 * @license MIT
 *
 * SPDX-License-Identifier: MIT
 */

#include "session_ts_worker_thread.h"

#include <lmcore/data_buffer.h>

#include <algorithm>
#include <chrono>
#include <iomanip>
#include <iostream>

#include "file_manager.h"
#include "session_ts_reader.h"

SessionTSWorkerThread::SessionTSWorkerThread(std::shared_ptr<RtspServerSession> session, const std::string &file_path,
                                             uint32_t bitrate)
    : BaseSessionWorkerThread(session, file_path), bitrate_(bitrate), packet_counter_(0), use_pcr_(false), last_pcr_(0),
      packets_since_last_pcr_(0), rtp_timestamp_increment_from_pcr_(0)
{
    if (!session_) {
        std::cout << "Invalid RtspServerSession provided to SessionTSWorkerThread" << std::endl;
        return;
    }

    std::cout << "SessionTSWorkerThread created for session: " << session_id_ << ", file: " << file_path_
              << ", bitrate: " << (bitrate / 1000000.0) << " Mbps" << std::endl;
}

SessionTSWorkerThread::~SessionTSWorkerThread()
{
    std::cout << "SessionTSWorkerThread destroyed for session: " << session_id_ << std::endl;
}

bool SessionTSWorkerThread::InitializeReader()
{
    auto mapped_file = FileManager::GetInstance().GetMappedFile(file_path_);
    if (!mapped_file) {
        std::cout << "Failed to get MappedFile for: " << file_path_ << std::endl;
        return false;
    }

    ts_reader_ = std::make_unique<SessionTSReader>(mapped_file);
    packet_counter_.store(0);

    // Calculate RTP timestamp increment based on bitrate
    // TS packet = 188 bytes = 1504 bits
    // Packet interval (seconds) = packet_size_bits / bitrate
    // RTP timestamp increment = 90000 * packet_interval
    constexpr size_t TS_PACKET_SIZE = 188;                // bytes
    constexpr size_t TS_PACKET_BITS = TS_PACKET_SIZE * 8; // 1504 bits
    uint32_t bps = bitrate_.load();
    if (bps == 0) {
        bps = 2000000; // 2 Mbps default fallback
    }

    // Calculate packet interval in seconds
    double packet_interval_seconds = static_cast<double>(TS_PACKET_BITS) / bps;
    // Calculate RTP timestamp increment (90kHz clock)
    rtp_timestamp_increment_ = static_cast<uint32_t>(90000.0 * packet_interval_seconds);

    // Ensure minimum increment (avoid 0)
    if (rtp_timestamp_increment_ == 0) {
        rtp_timestamp_increment_ = 144; // Default: ~25 packets per frame at 25fps
    }

    std::cout << "TS RTP timestamp increment: " << rtp_timestamp_increment_
              << " (90kHz clock, bitrate=" << (bps / 1000000.0) << " Mbps)" << std::endl;

    return true;
}

void SessionTSWorkerThread::CleanupReader()
{
    ts_reader_.reset();
}

void SessionTSWorkerThread::ReleaseFile()
{
    if (!file_path_.empty()) {
        FileManager::GetInstance().ReleaseMappedFile(file_path_);
    }
}

SessionTSReader::PlaybackInfo SessionTSWorkerThread::GetPlaybackInfo() const
{
    if (ts_reader_) {
        return ts_reader_->GetPlaybackInfo();
    }
    return SessionTSReader::PlaybackInfo{};
}

void SessionTSWorkerThread::Reset()
{
    ResetReader();
    packet_counter_.store(0);
    use_pcr_ = false;
    last_pcr_ = 0;
    packets_since_last_pcr_ = 0;
    rtp_timestamp_increment_from_pcr_ = 0;
    std::cout << "Session " << session_id_ << " reset to beginning" << std::endl;
}

void SessionTSWorkerThread::ResetReader()
{
    if (ts_reader_) {
        ts_reader_->Reset();
    }
}

bool SessionTSWorkerThread::SendNextData()
{
    return SendNextPacket();
}

std::chrono::microseconds SessionTSWorkerThread::GetDataInterval() const
{
    constexpr size_t TS_PACKET_SIZE = 188; // Standard TS packet size

    uint32_t bps = bitrate_.load();
    if (bps == 0) {
        bps = 2000000; // 2 Mbps default fallback
    }

    // TS packet = 188 bytes = 1504 bits
    // Interval = (bits_per_packet / bits_per_second) * 1,000,000 microseconds
    double interval_us = (TS_PACKET_SIZE * 8.0 / bps) * 1000000.0;
    return std::chrono::microseconds(static_cast<long long>(interval_us));
}

bool SessionTSWorkerThread::SendNextPacket()
{
    if (!ts_reader_ || !session_) {
        return false;
    }

    std::vector<uint8_t> packet_data;
    if (!ts_reader_->ReadNextPacket(packet_data)) {
        return false; // EOF or error
    }

    // Parse TS packet to extract PCR
    TSPacketInfo packet_info;
    bool packet_valid = TSParser::ParsePacket(packet_data.data(), packet_info);

    // Calculate RTP timestamp
    uint32_t rtp_timestamp = 0;

    if (packet_valid && packet_info.has_pcr) {
        // Use PCR-based timestamping (preferred method)
        if (!use_pcr_) {
            // First PCR found, initialize PCR-based mode
            use_pcr_ = true;
            last_pcr_ = packet_info.pcr;
            packets_since_last_pcr_ = 0;
            rtp_timestamp = TSParser::PCRToRTPTimestamp(packet_info.pcr);
            std::cout << "Session " << session_id_
                      << " switched to PCR-based timestamping, initial PCR: " << packet_info.pcr
                      << " (27MHz), RTP timestamp: " << rtp_timestamp << std::endl;
        } else {
            // Check for PCR discontinuity
            if (packet_info.discontinuity || TSParser::IsPCRDiscontinuous(last_pcr_, packet_info.pcr)) {
                // PCR discontinuity detected, reset
                std::cout << "Session " << session_id_ << " PCR discontinuity detected, resetting" << std::endl;
                last_pcr_ = packet_info.pcr;
                packets_since_last_pcr_ = 0;
                rtp_timestamp = TSParser::PCRToRTPTimestamp(packet_info.pcr);
            } else {
                // Calculate RTP timestamp increment from PCR interval if we have enough packets
                if (packets_since_last_pcr_ > 0) {
                    uint32_t calculated_increment =
                        TSParser::CalculateRTPIncrementFromPCR(last_pcr_, packet_info.pcr, packets_since_last_pcr_);
                    if (calculated_increment > 0) {
                        rtp_timestamp_increment_from_pcr_ = calculated_increment;
                    }
                }

                // Use PCR directly for this packet
                rtp_timestamp = TSParser::PCRToRTPTimestamp(packet_info.pcr);
                last_pcr_ = packet_info.pcr;
                packets_since_last_pcr_ = 0;
            }
        }
    } else {
        // PCR not available, use fallback method
        if (use_pcr_ && rtp_timestamp_increment_from_pcr_ > 0) {
            // Use PCR-calculated increment
            uint64_t last_rtp_ts = TSParser::PCRToRTPTimestamp(last_pcr_);
            rtp_timestamp =
                static_cast<uint32_t>(last_rtp_ts + (packets_since_last_pcr_ * rtp_timestamp_increment_from_pcr_));
            packets_since_last_pcr_++;
        } else {
            // Fallback to bitrate-based calculation
            rtp_timestamp = static_cast<uint32_t>(packet_counter_.load() * rtp_timestamp_increment_);
        }
    }

    // Convert std::vector<uint8_t> to DataBuffer
    auto data_buffer = lmshao::lmcore::DataBuffer::Create(packet_data.size());
    data_buffer->Assign(packet_data.data(), packet_data.size());

    // Create MediaFrame for RTSP session
    lmshao::lmrtsp::MediaFrame rtsp_frame;
    rtsp_frame.data = data_buffer;
    rtsp_frame.timestamp = rtp_timestamp;
    rtsp_frame.media_type = MediaType::MP2T;

    // Send frame to session
    bool success = session_->PushFrame(rtsp_frame);

    if (success) {
        data_sent_++;
        bytes_sent_ += rtsp_frame.data->Size();
        packet_counter_++;

        // Only log every 50 packets to reduce output
        if (data_sent_.load() % 50 == 0) {
            std::cout << "Session " << session_id_ << " sent " << data_sent_.load() << " packets, "
                      << bytes_sent_.load() << " bytes";
            if (use_pcr_) {
                std::cout << ", PCR-based timestamping";
            } else {
                std::cout << ", bitrate-based timestamping";
            }
            std::cout << std::endl;
        }
    } else {
        std::cout << "Session " << session_id_ << " failed to send packet" << std::endl;
    }

    return success;
}
