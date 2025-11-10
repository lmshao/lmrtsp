/**
 * @author SHAO Liming <lmshao@163.com>
 * @copyright Copyright (c) 2025 SHAO Liming
 * @license MIT
 *
 * SPDX-License-Identifier: MIT
 */

#include "session_mkv_reader.h"

#include <algorithm>
#include <cstring>
#include <iostream>

using namespace lmshao::lmmkv;

// ReaderListener implementation
SessionMkvReader::ReaderListener::ReaderListener(SessionMkvReader *parent, uint64_t track_number)
    : parent_(parent), target_track_(track_number)
{
}

void SessionMkvReader::ReaderListener::OnInfo(const MkvInfo &info)
{
    std::cout << "MKV Info: timecode_scale=" << info.timecode_scale_ns << " ns, duration=" << info.duration_seconds
              << " s" << std::endl;
    parent_->mkv_info_ = info;
}

void SessionMkvReader::ReaderListener::OnTrack(const MkvTrackInfo &track)
{
    std::cout << "MKV Track: number=" << track.track_number << ", codec=" << track.codec_id << std::endl;

    if (track.track_number == target_track_) {
        parent_->track_info_ = track;
        parent_->track_found_ = true;
        parent_->ExtractParameterSets();
    }
}

void SessionMkvReader::ReaderListener::OnFrame(const MkvFrame &frame)
{
    if (frame.track_number != target_track_) {
        return; // Skip frames from other tracks
    }

    std::unique_lock<std::mutex> lock(parent_->queue_mutex_);

    // In streaming mode, limit buffer size more aggressively
    if (parent_->frame_queue_.size() >= SessionMkvReader::MAX_BUFFER_FRAMES) {
        // Buffer is full, drop oldest frame (should rarely happen with good rate control)
        parent_->frame_queue_.pop();
    }

    // Copy frame data to queue
    LocalMediaFrameMkv local_frame;
    local_frame.data.assign(frame.data, frame.data + frame.size);
    local_frame.timestamp = frame.timecode_ns / 1000000; // Convert ns to ms
    local_frame.is_keyframe = frame.keyframe;

    parent_->frame_queue_.push(std::move(local_frame));
    parent_->total_frames_++;

    lock.unlock();
    parent_->queue_cv_.notify_one();
}

void SessionMkvReader::ReaderListener::OnEndOfStream()
{
    std::cout << "MKV End of stream, track " << target_track_ << std::endl;
    parent_->eos_reached_ = true;
    parent_->queue_cv_.notify_all();
}

void SessionMkvReader::ReaderListener::OnError(int code, const std::string &msg)
{
    std::cerr << "MKV Error(" << code << "): " << msg << std::endl;
    parent_->is_valid_ = false;
}

// SessionMkvReader implementation
SessionMkvReader::SessionMkvReader(std::shared_ptr<lmshao::lmcore::MappedFile> mapped_file, uint64_t track_number)
    : mapped_file_(mapped_file), track_number_(track_number), file_offset_(0), current_frame_index_(0),
      current_time_(0.0), total_frames_(0), eos_reached_(false), is_valid_(false), track_found_(false)
{
    if (!mapped_file_ || !mapped_file_->IsValid()) {
        std::cerr << "Invalid MappedFile instance" << std::endl;
        return;
    }

    std::cout << "SessionMkvReader created for file: " << mapped_file_->Path() << ", track: " << track_number_
              << " (streaming mode)" << std::endl;

    demuxer_ = std::make_unique<MkvDemuxer>();
    listener_ = std::make_shared<ReaderListener>(this, track_number_);
}

SessionMkvReader::~SessionMkvReader()
{
    if (demuxer_ && demuxer_->IsRunning()) {
        demuxer_->Stop();
    }
    std::cout << "SessionMkvReader destroyed" << std::endl;
}

bool SessionMkvReader::Initialize()
{
    if (!demuxer_ || !listener_) {
        return false;
    }

    demuxer_->SetListener(listener_);

    // Set track filter to only demux target track
    std::vector<uint64_t> tracks = {track_number_};
    demuxer_->SetTrackFilter(tracks);

    if (!demuxer_->Start()) {
        std::cerr << "Failed to start MkvDemuxer" << std::endl;
        return false;
    }

    // Streaming mode: only consume initial chunk to get track info and initial frames
    std::cout << "SessionMkvReader: Streaming mode - initial buffer fill..." << std::endl;

    if (!RefillBuffer()) {
        std::cerr << "Failed to initialize - no data consumed from initial chunk" << std::endl;
        return false;
    }

    // Wait briefly for initial frames to be parsed (non-blocking)
    std::unique_lock<std::mutex> lock(queue_mutex_);
    auto wait_result = queue_cv_.wait_for(lock, std::chrono::milliseconds(500),
                                          [this] { return !frame_queue_.empty() || eos_reached_; });

    if (!wait_result && frame_queue_.empty()) {
        std::cerr << "Timeout waiting for initial frames" << std::endl;
        return false;
    }

    if (!track_found_) {
        std::cerr << "Track " << track_number_ << " not found in MKV file" << std::endl;
        return false;
    }

    is_valid_ = true;
    std::cout << "SessionMkvReader initialized (streaming mode), initial buffer: " << frame_queue_.size() << " frames"
              << std::endl;

    return true;
}

