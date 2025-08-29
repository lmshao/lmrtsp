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
#include <iostream>

namespace lmshao::mpegts {

TSStreamMuxer::TSStreamMuxer()
    : video_pid_(0x1001), audio_pid_(0x1002), pcr_pid_(0x1001), bitrate_(5000000), pcr_interval_(100),
      audio_sample_rate_(0), audio_channels_(0), video_counter_(0), audio_counter_(0), pat_counter_(0), pmt_counter_(0),
      packet_count_(0), current_pcr_(0), running_(false)
{
}

TSStreamMuxer::~TSStreamMuxer()
{
    Stop();
}

void TSStreamMuxer::SetCallback(std::shared_ptr<TSMuxerListener> callback)
{
    callback_ = callback;
}

void TSStreamMuxer::MuxVideoData(uint16_t pid, const uint8_t *data, size_t size, uint64_t timestamp_ms)
{
    if (!running_ || !data || size == 0) {
        return;
    }

    // Create PES packet for video data
    std::vector<uint8_t> pes_packet;

    // PES header
    pes_packet.push_back(0x00);
    pes_packet.push_back(0x00);
    pes_packet.push_back(0x01); // Start code
    pes_packet.push_back(0xE0); // Stream ID (video)

    // PES packet length - Set to 0 for video (unbounded length)
    // This is allowed for video elementary streams according to MPEG-2 TS standard
    pes_packet.push_back(0x00);
    pes_packet.push_back(0x00);

    // PES header flags
    pes_packet.push_back(0x80); // '10' + scrambling + priority + data_alignment + copyright + original
    pes_packet.push_back(
        0xC0); // PTS_DTS_flags = '11' (PTS and DTS) + ESCR + ES_rate + DSM_trick + additional_copy + CRC + extension
    pes_packet.push_back(0x0A); // PES header data length (10 bytes for PTS+DTS)

    // Convert timestamp from milliseconds to 90kHz clock (PTS units)
    uint64_t pts = timestamp_ms * 90;
    uint64_t dts = pts; // For simple cases, DTS = PTS

    // PTS encoding according to MPEG-2 standard (33-bit value in 5 bytes)
    // Format: '0011' + PTS[32:30] + '1' + PTS[29:22] + '1' + PTS[21:15] + '1' + PTS[14:7] + '1' + PTS[6:0] + '1'
    pes_packet.push_back(0x30 | ((pts >> 29) & 0x0E) | 0x01); // '0011' + PTS[32:30] + marker(1)
    pes_packet.push_back((pts >> 22) & 0xFF);                 // PTS[29:22]
    pes_packet.push_back(((pts >> 14) & 0xFE) | 0x01);        // '1' + PTS[21:15] + marker(1)
    pes_packet.push_back((pts >> 7) & 0xFF);                  // PTS[14:7]
    pes_packet.push_back(((pts << 1) & 0xFE) | 0x01);         // '1' + PTS[6:0] + marker(1)

    // DTS encoding according to MPEG-2 standard (33-bit value in 5 bytes)
    // Format: '0001' + DTS[32:30] + '1' + DTS[29:22] + '1' + DTS[21:15] + '1' + DTS[14:7] + '1' + DTS[6:0] + '1'
    pes_packet.push_back(0x10 | ((dts >> 29) & 0x0E) | 0x01); // '0001' + DTS[32:30] + marker(1)
    pes_packet.push_back((dts >> 22) & 0xFF);                 // DTS[29:22]
    pes_packet.push_back(((dts >> 14) & 0xFE) | 0x01);        // '1' + DTS[21:15] + marker(1)
    pes_packet.push_back((dts >> 7) & 0xFF);                  // DTS[14:7]
    pes_packet.push_back(((dts << 1) & 0xFE) | 0x01);         // '1' + DTS[6:0] + marker(1)

    // Add ES data
    pes_packet.insert(pes_packet.end(), data, data + size);

    auto packets = CreatePackets(pid, pes_packet.data(), pes_packet.size());

    // Send packets
    for (const auto &packet : packets) {
        if (callback_) {
            auto raw_data = packet.GetRawData();
            callback_->OnTSPacket(raw_data.data(), raw_data.size());
        }
    }

    // Update PCR based on timestamp
    current_pcr_ = pts;

    // Send PCR periodically using video PID (matching source file behavior)
    if (packet_count_ % pcr_interval_ == 0) {
        GenerateAndSendPCR();
    }
}

void TSStreamMuxer::MuxAudioData(uint16_t pid, const uint8_t *data, size_t size, uint64_t timestamp_ms)
{
    if (!running_ || !data || size == 0) {
        return;
    }

    // Parse ADTS header and update audio parameters

    if (adts_header_.Parse(data, size)) {
        int sample_rate = adts_header_.GetSampleRate();
        int channels = adts_header_.GetChannels();
        std::cout << "MuxAudioData: Parsed ADTS header. Sample rate: " << sample_rate << ", Channels: " << channels
                  << std::endl;

        if (audio_sample_rate_ != sample_rate || audio_channels_ != channels) {
            std::cout << "MuxAudioData: Audio parameters changed. Updating PMT." << std::endl;
            SetAudioParams(sample_rate, channels);
            // Regenerate PMT with new audio parameters
            GenerateAndSendPMT();
        }
    } else {
        std::cerr << "MuxAudioData: Failed to parse ADTS header." << std::endl;
        return; // Cannot proceed without parsing header
    }

    const int adts_header_size = adts_header_.GetHeaderSize();
    if (size <= adts_header_size) {
        return;
    }

    const uint8_t *frame_data = data + adts_header_size;
    size_t frame_size = size - adts_header_size;

    // Create PES packet for audio data
    std::vector<uint8_t> pes_packet;

    // PES header
    pes_packet.push_back(0x00);
    pes_packet.push_back(0x00);
    pes_packet.push_back(0x01); // Start code
    pes_packet.push_back(0xC0); // Stream ID (audio)

    // Calculate PES packet length: PES header (3 bytes) + PES header data length (5 for PTS) + ES data
    uint16_t pes_length = 3 + 5 + frame_size;
    pes_packet.push_back((pes_length >> 8) & 0xFF);
    pes_packet.push_back(pes_length & 0xFF);

    // PES header flags
    pes_packet.push_back(0x80); // '10' + scrambling + priority + data_alignment + copyright + original
    pes_packet.push_back(
        0x80); // PTS_DTS_flags = '10' (PTS only) + ESCR + ES_rate + DSM_trick + additional_copy + CRC + extension
    pes_packet.push_back(0x05); // PES header data length (5 bytes for PTS)

    // Convert timestamp from milliseconds to 90kHz clock (PTS units)
    uint64_t pts = timestamp_ms * 90;
    // PTS encoding according to MPEG-2 standard (33-bit value in 5 bytes)
    // Format: '0010' + PTS[32:30] + '1' + PTS[29:22] + '1' + PTS[21:15] + '1' + PTS[14:7] + '1' + PTS[6:0] + '1'
    pes_packet.push_back(0x21 | ((pts >> 29) & 0x0E)); // '0010' + PTS[32..30] + marker(1)
    pes_packet.push_back((pts >> 22) & 0xFF);          // PTS[29..22]
    pes_packet.push_back(0x01 | ((pts >> 14) & 0xFE)); // marker(1) + PTS[21..15] + marker(1)
    pes_packet.push_back((pts >> 7) & 0xFF);           // PTS[14..7]
    pes_packet.push_back(0x01 | ((pts << 1) & 0xFE));  // marker(1) + PTS[6..0] + marker(1)

    // Add ES data
    pes_packet.insert(pes_packet.end(), frame_data, frame_data + frame_size);

    auto packets = CreatePackets(pid, pes_packet.data(), pes_packet.size());

    // Send packets
    for (const auto &packet : packets) {
        if (callback_) {
            auto raw_data = packet.GetRawData();
            callback_->OnTSPacket(raw_data.data(), raw_data.size());
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

void TSStreamMuxer::SetAudioParams(int sample_rate, int channels)
{
    audio_sample_rate_ = sample_rate;
    audio_channels_ = channels;
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

void TSStreamMuxer::Finalize()
{
    if (running_) {
        GenerateAndSendPCR();
    }
}

void TSStreamMuxer::Reset()
{
    Stop();
    packet_count_ = 0;
    video_counter_ = 0;
    audio_counter_ = 0;
    pat_counter_ = 0;
    pmt_counter_ = 0;
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

    // Create PAT table data
    std::vector<uint8_t> pat_section;

    // Table header
    pat_section.push_back(PAT_TABLE_ID); // Table ID (0x00)
    pat_section.push_back(0xB0);         // Section syntax indicator + reserved + section length high
    pat_section.push_back(0x0D);         // Section length low (13 bytes for single program)
    pat_section.push_back(0x00);
    pat_section.push_back(0x01); // Transport stream ID
    pat_section.push_back(0xC1); // Reserved + version + current_next_indicator
    pat_section.push_back(0x00); // Section number
    pat_section.push_back(0x00); // Last section number

    // Program 1 entry
    pat_section.push_back(0x00);
    pat_section.push_back(0x01); // Program number (1)
    pat_section.push_back((PMT_PID >> 8) | 0xE0);
    pat_section.push_back(PMT_PID & 0xFF); // Reserved + PMT PID

    // Calculate and append CRC32
    uint32_t crc = CalculateCRC32(pat_section.data(), pat_section.size());
    pat_section.push_back((crc >> 24) & 0xFF);
    pat_section.push_back((crc >> 16) & 0xFF);
    pat_section.push_back((crc >> 8) & 0xFF);
    pat_section.push_back(crc & 0xFF);

    // Create PAT packet with pointer field
    std::vector<uint8_t> pat_payload;
    pat_payload.push_back(0x00); // Pointer field
    pat_payload.insert(pat_payload.end(), pat_section.begin(), pat_section.end());

    // Create TS packet
    TSPacket pat_packet = CreatePacket(PAT_PID, pat_payload.data(), pat_payload.size(), true);

    // Send PAT packet
    auto raw_data = pat_packet.GetRawData();
    callback_->OnTSPacket(raw_data.data(), raw_data.size());
}

int TSStreamMuxer::GetAACSampleRateIndex(int sample_rate)
{
    switch (sample_rate) {
        case 96000:
            return 0;
        case 88200:
            return 1;
        case 64000:
            return 2;
        case 48000:
            return 3;
        case 44100:
            return 4;
        case 32000:
            return 5;
        case 24000:
            return 6;
        case 22050:
            return 7;
        case 16000:
            return 8;
        case 12000:
            return 9;
        case 11025:
            return 10;
        case 8000:
            return 11;
        case 7350:
            return 12;
        default:
            return 15; // escape value
    }
}

void TSStreamMuxer::GenerateAndSendPMT()
{
    if (!callback_) {
        return;
    }

    // --- Elementary stream descriptors ---
    std::vector<uint8_t> es_descriptors;

    // Video stream (H.264)
    es_descriptors.push_back(0x1B); // Stream type (H.264)
    es_descriptors.push_back((video_pid_ >> 8) | 0xE0);
    es_descriptors.push_back(video_pid_ & 0xFF);
    es_descriptors.push_back(0xF0); // ES_info_length high bits
    es_descriptors.push_back(0x00); // ES_info_length low bits

    // Audio stream (AAC)
    if (audio_sample_rate_ > 0 && audio_channels_ > 0) {
        es_descriptors.push_back(0x0F); // Stream type for ADTS AAC
        es_descriptors.push_back((audio_pid_ >> 8) | 0xE0);
        es_descriptors.push_back(audio_pid_ & 0xFF);
        es_descriptors.push_back(0xF0); // ES_info_length high bits
        es_descriptors.push_back(0x00); // ES_info_length low bits (0 length)
    }

    // --- PMT section ---
    std::vector<uint8_t> pmt_section;
    pmt_section.push_back(PMT_TABLE_ID); // table_id

    // section_length will be calculated and set later
    // 9 bytes (fixed section part) + es_descriptors.size() + 4 bytes (CRC)
    uint16_t section_length = 9 + es_descriptors.size() + 4;

    pmt_section.push_back(0xB0 | ((section_length >> 8) & 0x0F));
    pmt_section.push_back(section_length & 0xFF);

    pmt_section.push_back(0x00); // program_number high
    pmt_section.push_back(0x01); // program_number low
    pmt_section.push_back(0xC1); // version_number, current_next_indicator
    pmt_section.push_back(0x00); // section_number
    pmt_section.push_back(0x00); // last_section_number

    pmt_section.push_back((pcr_pid_ >> 8) | 0xE0);
    pmt_section.push_back(pcr_pid_ & 0xFF);

    pmt_section.push_back(0xF0); // program_info_length high
    pmt_section.push_back(0x00); // program_info_length low

    // Insert ES descriptors
    pmt_section.insert(pmt_section.end(), es_descriptors.begin(), es_descriptors.end());

    // Calculate and append CRC32
    uint32_t crc = CalculateCRC32(pmt_section.data(), pmt_section.size());
    pmt_section.push_back((crc >> 24) & 0xFF);
    pmt_section.push_back((crc >> 16) & 0xFF);
    pmt_section.push_back((crc >> 8) & 0xFF);
    pmt_section.push_back(crc & 0xFF);

    // --- Create and send TS packet ---
    std::vector<uint8_t> pmt_payload;
    pmt_payload.push_back(0x00); // Pointer field
    pmt_payload.insert(pmt_payload.end(), pmt_section.begin(), pmt_section.end());

    TSPacket pmt_packet = CreatePacket(PMT_PID, pmt_payload.data(), pmt_payload.size(), true);

    auto raw_data = pmt_packet.GetRawData();
    callback_->OnTSPacket(raw_data.data(), raw_data.size());
}

void TSStreamMuxer::GenerateAndSendPCR()
{
    if (!callback_) {
        return;
    }

    // Create a packet with adaptation field containing PCR + minimal PES payload
    TSPacket pcr_packet;
    pcr_packet.header.sync_byte = 0x47;
    pcr_packet.header.transport_error_indicator = 0;
    pcr_packet.header.payload_unit_start_indicator = 0;
    pcr_packet.header.transport_priority = 0;
    pcr_packet.header.pid = pcr_pid_;
    pcr_packet.header.transport_scrambling_control = 0;
    pcr_packet.header.adaptation_field_control = 3; // Both adaptation field and payload
    pcr_packet.header.continuity_counter = video_counter_ & 0x0F;
    video_counter_++;

    // Create adaptation field with PCR
    pcr_packet.adaptation_field = std::make_shared<coreutils::DataBuffer>();
    pcr_packet.adaptation_field->Assign(static_cast<uint8_t>(0x07)); // Adaptation field length (7 bytes)
    pcr_packet.adaptation_field->Append(static_cast<uint8_t>(0x50)); // PCR flag set, other flags 0

    // PCR encoding: 42-bit value (33-bit base + 9-bit extension)
    // The PCR is specified in units of a 27 MHz clock, while PTS/DTS (and current_pcr_) are in 90 kHz.
    // The 42-bit PCR value is split into a 33-bit base and a 9-bit extension:
    // PCR = pcr_base * 300 + pcr_ext
    // pcr_base is in units of 90 kHz. pcr_ext is in units of 27 MHz (and cycles 0-299).
    // Since our current_pcr_ is already in 90 kHz units, it can be used directly as pcr_base.
    // We don't have a higher resolution clock, so pcr_ext is 0.
    uint64_t pcr_base = current_pcr_;
    uint16_t pcr_ext = 0;

    // PCR base (33 bits in 6 bytes)
    pcr_packet.adaptation_field->Append(static_cast<uint8_t>((pcr_base >> 25) & 0xFF));
    pcr_packet.adaptation_field->Append(static_cast<uint8_t>((pcr_base >> 17) & 0xFF));
    pcr_packet.adaptation_field->Append(static_cast<uint8_t>((pcr_base >> 9) & 0xFF));
    pcr_packet.adaptation_field->Append(static_cast<uint8_t>((pcr_base >> 1) & 0xFF));
    pcr_packet.adaptation_field->Append(
        static_cast<uint8_t>(((pcr_base & 0x01) << 7) | 0x7E | ((pcr_ext >> 8) & 0x01)));
    pcr_packet.adaptation_field->Append(static_cast<uint8_t>(pcr_ext & 0xFF));

    // Add minimal payload (stuffing bytes)
    pcr_packet.payload = std::make_shared<coreutils::DataBuffer>();
    size_t remaining_space = TS_PACKET_SIZE - TS_HEADER_SIZE - 8; // 8 bytes for adaptation field
    for (size_t i = 0; i < remaining_space; i++) {
        pcr_packet.payload->Append(static_cast<uint8_t>(0xFF)); // Stuffing bytes
    }

    // Send PCR packet
    auto raw_data = pcr_packet.GetRawData();
    callback_->OnTSPacket(raw_data.data(), raw_data.size());
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

    // Set continuity counter for each PID
    if (pid == video_pid_) {
        packet.header.continuity_counter = video_counter_ & 0x0F;
        video_counter_++;
    } else if (pid == audio_pid_) {
        packet.header.continuity_counter = audio_counter_ & 0x0F;
        audio_counter_++;
    } else if (pid == PAT_PID) {
        packet.header.continuity_counter = pat_counter_ & 0x0F;
        pat_counter_++;
    } else if (pid == PMT_PID) {
        packet.header.continuity_counter = pmt_counter_ & 0x0F;
        pmt_counter_++;
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
    // PCR is already updated by video timestamps in MuxVideoData
    // No additional update needed here since PCR should follow PTS
}

uint32_t TSStreamMuxer::CalculateCRC32(const uint8_t *data, size_t length)
{
    // Standard MPEG-2 CRC32 polynomial: 0x04C11DB7
    static const uint32_t crc_table[256] = {
        0x00000000, 0x04c11db7, 0x09823b6e, 0x0d4326d9, 0x130476dc, 0x17c56b6b, 0x1a864db2, 0x1e475005, 0x2608edb8,
        0x22c9f00f, 0x2f8ad6d6, 0x2b4bcb61, 0x350c9b64, 0x31cd86d3, 0x3c8ea00a, 0x384fbdbd, 0x4c11db70, 0x48d0c6c7,
        0x4593e01e, 0x4152fda9, 0x5f15adac, 0x5bd4b01b, 0x569796c2, 0x52568b75, 0x6a1936c8, 0x6ed82b7f, 0x639b0da6,
        0x675a1011, 0x791d4014, 0x7ddc5da3, 0x709f7b7a, 0x745e66cd, 0x9823b6e0, 0x9ce2ab57, 0x91a18d8e, 0x95609039,
        0x8b27c03c, 0x8fe6dd8b, 0x82a5fb52, 0x8664e6e5, 0xbe2b5b58, 0xbaea46ef, 0xb7a96036, 0xb3687d81, 0xad2f2d84,
        0xa9ee3033, 0xa4ad16ea, 0xa06c0b5d, 0xd4326d90, 0xd0f37027, 0xddb056fe, 0xd9714b49, 0xc7361b4c, 0xc3f706fb,
        0xceb42022, 0xca753d95, 0xf23a8028, 0xf6fb9d9f, 0xfbb8bb46, 0xff79a6f1, 0xe13ef6f4, 0xe5ffeb43, 0xe8bccd9a,
        0xec7dd02d, 0x34867077, 0x30476dc0, 0x3d044b19, 0x39c556ae, 0x278206ab, 0x23431b1c, 0x2e003dc5, 0x2ac12072,
        0x128e9dcf, 0x164f8078, 0x1b0ca6a1, 0x1fcdbb16, 0x018aeb13, 0x054bf6a4, 0x0808d07d, 0x0cc9cdca, 0x7897ab07,
        0x7c56b6b0, 0x71159069, 0x75d48dde, 0x6b93dddb, 0x6f52c06c, 0x6211e6b5, 0x66d0fb02, 0x5e9f46bf, 0x5a5e5b08,
        0x571d7dd1, 0x53dc6066, 0x4d9b3063, 0x495a2dd4, 0x44190b0d, 0x40d816ba, 0xaca5c697, 0xa864db20, 0xa527fdf9,
        0xa1e6e04e, 0xbfa1b04b, 0xbb60adfc, 0xb6238b25, 0xb2e29692, 0x8aad2b2f, 0x8e6c3698, 0x832f1041, 0x87ee0df6,
        0x99a95df3, 0x9d684044, 0x902b669d, 0x94ea7b2a, 0xe0b41de7, 0xe4750050, 0xe9362689, 0xedf73b3e, 0xf3b06b3b,
        0xf771768c, 0xfa325055, 0xfef34de2, 0xc6bcf05f, 0xc27dede8, 0xcf3ecb31, 0xcbffd686, 0xd5b88683, 0xd1799b34,
        0xdc3abded, 0xd8fba05a, 0x690ce0ee, 0x6dcdfd59, 0x608edb80, 0x644fc637, 0x7a089632, 0x7ec98b85, 0x738aad5c,
        0x774bb0eb, 0x4f040d56, 0x4bc510e1, 0x46863638, 0x42472b8f, 0x5c007b8a, 0x58c1663d, 0x558240e4, 0x51435d53,
        0x251d3b9e, 0x21dc2629, 0x2c9f00f0, 0x285e1d47, 0x36194d42, 0x32d850f5, 0x3f9b762c, 0x3b5a6b9b, 0x0315d626,
        0x07d4cb91, 0x0a97ed48, 0x0e56f0ff, 0x1011a0fa, 0x14d0bd4d, 0x19939b94, 0x1d528623, 0xf12f560e, 0xf5ee4bb9,
        0xf8ad6d60, 0xfc6c70d7, 0xe22b20d2, 0xe6ea3d65, 0xeba91bbc, 0xef68060b, 0xd727bbb6, 0xd3e6a601, 0xdea580d8,
        0xda649d6f, 0xc423cd6a, 0xc0e2d0dd, 0xcda1f604, 0xc960ebb3, 0xbd3e8d7e, 0xb9ff90c9, 0xb4bcb610, 0xb07daba7,
        0xae3afba2, 0xaafbe615, 0xa7b8c0cc, 0xa379dd7b, 0x9b3660c6, 0x9ff77d71, 0x92b45ba8, 0x9675461f, 0x8832161a,
        0x8cf30bad, 0x81b02d74, 0x857130c3, 0x5d8a9099, 0x594b8d2e, 0x5408abf7, 0x50c9b640, 0x4e8ee645, 0x4a4ffbf2,
        0x470cdd2b, 0x43cdc09c, 0x7b827d21, 0x7f436096, 0x7200464f, 0x76c15bf8, 0x68860bfd, 0x6c47164a, 0x61043093,
        0x65c52d24, 0x119b4be9, 0x155a565e, 0x18197087, 0x1cd86d30, 0x029f3d35, 0x065e2082, 0x0b1d065b, 0x0fdc1bec,
        0x3793a651, 0x3352bbe6, 0x3e119d3f, 0x3ad08088, 0x2497d08d, 0x2056cd3a, 0x2d15ebe3, 0x29d4f654, 0xc5a92679,
        0xc1683bce, 0xcc2b1d17, 0xc8ea00a0, 0xd6ad50a5, 0xd26c4d12, 0xdf2f6bcb, 0xdbee767c, 0xe3a1cbc1, 0xe760d676,
        0xea23f0af, 0xeee2ed18, 0xf0a5bd1d, 0xf464a0aa, 0xf9278673, 0xfde69bc4, 0x89b8fd09, 0x8d79e0be, 0x803ac667,
        0x84fbdbd0, 0x9abc8bd5, 0x9e7d9662, 0x933eb0bb, 0x97ffad0c, 0xafb010b1, 0xab710d06, 0xa6322bdf, 0xa2f33668,
        0xbcb4666d, 0xb8757bda, 0xb5365d03, 0xb1f740b4};

    uint32_t crc = 0xFFFFFFFF;
    for (size_t i = 0; i < length; i++) {
        crc = (crc << 8) ^ crc_table[((crc >> 24) ^ data[i]) & 0xFF];
    }
    return crc;
}

} // namespace lmshao::mpegts
