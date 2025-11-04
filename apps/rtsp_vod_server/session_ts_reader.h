/**
 * @author SHAO Liming <lmshao@163.com>
 * @copyright Copyright (c) 2025 SHAO Liming
 * @license MIT
 *
 * SPDX-License-Identifier: MIT
 */

#ifndef LMSHAO_RTSP_SESSION_TS_READER_H
#define LMSHAO_RTSP_SESSION_TS_READER_H

#include <memory>
#include <vector>

#include "lmcore/mapped_file.h"
#include "ts_file_reader.h"

/**
 * @brief Session-specific TS reader with independent playback state
 *
 * Each session maintains its own reading position while sharing
 * the underlying file mapping.
 */
class SessionTSReader {
public:
    /**
     * @brief Constructor
     * @param mapped_file Shared MappedFile instance
     */
    explicit SessionTSReader(std::shared_ptr<lmshao::lmcore::MappedFile> mapped_file);

    /**
     * @brief Destructor
     */
    ~SessionTSReader() = default;

    /**
     * @brief Read the next TS packet
     * @param packet_data Vector to store the packet data
     * @return true if successful, false on EOF
     */
    bool ReadNextPacket(std::vector<uint8_t> &packet_data);

    /**
     * @brief Reset to the beginning of the file
     */
    void Reset();

    /**
     * @brief Playback information structure
     */
    struct PlaybackInfo {
        size_t current_packet_; ///< Current packet index
        size_t total_packets_;  ///< Total number of packets
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
     * @brief Get bitrate
     * @return Bitrate in bits per second
     */
    uint32_t GetBitrate() const;

private:
    std::unique_ptr<TSFileReader> ts_reader_;
    size_t current_packet_index_;
};

#endif // LMSHAO_RTSP_SESSION_TS_READER_H
