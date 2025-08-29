/**
 * @author SHAO Liming <lmshao@163.com>
 * @copyright Copyright (c) 2025 SHAO Liming
 * @license MIT
 *
 * SPDX-License-Identifier: MIT
 */

#ifndef LMSHAO_MPEGTS_TS_MUXER_H
#define LMSHAO_MPEGTS_TS_MUXER_H

#include <map>
#include <memory>
#include <vector>

#include "adts_header.h"
#include "ts_callback.h"
#include "ts_types.h"

namespace lmshao::mpegts {

// TS stream muxer
class TSStreamMuxer {
public:
    TSStreamMuxer();
    ~TSStreamMuxer();

    // Set callback
    void SetCallback(std::shared_ptr<TSMuxerListener> callback);

    // Continuous muxing interface
    void MuxVideoData(uint16_t pid, const uint8_t *data, size_t size, uint64_t timestamp_ms = 0);
    void MuxAudioData(uint16_t pid, const uint8_t *data, size_t size, uint64_t timestamp_ms = 0);

    // Configuration
    void SetVideoPID(uint16_t pid);
    void SetAudioPID(uint16_t pid);
    void SetAudioParams(int sample_rate, int channels);
    void SetPCRPID(uint16_t pid);
    void SetBitrate(uint32_t bitrate);
    void SetPCRInterval(uint32_t interval_packets);

    // Control
    void Start();
    void Stop();
    void Reset();
    void Finalize();

    // Status query
    bool IsRunning() const;
    size_t GetMuxedPackets() const;

private:
    // Internal state
    std::shared_ptr<TSMuxerListener> callback_;
    uint16_t video_pid_;
    uint16_t audio_pid_;
    int audio_sample_rate_;
    int audio_channels_;
    uint16_t pcr_pid_;
    uint32_t bitrate_;
    uint32_t pcr_interval_;
    uint8_t video_counter_;
    uint8_t audio_counter_;
    uint8_t pat_counter_;
    uint8_t pmt_counter_;
    uint32_t packet_count_;
    uint64_t current_pcr_;
    ADTSHeader adts_header_;
    bool running_;

    // Muxing methods
    void GenerateAndSendPAT();
    void GenerateAndSendPMT();
    int GetAACSampleRateIndex(int sample_rate);
    void GenerateAndSendPCR();
    std::vector<TSPacket> CreatePackets(uint16_t pid, const uint8_t *data, size_t size);

    // Helper methods
    TSPacket CreatePacket(uint16_t pid, const uint8_t *payload_data, size_t payload_size, bool payload_start = false);
    void UpdatePCR();
    uint32_t CalculateCRC32(const uint8_t *data, size_t length);
};

} // namespace lmshao::mpegts

#endif // LMSHAO_MPEGTS_TS_MUXER_H
