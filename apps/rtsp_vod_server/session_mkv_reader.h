/**
 * @author SHAO Liming <lmshao@163.com>
 * @copyright Copyright (c) 2025 SHAO Liming
 * @license MIT
 *
 * SPDX-License-Identifier: MIT
 */

#ifndef LMSHAO_RTSP_SESSION_MKV_READER_H
#define LMSHAO_RTSP_SESSION_MKV_READER_H

#include <condition_variable>
#include <memory>
#include <mutex>
#include <queue>
#include <vector>

#include "lmcore/mapped_file.h"
#include "lmmkv/mkv_demuxer.h"
#include "lmmkv/mkv_listeners.h"

/**
 * @brief Local frame structure for SessionMkvReader
 */
struct LocalMediaFrameMkv {
    std::vector<uint8_t> data;
    uint64_t timestamp; // in milliseconds
    bool is_keyframe;
};

/**
 * @brief Session-specific MKV reader with independent playback state
 *
 * This class provides MKV demuxing for a specific track from a shared
 * MappedFile instance. Each session maintains its own demuxer and playback state.
 */
class SessionMkvReader {
public:
    /**
     * @brief Constructor
     * @param mapped_file Shared MappedFile instance
     * @param track_number Target track number to extract
     */
    explicit SessionMkvReader(std::shared_ptr<lmshao::lmcore::MappedFile> mapped_file, uint64_t track_number);

    /**
     * @brief Destructor
     */
    ~SessionMkvReader();

    /**
     * @brief Initialize the reader (start demuxing)
     * @return true if successful, false on error
     */
    bool Initialize();

    /**
     * @brief Read the next frame from current position
     * @param frame MediaFrame to store the frame data
     * @return true if successful, false on EOF or error
     */
    bool ReadNextFrame(LocalMediaFrameMkv &frame);

    /**
     * @brief Reset to the beginning of the file
     */
    void Reset();

    /**
     * @brief Playback information structure
     */
    struct PlaybackInfo {
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
     * @brief Check if end of stream is reached
     * @return true if EOS, false otherwise
     */
    bool IsEOS() const;

    /**
     * @brief Get SPS (H.264/H.265)
     */
    std::vector<uint8_t> GetSPS() const;

    /**
     * @brief Get PPS (H.264/H.265)
     */
    std::vector<uint8_t> GetPPS() const;

    /**
     * @brief Get VPS (H.265 only)
     */
    std::vector<uint8_t> GetVPS() const;

    /**
     * @brief Get frame rate (estimated from track info)
     */
    uint32_t GetFrameRate() const;

    /**
     * @brief Get video width
     */
    uint32_t GetWidth() const;

    /**
     * @brief Get video height
     */
    uint32_t GetHeight() const;

    /**
     * @brief Get sample rate (for audio tracks)
     */
    uint32_t GetSampleRate() const;

    /**
     * @brief Get channel count (for audio tracks)
     */
    uint32_t GetChannels() const;

    /**
     * @brief Get codec type
     */
    std::string GetCodecId() const;

    /**
     * @brief Check if reader is valid
     */
    bool IsValid() const { return is_valid_; }

private:
    /**
     * @brief Internal listener for MkvDemuxer callbacks
     */
    class ReaderListener : public lmshao::lmmkv::IMkvDemuxListener {
    public:
        explicit ReaderListener(SessionMkvReader *parent, uint64_t track_number);

        void OnInfo(const lmshao::lmmkv::MkvInfo &info) override;
        void OnTrack(const lmshao::lmmkv::MkvTrackInfo &track) override;
        void OnFrame(const lmshao::lmmkv::MkvFrame &frame) override;
        void OnEndOfStream() override;
        void OnError(int code, const std::string &msg) override;

    private:
        SessionMkvReader *parent_;
        uint64_t target_track_;
    };

    /**
     * @brief Extract parameter sets from codec_private (avcC/hvcC format)
     */
    void ExtractParameterSets();

    /**
     * @brief Extract SPS/PPS from avcC (H.264)
     */
    void ExtractAvcC();

    /**
     * @brief Extract VPS/SPS/PPS from hvcC (H.265)
     */
    void ExtractHvcC();

    /**
     * @brief Refill frame buffer by consuming more data from file
     * @return true if more data was consumed, false if EOF
     */
    bool RefillBuffer();

private:
    std::shared_ptr<lmshao::lmcore::MappedFile> mapped_file_;
    std::unique_ptr<lmshao::lmmkv::MkvDemuxer> demuxer_;
    std::shared_ptr<ReaderListener> listener_;

    uint64_t track_number_;
    lmshao::lmmkv::MkvTrackInfo track_info_;
    lmshao::lmmkv::MkvInfo mkv_info_;

    // Streaming demux control
    size_t file_offset_;                             ///< Current read position in file
    static constexpr size_t CHUNK_SIZE = 128 * 1024; ///< Read 128KB at a time
    static constexpr size_t MIN_BUFFER_FRAMES = 10;  ///< Minimum frames to buffer before refill
    static constexpr size_t MAX_BUFFER_FRAMES = 50;  ///< Maximum frames to buffer (prevent memory overflow)

    // Frame queue for async callback to sync read
    std::queue<LocalMediaFrameMkv> frame_queue_;
    mutable std::mutex queue_mutex_;
    std::condition_variable queue_cv_;

    // Parameter sets (video)
    std::vector<uint8_t> sps_;
    std::vector<uint8_t> pps_;
    std::vector<uint8_t> vps_; // H.265 only

    // Playback state
    size_t current_frame_index_;
    double current_time_;
    size_t total_frames_;
    bool eos_reached_;
    bool is_valid_;
    bool track_found_;
};

#endif // LMSHAO_RTSP_SESSION_MKV_READER_H
