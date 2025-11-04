/**
 * @author SHAO Liming <lmshao@163.com>
 * @copyright Copyright (c) 2025 SHAO Liming
 * @license MIT
 *
 * SPDX-License-Identifier: MIT
 */

#ifndef LMSHAO_RTSP_SESSION_H264_READER_H
#define LMSHAO_RTSP_SESSION_H264_READER_H

#include <memory>
#include <vector>

#include "lmcore/mapped_file.h"

/**
 * @brief Local frame structure for SessionH264Reader
 */
struct LocalMediaFrame {
    std::vector<uint8_t> data; ///< Frame data
    uint64_t timestamp;        ///< Timestamp in milliseconds
    bool is_keyframe;          ///< Whether this is a keyframe
};

/**
 * @brief Session-specific H.264 reader with independent playback state
 *
 * This class provides thread-safe H.264 frame reading from a shared
 * MappedFile instance. Each session maintains its own reading position
 * and playback state while sharing the underlying file mapping.
 */
class SessionH264Reader {
public:
    /**
     * @brief Constructor
     * @param mapped_file Shared MappedFile instance
     */
    explicit SessionH264Reader(std::shared_ptr<lmshao::lmcore::MappedFile> mapped_file);

    /**
     * @brief Destructor
     */
    ~SessionH264Reader() = default;

    /**
     * @brief Read the next frame from current position
     * @param frame MediaFrame to store the frame data
     * @return true if successful, false on EOF or error
     */
    bool ReadNextFrame(LocalMediaFrame &frame);

    /**
     * @brief Read the next frame data
     * @param frame_data Vector to store the frame data
     * @return true if successful, false on EOF or error
     */
    bool ReadNextFrame(std::vector<uint8_t> &frame_data);

    /**
     * @brief Seek to a specific timestamp (seconds)
     * @param timestamp Target timestamp in seconds
     * @return true if successful, false on error
     */
    bool SeekToTime(double timestamp);

    /**
     * @brief Seek to a specific frame index
     * @param frame_index Target frame index (0-based)
     * @return true if successful, false on error
     */
    bool SeekToFrame(size_t frame_index);

    /**
     * @brief Reset to the beginning of the file
     */
    void Reset();

    /**
     * @brief Playback information structure
     */
    struct PlaybackInfo {
        size_t current_offset_; ///< Current file offset
        size_t current_frame_;  ///< Current frame index
        double current_time_;   ///< Current timestamp in seconds
        size_t total_frames_;   ///< Total number of frames
        double total_duration_; ///< Total duration in seconds
    };

    /**
     * @brief Get current playback information
     * @return PlaybackInfo structure
     */
    PlaybackInfo GetPlaybackInfo() const;

    /**
     * @brief Check if end of file is reached
     * @return true if EOF, false otherwise
     */
    bool IsEOF() const;

    /**
     * @brief Get SPS (Sequence Parameter Set)
     * @return SPS data
     */
    std::vector<uint8_t> GetSPS() const;

    /**
     * @brief Get PPS (Picture Parameter Set)
     * @return PPS data
     */
    std::vector<uint8_t> GetPPS() const;

    /**
     * @brief Get frame rate
     * @return Frame rate in fps
     */
    uint32_t GetFrameRate() const;

private:
    /**
     * @brief Frame information structure for indexing
     */
    struct FrameInfo {
        size_t offset_;     ///< Frame offset in file
        size_t size_;       ///< Frame size in bytes
        double timestamp_;  ///< Frame timestamp in seconds
        bool is_keyframe_;  ///< Whether this is a keyframe (IDR)
        uint8_t nalu_type_; ///< NALU type
    };

    std::shared_ptr<lmshao::lmcore::MappedFile> mapped_file_;

    // Session-specific state
    size_t current_offset_;      ///< Current reading position
    size_t current_frame_index_; ///< Current frame index
    double current_timestamp_;   ///< Current timestamp

    // Frame index cache (built lazily)
    mutable std::vector<FrameInfo> frame_index_;
    mutable bool index_built_;
    mutable std::vector<uint8_t> sps_;
    mutable std::vector<uint8_t> pps_;
    mutable uint32_t frame_rate_;
    mutable bool parameter_sets_extracted_;

    // Internal methods
    bool FindNextNALU(size_t start_offset, size_t &nalu_start, size_t &nalu_size, uint8_t &nalu_type);
    void BuildFrameIndex() const;
    void ExtractParameterSets() const;
    bool SeekToOffset(size_t offset);
    int FindStartCode(const uint8_t *data, size_t start_pos, size_t data_size);
};

#endif // LMSHAO_RTSP_SESSION_H264_READER_H