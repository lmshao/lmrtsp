/**
 * @author SHAO Liming <lmshao@163.com>
 * @copyright Copyright (c) 2025 SHAO Liming
 * @license MIT
 *
 * SPDX-License-Identifier: MIT
 */

#ifndef LMSHAO_RTSP_TS_FILE_READER_H
#define LMSHAO_RTSP_TS_FILE_READER_H

#include <memory>
#include <vector>

#include "lmcore/mapped_file.h"

/**
 * @brief MPEG-TS file reader for VOD streaming
 *
 * Reads MPEG-TS files and provides packets for streaming.
 * TS files contain multiplexed audio/video in 188-byte packets.
 */
class TSFileReader {
public:
    /**
     * @brief Constructor
     * @param mapped_file Shared MappedFile instance
     */
    explicit TSFileReader(std::shared_ptr<lmshao::lmcore::MappedFile> mapped_file);

    /**
     * @brief Destructor
     */
    ~TSFileReader() = default;

    /**
     * @brief Read the next TS packet
     * @param packet_data Output packet data (188 bytes)
     * @return true if successful, false on EOF
     */
    bool ReadNextPacket(std::vector<uint8_t> &packet_data);

    /**
     * @brief Reset to the beginning of the file
     */
    void Reset();

    /**
     * @brief Check if end of file is reached
     * @return true if EOF, false otherwise
     */
    bool IsEOF() const;

    /**
     * @brief Get playback information
     */
    struct PlaybackInfo {
        size_t current_offset_; ///< Current file offset
        size_t total_packets_;  ///< Total number of TS packets
        double total_duration_; ///< Estimated duration in seconds
    };

    /**
     * @brief Get current playback information
     * @return PlaybackInfo structure
     */
    PlaybackInfo GetPlaybackInfo() const;

    /**
     * @brief Get estimated bitrate
     * @return Bitrate in bits per second
     */
    uint32_t GetBitrate() const;

private:
    // TS packet constants
    static constexpr size_t TS_PACKET_SIZE = 188;
    static constexpr uint8_t TS_SYNC_BYTE = 0x47;

    std::shared_ptr<lmshao::lmcore::MappedFile> mapped_file_;
    size_t current_offset_;
    size_t total_packets_;
    uint32_t estimated_bitrate_;

    /**
     * @brief Calculate total number of packets
     */
    void CalculateTotalPackets();
};

#endif // LMSHAO_RTSP_TS_FILE_READER_H
