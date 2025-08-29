/**
 * @author SHAO Liming <lmshao@163.com>
 * @copyright Copyright (c) 2025 SHAO Liming
 * @license MIT
 *
 * SPDX-License-Identifier: MIT
 */

#ifndef LMSHAO_MPEGTS_TS_CALLBACK_H
#define LMSHAO_MPEGTS_TS_CALLBACK_H

#include <cstddef>
#include <cstdint>
#include <map>
#include <string>
#include <vector>

namespace lmshao::mpegts {

// TS muxer callback interface
class TSMuxerListener {
public:
    virtual ~TSMuxerListener() = default;

    // Called when a TS packet is ready
    virtual void OnTSPacket(const uint8_t *data, size_t size) = 0;
};

// TS demuxer callback interface
class TSDemuxerListener {
public:
    virtual ~TSDemuxerListener() = default;

    virtual void OnVideoData(uint16_t pid, const uint8_t *data, size_t size, uint64_t pts) = 0;
    virtual void OnAudioData(uint16_t pid, const uint8_t *data, size_t size, uint64_t pts) = 0;

    virtual void OnPATReceived(const std::vector<uint16_t> &program_pids) = 0;
    virtual void OnPMTReceived(uint16_t program_pid, const std::map<uint16_t, uint8_t> &stream_pids) = 0;

    virtual void OnParseError(const std::string &error) = 0;
    virtual void OnSyncLoss() = 0;
};

} // namespace lmshao::mpegts

#endif // LMSHAO_MPEGTS_TS_CALLBACK_H
