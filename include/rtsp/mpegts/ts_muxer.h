/**
 * @author SHAO Liming <lmshao@163.com>
 * @copyright Copyright (c) 2025 SHAO Liming
 * @license MIT
 *
 * SPDX-License-Identifier: MIT
 */

#ifndef RTSP_TS_MUXER_H
#define RTSP_TS_MUXER_H

#include <map>
#include <memory>
#include <vector>

#include "ts_callback.h"
#include "ts_types.h"

namespace lmshao::rtsp::mpegts {

// TS stream muxer
class TSStreamMuxer {
public:
    TSStreamMuxer();
    ~TSStreamMuxer();

    // Set callback
    void SetCallback(std::shared_ptr<TSPacketCallback> callback);

    // Continuous muxing interface
    void MuxVideoData(uint16_t pid, const uint8_t *data, size_t size);
    void MuxAudioData(uint16_t pid, const uint8_t *data, size_t size);

    // Configuration
    void SetVideoPID(uint16_t pid);
    void SetAudioPID(uint16_t pid);
    void SetPCRPID(uint16_t pid);
    void SetBitrate(uint32_t bitrate);
    void SetPCRInterval(uint32_t interval_packets);

    // Control
    void Start();
    void Stop();
    void Reset();

    // Status query
    bool IsRunning() const;
    size_t GetMuxedPackets() const;

private:
    // Internal state
    std::shared_ptr<TSPacketCallback> callback_;
    uint16_t video_pid_;
    uint16_t audio_pid_;
    uint16_t pcr_pid_;
    uint32_t bitrate_;
    uint32_t pcr_interval_;
    uint8_t video_counter_;
    uint8_t audio_counter_;
    uint32_t packet_count_;
    uint64_t current_pcr_;
    bool running_;

    // Muxing methods
    void GenerateAndSendPAT();
    void GenerateAndSendPMT();
    void GenerateAndSendPCR();
    std::vector<TSPacket> CreatePackets(uint16_t pid, const uint8_t *data, size_t size);

    // Helper methods
    TSPacket CreatePacket(uint16_t pid, const uint8_t *payload_data, size_t payload_size, bool payload_start = false);
    void UpdatePCR();
};

} // namespace lmshao::rtsp::mpegts

#endif // RTSP_TS_MUXER_H