bool SessionMkvReader::ReadNextFrame(LocalMediaFrameMkv &frame)
{
    std::unique_lock<std::mutex> lock(queue_mutex_);

    // On-demand refill: if buffer is running low, consume more data
    if (frame_queue_.size() < MIN_BUFFER_FRAMES && !eos_reached_) {
        lock.unlock(); // Release lock during I/O operation
        RefillBuffer();
        lock.lock();
    }

    // Wait for frame to be available (with timeout to prevent deadlock)
    auto wait_result = queue_cv_.wait_for(lock, std::chrono::milliseconds(100),
                                          [this] { return !frame_queue_.empty() || eos_reached_; });

    if (frame_queue_.empty()) {
        return false; // EOF or timeout
    }

    frame = std::move(frame_queue_.front());
    frame_queue_.pop();

    current_frame_index_++;
    current_time_ = frame.timestamp / 1000.0;

    return true;
}

void SessionMkvReader::Reset()
{
    std::lock_guard<std::mutex> lock(queue_mutex_);

    // Clear queue
    while (!frame_queue_.empty()) {
        frame_queue_.pop();
    }

    current_frame_index_ = 0;
    current_time_ = 0.0;
    total_frames_ = 0;
    eos_reached_ = false;
    file_offset_ = 0; // Reset file position for streaming mode

    // Re-demux the file
    if (demuxer_) {
        demuxer_->Reset();
        demuxer_->Start();

        // Streaming mode: refill initial buffer
        RefillBuffer();
    }

    std::cout << "SessionMkvReader reset to beginning (streaming mode)" << std::endl;
}

bool SessionMkvReader::RefillBuffer()
{
    if (eos_reached_) {
        return false; // Already reached end of file
    }

    const uint8_t *data = mapped_file_->Data();
    size_t file_size = mapped_file_->Size();

    if (file_offset_ >= file_size) {
        eos_reached_ = true;
        return false;
    }

    // Calculate chunk size to consume
    size_t remaining = file_size - file_offset_;
    size_t to_consume = std::min(CHUNK_SIZE, remaining);

    // Consume next chunk
    size_t consumed = demuxer_->Consume(data + file_offset_, to_consume);
    if (consumed > 0) {
        file_offset_ += consumed;
        return true;
    }

    return false;
}

SessionMkvReader::PlaybackInfo SessionMkvReader::GetPlaybackInfo() const
{
    std::lock_guard<std::mutex> lock(queue_mutex_);

    PlaybackInfo info;
    info.current_frame_ = current_frame_index_;
    info.current_time_ = current_time_;
    info.total_frames_ = total_frames_;
    info.total_duration_ = mkv_info_.duration_seconds;

    return info;
}

bool SessionMkvReader::IsEOS() const
{
    std::lock_guard<std::mutex> lock(queue_mutex_);
    return eos_reached_ && frame_queue_.empty();
}

void SessionMkvReader::ExtractParameterSets()
{
    const auto &codec_id = track_info_.codec_id;

    if (codec_id.find("V_MPEG4/ISO/AVC") == 0) {
        // H.264
        ExtractAvcC();
    } else if (codec_id.find("V_MPEGH/ISO/HEVC") == 0) {
        // H.265
        ExtractHvcC();
    }
}

