/**
 * @author SHAO Liming <lmshao@163.com>
 * @copyright Copyright (c) 2025 SHAO Liming
 * @license MIT
 *
 * SPDX-License-Identifier: MIT
 */

#ifndef SESSION_AAC_READER_H
#define SESSION_AAC_READER_H

#include <vector>

#include "aac_file_reader.h"

/**
 * @brief Session-specific AAC reader
 *
 * Each RTSP session gets its own SessionAacReader to maintain
 * independent playback position over a shared MappedFile.
 */
class SessionAacReader {
public:
    explicit SessionAacReader(std::shared_ptr<lmshao::lmcore::MappedFile> mapped_file);
    ~SessionAacReader() = default;

    /**
     * @brief Read next AAC frame for this session
     * @param frame_data Output buffer for frame data
     * @return true if frame was read, false if EOF
     */
    bool ReadNextFrame(std::vector<uint8_t> &frame_data);

    /**
     * @brief Reset playback to beginning
     */
    void Reset();

    /**
     * @brief Get playback information
     */
    const AacPlaybackInfo &GetPlaybackInfo() const { return reader_.GetPlaybackInfo(); }

    /**
     * @brief Get sample rate
     */
    uint32_t GetSampleRate() const { return reader_.GetSampleRate(); }

    /**
     * @brief Get channel count
     */
    uint8_t GetChannels() const { return reader_.GetChannels(); }

    /**
     * @brief Get profile
     */
    uint8_t GetProfile() const { return reader_.GetProfile(); }

    /**
     * @brief Get average bitrate in bps
     */
    uint32_t GetBitrate() const { return reader_.GetBitrate(); }

    /**
     * @brief Check if reader is valid
     */
    bool IsValid() const { return reader_.IsValid(); }

private:
    AacFileReader reader_;
};

#endif // SESSION_AAC_READER_H
