/**
 * @author SHAO Liming <lmshao@163.com>
 * @copyright Copyright (c) 2025 SHAO Liming
 * @license MIT
 *
 * SPDX-License-Identifier: MIT
 */

#ifndef LMSHAO_RTSP_SESSION_H265_READER_H
#define LMSHAO_RTSP_SESSION_H265_READER_H

#include <memory>
#include <vector>

#include "lmcore/mapped_file.h"

/**
 * @brief Local frame structure for SessionH265Reader
 */
struct LocalMediaFrameH265 {
    std::vector<uint8_t> data;
    uint64_t timestamp;
    bool is_keyframe;
};

/**
 * @brief Session-specific H.265 reader with independent playback state
 */
class SessionH265Reader {
public:
    explicit SessionH265Reader(std::shared_ptr<lmshao::lmcore::MappedFile> mapped_file);
    ~SessionH265Reader() = default;

    bool ReadNextFrame(LocalMediaFrameH265 &frame);
    bool ReadNextFrame(std::vector<uint8_t> &frame_data);
    bool SeekToTime(double timestamp);
    bool SeekToFrame(size_t frame_index);
    void Reset();

    struct PlaybackInfo {
        size_t current_offset_;
        size_t current_frame_;
        double current_time_;
        size_t total_frames_;
        double total_duration_;
    };

    PlaybackInfo GetPlaybackInfo() const;
    bool IsEOF() const;

    std::vector<uint8_t> GetVPS() const;
    std::vector<uint8_t> GetSPS() const;
    std::vector<uint8_t> GetPPS() const;
    uint32_t GetFrameRate() const;

private:
    struct FrameInfo {
        size_t offset_;
        size_t size_;
        double timestamp_;
        bool is_keyframe_;
        uint8_t nalu_type_;
    };

    std::shared_ptr<lmshao::lmcore::MappedFile> mapped_file_;

    size_t current_offset_;
    size_t current_frame_index_;
    double current_timestamp_;

    mutable std::vector<FrameInfo> frame_index_;
    mutable bool index_built_;
    mutable std::vector<uint8_t> vps_;
    mutable std::vector<uint8_t> sps_;
    mutable std::vector<uint8_t> pps_;
    mutable uint32_t frame_rate_;
    mutable bool parameter_sets_extracted_;

    bool FindNextNALU(size_t start_offset, size_t &nalu_start, size_t &nalu_size, uint8_t &nalu_type);
    void BuildFrameIndex() const;
    void ExtractParameterSets() const;
    bool SeekToOffset(size_t offset);
    int FindStartCode(const uint8_t *data, size_t start_pos, size_t data_size);
};

#endif // LMSHAO_RTSP_SESSION_H265_READER_H