void SessionMkvReader::ExtractAvcC()
{
    // Parse avcC format (ISO/IEC 14496-15)
    const auto &cp = track_info_.codec_private;

    if (cp.size() < 8) {
        std::cerr << "Invalid avcC data size: " << cp.size() << std::endl;
        return;
    }

    // avcC structure:
    // [0] configurationVersion
    // [1] AVCProfileIndication
    // [2] profile_compatibility
    // [3] AVCLevelIndication
    // [4] lengthSizeMinusOne (0xFF & 0x03)
    // [5] numOfSequenceParameterSets (0xFF & 0x1F)

    size_t offset = 5;
    uint8_t num_sps = cp[offset] & 0x1F;
    offset++;

    // Extract SPS
    for (uint8_t i = 0; i < num_sps && offset + 2 < cp.size(); i++) {
        uint16_t sps_length = (cp[offset] << 8) | cp[offset + 1];
        offset += 2;

        if (offset + sps_length > cp.size()) {
            std::cerr << "Invalid SPS length" << std::endl;
            break;
        }

        // Add Annex B start code
        sps_ = {0x00, 0x00, 0x00, 0x01};
        sps_.insert(sps_.end(), cp.begin() + offset, cp.begin() + offset + sps_length);
        offset += sps_length;

        std::cout << "Extracted SPS: " << sps_length << " bytes" << std::endl;
        break; // Use first SPS
    }

    // Extract PPS
    if (offset + 1 < cp.size()) {
        uint8_t num_pps = cp[offset];
        offset++;

        for (uint8_t i = 0; i < num_pps && offset + 2 < cp.size(); i++) {
            uint16_t pps_length = (cp[offset] << 8) | cp[offset + 1];
            offset += 2;

            if (offset + pps_length > cp.size()) {
                std::cerr << "Invalid PPS length" << std::endl;
                break;
            }

            // Add Annex B start code
            pps_ = {0x00, 0x00, 0x00, 0x01};
            pps_.insert(pps_.end(), cp.begin() + offset, cp.begin() + offset + pps_length);
            offset += pps_length;

            std::cout << "Extracted PPS: " << pps_length << " bytes" << std::endl;
            break; // Use first PPS
        }
    }
}

void SessionMkvReader::ExtractHvcC()
{
    // Parse hvcC format (ISO/IEC 14496-15)
    const auto &cp = track_info_.codec_private;

    if (cp.size() < 23) {
        std::cerr << "Invalid hvcC data size: " << cp.size() << std::endl;
        return;
    }

    // hvcC structure is more complex than avcC
    // For simplicity, we'll extract the first VPS/SPS/PPS found
    // Full parsing requires handling multiple array entries

    size_t offset = 22; // Skip header fields
    uint8_t num_arrays = cp[offset];
    offset++;

    for (uint8_t i = 0; i < num_arrays && offset + 3 < cp.size(); i++) {
        uint8_t nal_unit_type = cp[offset] & 0x3F;
        offset++;

        uint16_t num_nalus = (cp[offset] << 8) | cp[offset + 1];
        offset += 2;

        for (uint16_t j = 0; j < num_nalus && offset + 2 < cp.size(); j++) {
            uint16_t nalu_length = (cp[offset] << 8) | cp[offset + 1];
            offset += 2;

            if (offset + nalu_length > cp.size()) {
                break;
            }

            std::vector<uint8_t> nalu = {0x00, 0x00, 0x00, 0x01};
            nalu.insert(nalu.end(), cp.begin() + offset, cp.begin() + offset + nalu_length);
            offset += nalu_length;

            // H.265 NAL unit types: VPS=32, SPS=33, PPS=34
            if (nal_unit_type == 32 && vps_.empty()) {
                vps_ = nalu;
                std::cout << "Extracted VPS: " << nalu_length << " bytes" << std::endl;
            } else if (nal_unit_type == 33 && sps_.empty()) {
                sps_ = nalu;
                std::cout << "Extracted SPS: " << nalu_length << " bytes" << std::endl;
            } else if (nal_unit_type == 34 && pps_.empty()) {
                pps_ = nalu;
                std::cout << "Extracted PPS: " << nalu_length << " bytes" << std::endl;
            }
        }
    }
}

std::vector<uint8_t> SessionMkvReader::GetSPS() const
{
    return sps_;
}

std::vector<uint8_t> SessionMkvReader::GetPPS() const
{
    return pps_;
}

std::vector<uint8_t> SessionMkvReader::GetVPS() const
{
    return vps_;
}

uint32_t SessionMkvReader::GetFrameRate() const
{
    // Estimate from duration and frame count
    if (mkv_info_.duration_seconds > 0 && total_frames_ > 0) {
        return static_cast<uint32_t>(total_frames_ / mkv_info_.duration_seconds);
    }
    return 25; // Default
}

uint32_t SessionMkvReader::GetWidth() const
{
    return track_info_.width;
}

uint32_t SessionMkvReader::GetHeight() const
{
    return track_info_.height;
}

uint32_t SessionMkvReader::GetSampleRate() const
{
    return track_info_.sample_rate;
}

uint32_t SessionMkvReader::GetChannels() const
{
    return track_info_.channels;
}

std::string SessionMkvReader::GetCodecId() const
{
    return track_info_.codec_id;
}
