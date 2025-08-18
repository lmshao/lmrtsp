/**
 * @author SHAO Liming <lmshao@163.com>
 * @copyright Copyright (c) 2025 SHAO Liming
 * @license MIT
 *
 * SPDX-License-Identifier: MIT
 */

#include "mpegts/ts_muxer.h"

#include <algorithm>
#include <cstring>

namespace lmshao::rtsp::mpegts {

TSStreamMuxer::TSStreamMuxer()
    : video_pid_(0x1001), audio_pid_(0x1002), pcr_pid_(0x1001), bitrate_(5000000), pcr_interval_(40), video_counter_(0),
      audio_counter_(0), packet_count_(0), current_pcr_(0), running_(false)
{
}

TSStreamMuxer::~TSStreamMuxer()
{
    Stop();
}

void TSStreamMuxer::SetCallback(std::shared_ptr<TSPacketCallback> callback)
{
    callback_ = callback;
}

void TSStreamMuxer::MuxVideoData(uint16_t pid, const uint8_t *data, size_t size)
{
    if (!running_ || !data || size == 0) {
        return;
    }

    auto packets = CreatePackets(pid, data, size);

    // Send packets
    for (const auto &packet : packets) {
        if (callback_) {
            // Get raw packet data directly
            auto raw_data = packet.GetRawData();
            callback_->OnVideoData(pid, raw_data.data(), raw_data.size());
        }
    }

    // Update PCR
    if (packet_count_ % pcr_interval_ == 0) {
        GenerateAndSendPCR();
    }
}

void TSStreamMuxer::MuxAudioData(uint16_t pid, const uint8_t *data, size_t size)
{
    if (!running_ || !data || size == 0) {
        return;
    }

    auto packets = CreatePackets(pid, data, size);

    // Send packets
    for (const auto &packet : packets) {
        if (callback_) {
            // Get raw packet data directly
            auto raw_data = packet.GetRawData();
            callback_->OnAudioData(pid, raw_data.data(), raw_data.size());
        }
    }
}

void TSStreamMuxer::SetVideoPID(uint16_t pid)
{
    video_pid_ = pid;
}

void TSStreamMuxer::SetAudioPID(uint16_t pid)
{
    audio_pid_ = pid;
}

void TSStreamMuxer::SetPCRPID(uint16_t pid)
{
    pcr_pid_ = pid;
}

void TSStreamMuxer::SetBitrate(uint32_t bitrate)
{
    bitrate_ = bitrate;
}

void TSStreamMuxer::SetPCRInterval(uint32_t interval_packets)
{
    pcr_interval_ = interval_packets;
}

void TSStreamMuxer::Start()
{
    running_ = true;
    packet_count_ = 0;
    video_counter_ = 0;
    audio_counter_ = 0;

    // Generate initial control packets
    GenerateAndSendPAT();
    GenerateAndSendPMT();
}

void TSStreamMuxer::Stop()
{
    running_ = false;
}

void TSStreamMuxer::Reset()
{
    Stop();
    packet_count_ = 0;
    video_counter_ = 0;
    audio_counter_ = 0;
    current_pcr_ = 0;
}

bool TSStreamMuxer::IsRunning() const
{
    return running_;
}

size_t TSStreamMuxer::GetMuxedPackets() const
{
    return packet_count_;
}

void TSStreamMuxer::GenerateAndSendPAT()
{
    if (!callback_) {
        return;
    }

    // Create PAT packet
    TSPacket pat_packet = CreatePacket(PAT_PID, nullptr, 0, true);

    // Create PAT data (simplified implementation)
    auto pat_data = std::make_shared<coreutils::DataBuffer>();
    pat_data->Assign(PAT_TABLE_ID);     // Table ID
    pat_data->Append((uint8_t)0x80);    // Flags
    pat_data->Append((uint16_t)0x0000); // Reserved field
    pat_data->Append((uint16_t)0x0001); // Program number
    pat_data->Append((uint16_t)0x1000); // PMT PID

    pat_packet.payload = pat_data;

    // Send PAT packet
    auto raw_data = pat_packet.GetRawData();
    callback_->OnVideoData(PAT_PID, raw_data.data(), raw_data.size());
}

void TSStreamMuxer::GenerateAndSendPMT()
{
    if (!callback_) {
        return;
    }

    // Create PMT packet
    TSPacket pmt_packet = CreatePacket(PMT_PID, nullptr, 0, true);

    // Create PMT data (simplified implementation)
    auto pmt_data = std::make_shared<coreutils::DataBuffer>();
    pmt_data->Assign(PMT_TABLE_ID);     // Table ID
    pmt_data->Append((uint8_t)0x80);    // Flags
    pmt_data->Append((uint16_t)0x0000); // Reserved field
    pmt_data->Append((uint16_t)0x0001); // Program number
    pmt_data->Append((uint16_t)0x0000); // Reserved field
    pmt_data->Append((uint16_t)0x0000); // PCR PID
    pmt_data->Append((uint16_t)0x0000); // Program info length

    // Add video stream
    pmt_data->Append((uint8_t)0x1B);        // H.264 video
    pmt_data->Append((uint16_t)video_pid_); // Video PID
    pmt_data->Append((uint16_t)0x0000);     // ES info length

    // Add audio stream
    pmt_data->Append((uint8_t)0x0F);        // AAC audio
    pmt_data->Append((uint16_t)audio_pid_); // Audio PID
    pmt_data->Append((uint16_t)0x0000);     // ES info length

    pmt_packet.payload = pmt_data;

    // Send PMT packet
    auto raw_data = pmt_packet.GetRawData();
    callback_->OnVideoData(PMT_PID, raw_data.data(), raw_data.size());
}

void TSStreamMuxer::GenerateAndSendPCR()
{
    UpdatePCR();

    if (!callback_) {
        return;
    }

    // Create PCR packet
    TSPacket pcr_packet = CreatePacket(pcr_pid_, nullptr, 0, false);

    // Create PCR data (simplified implementation)
    auto pcr_data = std::make_shared<coreutils::DataBuffer>();
    pcr_data->Assign((uint8_t)0x10); // Adaptation field control

    // PCR field (simplified implementation)
    uint64_t pcr_base = current_pcr_ / 300;
    uint16_t pcr_ext = current_pcr_ % 300;

    pcr_data->Append((uint8_t)((pcr_base >> 25) & 0x1F));
    pcr_data->Append((uint8_t)((pcr_base >> 17) & 0xFF));
    pcr_data->Append((uint8_t)((pcr_base >> 9) & 0xFE));
    pcr_data->Append((uint8_t)((pcr_base >> 1) & 0xFF));
    pcr_data->Append((uint8_t)(((pcr_base & 0x01) << 7) | ((pcr_ext >> 8) & 0x01)));
    pcr_data->Append((uint8_t)(pcr_ext & 0xFF));

    pcr_packet.payload = pcr_data;

    // Send PCR packet
    auto raw_data = pcr_packet.GetRawData();
    callback_->OnVideoData(pcr_pid_, raw_data.data(), raw_data.size());
}

std::vector<TSPacket> TSStreamMuxer::CreatePackets(uint16_t pid, const uint8_t *data, size_t size)
{
    std::vector<TSPacket> packets;

    if (!data || size == 0) {
        return packets;
    }

    size_t remaining = size;
    size_t offset = 0;
    bool first_packet = true;

    while (remaining > 0) {
        size_t payload_size = std::min(remaining, TS_PAYLOAD_SIZE);

        TSPacket packet = CreatePacket(pid, data + offset, payload_size, first_packet);

        packets.push_back(packet);

        remaining -= payload_size;
        offset += payload_size;
        first_packet = false;
        packet_count_++;
    }

    return packets;
}

TSPacket TSStreamMuxer::CreatePacket(uint16_t pid, const uint8_t *payload_data, size_t payload_size, bool payload_start)
{
    TSPacket packet;

    packet.header.sync_byte = TS_SYNC_BYTE;
    packet.header.transport_error_indicator = 0;
    packet.header.payload_unit_start_indicator = payload_start ? 1 : 0;
    packet.header.transport_priority = 0;
    packet.header.pid = pid;
    packet.header.transport_scrambling_control = 0;
    packet.header.adaptation_field_control = payload_data ? 0x01 : 0x02; // Payload only or adaptation field only

    // Set continuity counter
    if (pid == video_pid_) {
        packet.header.continuity_counter = video_counter_ & 0x0F;
        video_counter_++;
    } else if (pid == audio_pid_) {
        packet.header.continuity_counter = audio_counter_ & 0x0F;
        audio_counter_++;
    } else {
        packet.header.continuity_counter = 0;
    }

    if (payload_data && payload_size > 0) {
        packet.payload = std::make_shared<coreutils::DataBuffer>(payload_size);
        packet.payload->Assign(payload_data, payload_size);
    }

    return packet;
}

void TSStreamMuxer::UpdatePCR()
{
    // Simplified PCR update, should actually be based on system clock
    current_pcr_ += 3600; // Assume 90kHz clock, increase 40ms per packet
}

} // namespace lmshao::rtsp::mpegts
