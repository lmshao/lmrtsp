/**
 * @author SHAO Liming <lmshao@163.com>
 * @copyright Copyright (c) 2025 SHAO Liming
 * @license MIT
 *
 * SPDX-License-Identifier: MIT
 */

#ifndef AAC_FILE_READER_H
#define AAC_FILE_READER_H

#include <lmcore/mapped_file.h>
#include <lmrtsp/adts_parser.h>

#include <memory>
#include <vector>

/**
 * @brief Playback information for AAC stream
 */
struct AacPlaybackInfo {
    size_t total_frames_ = 0;
    double total_duration_ = 0.0; // seconds
    size_t current_frame_ = 0;
};

/**
 * @brief AAC file reader for parsing ADTS format files
 */
class AacFileReader {
public:
    explicit AacFileReader(std::shared_ptr<lmshao::lmcore::MappedFile> mapped_file);
    ~AacFileReader() = default;

    /**
     * @brief Read next AAC frame (ADTS frame)
     * @param frame_data Output buffer for frame data (including ADTS header)
     * @return true if frame was read successfully, false if EOF or error
     */
    bool ReadNextFrame(std::vector<uint8_t> &frame_data);

    /**
     * @brief Reset reader to beginning of file
     */
    void Reset();

    /**
     * @brief Get playback information
     */
    const AacPlaybackInfo &GetPlaybackInfo() const { return playback_info_; }

    /**
     * @brief Get sample rate from ADTS header
     */
    uint32_t GetSampleRate() const { return sample_rate_; }

    /**
     * @brief Get channel count from ADTS header
     */
    uint8_t GetChannels() const { return channels_; }

    /**
     * @brief Get profile from ADTS header
     */
    uint8_t GetProfile() const { return profile_; }

    /**
     * @brief Check if file is valid AAC/ADTS format
     */
    bool IsValid() const { return is_valid_; }

    /**
     * @brief Get average bitrate in bps
     */
    uint32_t GetBitrate() const;

private:
    /**
     * @brief Analyze file to count frames and extract metadata
     */
    void AnalyzeFile();

    std::shared_ptr<lmshao::lmcore::MappedFile> mapped_file_;
    size_t file_size_ = 0;
    size_t current_offset_ = 0;
    bool is_valid_ = false;

    // AAC stream metadata
    uint32_t sample_rate_ = 0;
    uint8_t channels_ = 0;
    uint8_t profile_ = 0;

    // Playback info
    AacPlaybackInfo playback_info_;
};

#endif // AAC_FILE_READER_H
