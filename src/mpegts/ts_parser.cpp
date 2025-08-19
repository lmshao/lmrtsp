/**
 * @author SHAO Liming <lmshao@163.com>
 * @copyright Copyright (c) 2025 SHAO Liming
 * @license MIT
 *
 * SPDX-License-Identifier: MIT
 */

#include "mpegts/ts_parser.h"

#include <algorithm>
#include <cstring>

#include "rtsp_log.h"

namespace lmshao::rtsp::mpegts {

TSStreamParser::TSStreamParser() : running_(false), parsed_packets_(0), pcr_parsing_enabled_(true) {}

TSStreamParser::~TSStreamParser()
{
    Stop();
}

void TSStreamParser::SetCallback(std::shared_ptr<TSDemuxerListener> callback)
{
    callback_ = callback;
}

void TSStreamParser::ParseData(const uint8_t *data, size_t size)
{
    if (!running_ || !data || size == 0) {
        return;
    }

    // Parse data by TS packet size
    size_t offset = 0;
    while (offset + TS_PACKET_SIZE <= size) {
        // Find sync byte
        if (data[offset] != TS_SYNC_BYTE) {
            if (callback_) {
                callback_->OnSyncLoss();
            }
            offset++;
            continue;
        }

        // Parse TS header using static method
        TSPacket packet;
        packet.header = TSHeader::ParseFromData(data + offset);

        // Calculate actual payload size (considering adaptation field)
        size_t payload_start = TS_HEADER_SIZE;
        size_t payload_size = TS_PACKET_SIZE - TS_HEADER_SIZE;

        // Parse adaptation field if present
        if (packet.header.adaptation_field_control == 2 || packet.header.adaptation_field_control == 3) {
            uint8_t adaptation_field_length = data[offset + TS_HEADER_SIZE];

            // Store adaptation field data (including length byte)
            if (adaptation_field_length > 0) {
                size_t af_total_size = 1 + adaptation_field_length; // length byte + actual data
                packet.adaptation_field = std::make_shared<coreutils::DataBuffer>(af_total_size);
                packet.adaptation_field->Assign(data + offset + TS_HEADER_SIZE, af_total_size);
            }

            payload_start += 1 + adaptation_field_length;
            if (payload_start >= TS_PACKET_SIZE) {
                payload_size = 0;
            } else {
                payload_size = TS_PACKET_SIZE - payload_start;
            }
        }

        // Parse payload (only when payload exists)
        if (payload_size > 0 &&
            (packet.header.adaptation_field_control == 1 || packet.header.adaptation_field_control == 3)) {
            packet.payload = std::make_shared<coreutils::DataBuffer>(payload_size);
            packet.payload->Assign(data + offset + payload_start, payload_size);
        }

        ParsePacket(packet);
        offset += TS_PACKET_SIZE;
    }
}

void TSStreamParser::ParsePacket(const TSPacket &packet)
{
    if (!running_) {
        return;
    }

    parsed_packets_++;

    uint16_t pid = packet.header.pid;
    bool payload_start = packet.header.payload_unit_start_indicator;

    // Process PAT
    if (pid == PAT_PID) {
        if (payload_start && packet.payload && packet.payload->Size() > 0) {
            ProcessPAT(packet.payload);
        }
        return;
    }

    // Process PMT (dynamic PID)
    if (pmt_pids_.find(pid) != pmt_pids_.end()) {
        if (payload_start && packet.payload && packet.payload->Size() > 0) {
            ProcessPMT(pid, packet.payload);
        }
        return;
    }

    // Process PCR (check adaptation field)
    if (pcr_parsing_enabled_ && HasPCR(packet)) {
        uint64_t pcr = ExtractPCR(packet);
        if (pcr > 0) {
            ProcessPCR(pid, pcr);
        }
    }

    // Process stream data
    if (packet.payload && packet.payload->Size() > 0) {
        // Progress tracking every 5000 packets
        if (parsed_packets_ % 5000 == 0) {
            RTSP_LOGD("Processed %zu packets", parsed_packets_);
        }
        ProcessStreamData(pid, packet.payload);
    }
}

void TSStreamParser::Start()
{
    running_ = true;
}

void TSStreamParser::Stop()
{
    running_ = false;
}

void TSStreamParser::Reset()
{
    Stop();
    stream_buffers_.clear();
    last_pcr_.clear();
    pmt_pids_.clear();
    parsed_packets_ = 0;
}

void TSStreamParser::EnableVideoStream(uint16_t pid, bool enable)
{
    if (enable) {
        enabled_video_pids_.insert(pid);
    } else {
        enabled_video_pids_.erase(pid);
    }
}

void TSStreamParser::EnableAudioStream(uint16_t pid, bool enable)
{
    if (enable) {
        enabled_audio_pids_.insert(pid);
    } else {
        enabled_audio_pids_.erase(pid);
    }
}

void TSStreamParser::EnablePCRParsing(bool enable)
{
    pcr_parsing_enabled_ = enable;
}

bool TSStreamParser::IsRunning() const
{
    return running_;
}

size_t TSStreamParser::GetParsedPackets() const
{
    return parsed_packets_;
}

std::vector<uint16_t> TSStreamParser::GetActiveVideoPIDs() const
{
    std::vector<uint16_t> pids;
    for (uint16_t pid : enabled_video_pids_) {
        if (stream_buffers_.find(pid) != stream_buffers_.end()) {
            pids.push_back(pid);
        }
    }
    return pids;
}

std::vector<uint16_t> TSStreamParser::GetActiveAudioPIDs() const
{
    std::vector<uint16_t> pids;
    for (uint16_t pid : enabled_audio_pids_) {
        if (stream_buffers_.find(pid) != stream_buffers_.end()) {
            pids.push_back(pid);
        }
    }
    return pids;
}

void TSStreamParser::ProcessPAT(const std::shared_ptr<coreutils::DataBuffer> &data)
{
    if (!callback_) {
        return;
    }

    /*
     * PAT (Program Association Table) structure parsing
     *
     * PAT Table Structure (after pointer_field):
     *
     * Header Section (8 bytes):
     *   Byte 0:     table_id (always 0x00 for PAT)
     *   Byte 1:     section_syntax_indicator (1 bit) + reserved (1 bit) + section_length high 4 bits
     *   Byte 2:     section_length low 8 bits (total 12 bits for section_length)
     *   Byte 3-4:   transport_stream_id (16 bits)
     *   Byte 5:     reserved (2 bits) + version_number (5 bits) + current_next_indicator (1 bit)
     *   Byte 6:     section_number (8 bits)
     *   Byte 7:     last_section_number (8 bits)
     *
     * Program Entries Section (variable length):
     *   Each program entry is 4 bytes:
     *     Byte 0-1: program_number (16 bits)
     *     Byte 2-3: reserved (3 bits) + program_map_PID (13 bits)
     *
     *   Number of programs = (section_length - 9) / 4
     *   (9 = header after section_length field + CRC32)
     *
     * CRC Section (4 bytes):
     *   Last 4 bytes: CRC32 checksum
     *
     * Notes:
     *   - If program_number is 0, the PID refers to the network PID (not a program)
     *   - All multi-byte fields are in big-endian format
     *   - Program entries start at byte 8 and continue until CRC32
     */

    const uint8_t *pat_data = data->Data();
    size_t offset = 0;

    // Handle pointer_field (offset to first section)
    if (data->Size() > 0) {
        uint8_t pointer_field = pat_data[0];
        offset = 1 + pointer_field; // Skip pointer_field, locate to table_id
    }

    // Validate data length
    if (offset >= data->Size() || data->Size() - offset < PAT_TABLE_HEADER_SIZE) {
        RTSP_LOGE("PAT: Data too small after pointer field (%zu bytes available, need %zu)", data->Size() - offset,
                  PAT_TABLE_HEADER_SIZE);
        return;
    }

    const uint8_t *table_data = pat_data + offset;

    // Validate table_id (PAT table fixed to 0x00)
    if (table_data[0] != PAT_TABLE_ID) {
        RTSP_LOGE("PAT: Wrong table ID 0x%02X, expected 0x%02X", table_data[0], PAT_TABLE_ID);
        return;
    }

    // Parse section_length (12 bits, from low 4 bits of byte 1 and byte 2)
    uint16_t section_length = ((table_data[1] & 0x0F) << 8) | table_data[2];
    if (section_length < 5) { // Minimum length: header(8) + 1 program entry(4) + CRC(4) - 3 = 5
        RTSP_LOGD("PAT: Section too short (minimum 5 bytes required)");
        return;
    }

    // Parse PAT table header information (commented out unused variables)
    // uint16_t transport_stream_id = (table_data[3] << 8) | table_data[4];
    // uint8_t version_number = (table_data[5] >> 1) & 0x1F;
    // bool current_next_indicator = table_data[5] & 0x01;
    // uint8_t section_number = table_data[6];
    // uint8_t last_section_number = table_data[7];

    // Parse program entries
    std::vector<uint16_t> program_pids;
    size_t program_data_start = PAT_TABLE_HEADER_SIZE;           // Skip table header (8 bytes)
    size_t program_data_end = 3 + section_length - PAT_CRC_SIZE; // Subtract CRC (4 bytes)

    size_t entry_offset = program_data_start;
    while (entry_offset + PAT_PROGRAM_ENTRY_SIZE <= program_data_end &&
           offset + entry_offset + PAT_PROGRAM_ENTRY_SIZE <= data->Size()) {

        // Parse program entry (4 bytes)
        uint16_t program_number = (table_data[entry_offset] << 8) | table_data[entry_offset + 1];
        uint16_t program_pid = ((table_data[entry_offset + 2] & 0x1F) << 8) | table_data[entry_offset + 3];

        // program_number = 0 indicates network PID, not an actual program
        if (program_number != 0) {
            program_pids.push_back(program_pid);
            pmt_pids_.insert(program_pid); // Add to PMT PID set
        }

        entry_offset += PAT_PROGRAM_ENTRY_SIZE;
    }

    RTSP_LOGD("PAT: Found %zu programs", program_pids.size());

    if (callback_) {
        callback_->OnPATReceived(program_pids);
    }
}

void TSStreamParser::ProcessPMT(uint16_t pid, const std::shared_ptr<coreutils::DataBuffer> &data)
{
    /*
     * PMT (Program Map Table) structure parsing
     *
     * PMT Table Structure (after pointer_field):
     *
     * Header Section (12 bytes minimum):
     *   Byte 0:     table_id (always 0x02 for PMT)
     *   Byte 1:     section_syntax_indicator (1 bit) + reserved (1 bit) + section_length high 4 bits
     *   Byte 2:     section_length low 8 bits (total 12 bits for section_length)
     *   Byte 3-4:   program_number (16 bits)
     *   Byte 5:     reserved (2 bits) + version_number (5 bits) + current_next_indicator (1 bit)
     *   Byte 6:     section_number (8 bits, usually 0x00)
     *   Byte 7:     last_section_number (8 bits, usually 0x00)
     *   Byte 8-9:   PCR_PID (reserved 3 bits + 13 bits PID)
     *   Byte 10-11: program_info_length (reserved 4 bits + 12 bits length)
     *
     * Program Descriptors Section (variable length):
     *   Length = program_info_length bytes
     *   Contains program-level descriptors (CA, copyright, etc.)
     *
     * Elementary Stream Info Section (variable length):
     *   Each stream entry is at least 5 bytes:
     *     Byte 0:   stream_type (8 bits) - codec type (0x1B=H.264, 0x0F=AAC, etc.)
     *     Byte 1-2: elementary_PID (reserved 3 bits + 13 bits PID)
     *     Byte 3-4: ES_info_length (reserved 4 bits + 12 bits length)
     *     Byte 5+:  ES descriptors (length = ES_info_length)
     *
     * CRC Section (4 bytes):
     *   Last 4 bytes: CRC32 checksum
     *
     * Magic Numbers:
     *   0x02: PMT table_id
     *   0x1F: Mask for 13-bit PID extraction (0001 1111)
     *   0x0F: Mask for 12-bit length extraction (0000 1111)
     *   12:   Minimum PMT header size (bytes 0-11)
     *   4:    CRC32 size
     *   5:    Minimum elementary stream entry size
     */

    const uint8_t *pmt_data = data->Data();
    size_t offset = 0;

    // Handle pointer_field (offset to first section)
    if (data->Size() > 0) {
        uint8_t pointer_field = pmt_data[0];
        offset = 1 + pointer_field; // Skip pointer_field, locate to table_id
    }

    // Validate minimum data length (12 bytes = PMT header minimum)
    if (offset >= data->Size() || data->Size() - offset < PMT_TABLE_HEADER_SIZE) {
        RTSP_LOGE("PMT: Data too small after pointer field (%zu bytes available, need %zu)", data->Size() - offset,
                  PMT_TABLE_HEADER_SIZE);
        return;
    }

    const uint8_t *table_data = pmt_data + offset;

    // Validate table_id (PMT table fixed to 0x02)
    if (table_data[0] != PMT_TABLE_ID) {
        RTSP_LOGE("PMT: Wrong table ID 0x%02X, expected 0x%02X", table_data[0], PMT_TABLE_ID);
        return;
    }

    // Parse section_length (12 bits, from low 4 bits of byte 1 and byte 2)
    uint16_t section_length = ((table_data[1] & 0x0F) << 8) | table_data[2];
    if (section_length < 9) { // Minimum length: header(12) + CRC(4) - 3 = 9
        RTSP_LOGE("PMT: Section too short (%d bytes, minimum 9 required)", section_length);
        return;
    }

    // Parse PMT header information (commented out unused variables)
    uint16_t program_number = (table_data[3] << 8) | table_data[4];
    // uint8_t version_number = (table_data[5] >> 1) & 0x1F;
    // bool current_next_indicator = table_data[5] & 0x01;
    // uint8_t section_number = table_data[6];
    // uint8_t last_section_number = table_data[7];

    // Parse PCR_PID (13 bits, from low 5 bits of byte 8 and byte 9) - commented out as unused
    // uint16_t pcr_pid = ((table_data[8] & 0x1F) << 8) | table_data[9];

    // Parse program_info_length (12 bits, from low 4 bits of byte 10 and byte 11)
    uint16_t program_info_length = ((table_data[10] & 0x0F) << 8) | table_data[11];

    // Parse elementary stream entries
    std::map<uint16_t, uint8_t> stream_pids;
    size_t stream_data_start =
        PMT_TABLE_HEADER_SIZE + program_info_length;            // Skip PMT header (12 bytes) + program descriptors
    size_t stream_data_end = 3 + section_length - PMT_CRC_SIZE; // Subtract CRC (4 bytes)

    size_t entry_offset = stream_data_start;
    while (entry_offset + PMT_STREAM_ENTRY_SIZE <= stream_data_end &&
           offset + entry_offset + PMT_STREAM_ENTRY_SIZE <= data->Size()) {

        // Parse elementary stream entry (minimum 5 bytes)
        uint8_t stream_type = table_data[entry_offset];
        uint16_t elementary_pid = ((table_data[entry_offset + 1] & 0x1F) << 8) | table_data[entry_offset + 2];
        uint16_t es_info_length = ((table_data[entry_offset + 3] & 0x0F) << 8) | table_data[entry_offset + 4];

        // Codec name mapping (commented out as it's only used for debug logging)
        /*
        const char *codec_name = "Unknown";
        switch (stream_type) {
            case 0x02: codec_name = "MPEG-2 Video"; break;
            case 0x03: codec_name = "MPEG-1 Audio"; break;
            case 0x04: codec_name = "MPEG-2 Audio"; break;
            case 0x0F: codec_name = "AAC Audio"; break;
            case 0x1B: codec_name = "H.264 Video"; break;
            case 0x24: codec_name = "H.265 Video"; break;
            case 0x81: codec_name = "AC-3 Audio"; break;
        }
        */

        stream_pids[elementary_pid] = stream_type;

        // Move to next elementary stream entry
        entry_offset += PMT_STREAM_ENTRY_SIZE + es_info_length;
    }

    RTSP_LOGD("PMT: Found %zu streams (Program %d)", stream_pids.size(), program_number);

    // Notify callback
    if (callback_) {
        callback_->OnPMTReceived(pid, stream_pids);
    }
}

void TSStreamParser::ProcessStreamData(uint16_t pid, const std::shared_ptr<coreutils::DataBuffer> &data)
{
    if (!callback_) {
        return;
    }

    // Get or create stream buffer
    auto &buffer = stream_buffers_[pid];
    if (!buffer) {
        buffer = std::make_shared<coreutils::DataBuffer>();
    }

    // Add data to buffer
    buffer->Append(data->Data(), data->Size());

    // For audio/video streams, pass data directly (simplified processing)
    // In real applications, this should parse PES packets and frame boundaries
    uint64_t pts = 0;
    if (last_pcr_.count(pid)) {
        pts = last_pcr_[pid];
    }

    if (IsVideoPID(pid)) {
        callback_->OnVideoData(pid, data->Data(), data->Size(), pts);
    } else if (IsAudioPID(pid)) {
        callback_->OnAudioData(pid, data->Data(), data->Size(), pts);
    }
}

void TSStreamParser::ProcessPCR(uint16_t pid, uint64_t pcr)
{
    last_pcr_[pid] = pcr;

    // Log PCR info occasionally for debugging
    static size_t pcr_count = 0;
    if (++pcr_count % 100 == 0) {
        RTSP_LOGD("PCR: PID 0x%04X, PCR = %llu (%.3f sec)", pid, pcr, pcr / 27000000.0);
    }
}

bool TSStreamParser::IsVideoPID(uint16_t pid) const
{
    return enabled_video_pids_.find(pid) != enabled_video_pids_.end();
}

bool TSStreamParser::IsAudioPID(uint16_t pid) const
{
    return enabled_audio_pids_.find(pid) != enabled_audio_pids_.end();
}

bool TSStreamParser::HasPCR(const TSPacket &packet) const
{
    /*
     * PCR is located in the adaptation field of TS packet.
     * Check:
     * 1. adaptation_field_control indicates adaptation field present (10 or 11)
     * 2. adaptation field length > 0
     * 3. PCR_flag is set in adaptation field
     */

    // Check if adaptation field is present (bits 01 or 11 in adaptation_field_control)
    if (packet.header.adaptation_field_control != 2 && packet.header.adaptation_field_control != 3) {
        return false; // No adaptation field
    }

    // Check if we have adaptation field data
    if (!packet.adaptation_field || packet.adaptation_field->Size() < 2) {
        return false; // No adaptation field data or too small
    }

    const uint8_t *af_data = packet.adaptation_field->Data();
    uint8_t adaptation_field_length = af_data[0];

    // Check adaptation field length
    if (adaptation_field_length < 7) {
        return false; // PCR requires at least 6 bytes + flags byte
    }

    // Check PCR_flag (bit 4 of flags byte)
    uint8_t flags = af_data[1];
    bool pcr_flag = (flags & 0x10) != 0;

    return pcr_flag;
}

uint64_t TSStreamParser::ExtractPCR(const TSPacket &packet)
{
    /*
     * PCR structure in adaptation field (after length and flags):
     * PCR_base (33 bits) + reserved (6 bits) + PCR_extension (9 bits)
     * Total: 48 bits (6 bytes)
     *
     * Byte layout:
     * 0: PCR_base[32:25]
     * 1: PCR_base[24:17]
     * 2: PCR_base[16:9]
     * 3: PCR_base[8:1]
     * 4: PCR_base[0] + reserved(6) + PCR_ext[8]
     * 5: PCR_ext[7:0]
     */

    if (!HasPCR(packet)) {
        return 0;
    }

    const uint8_t *af_data = packet.adaptation_field->Data();
    // Skip adaptation_field_length(1) + flags(1) = 2 bytes to get to PCR data
    const uint8_t *pcr_data = af_data + 2;

    // Extract 33-bit PCR_base
    uint64_t pcr_base = ((uint64_t)pcr_data[0] << 25) | ((uint64_t)pcr_data[1] << 17) | ((uint64_t)pcr_data[2] << 9) |
                        ((uint64_t)pcr_data[3] << 1) | ((uint64_t)(pcr_data[4] >> 7) & 0x01);

    // Extract 9-bit PCR_extension
    uint16_t pcr_ext = ((uint16_t)(pcr_data[4] & 0x01) << 8) | (uint16_t)pcr_data[5];

    // PCR = PCR_base * 300 + PCR_extension (27MHz clock)
    return (pcr_base * 300) + pcr_ext;
}

} // namespace lmshao::rtsp::mpegts
