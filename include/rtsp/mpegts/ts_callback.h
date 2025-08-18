/**
 * @author SHAO Liming <lmshao@163.com>
 * @copyright Copyright (c) 2025 SHAO Liming
 * @license MIT
 *
 * SPDX-License-Identifier: MIT
 */

#ifndef RTSP_TS_CALLBACK_H
#define RTSP_TS_CALLBACK_H

#include <map>
#include <memory>
#include <string>
#include <vector>

namespace lmshao::rtsp::mpegts {

// TS packet callback interface
class TSPacketCallback {
public:
    virtual ~TSPacketCallback() = default;

    virtual void OnVideoData(uint16_t pid, const uint8_t *data, size_t size, uint64_t pcr = 0) = 0;
    virtual void OnAudioData(uint16_t pid, const uint8_t *data, size_t size, uint64_t pcr = 0) = 0;

    virtual void OnPATReceived(const std::vector<uint16_t> &program_pids) = 0;
    virtual void OnPMTReceived(uint16_t program_pid, const std::map<uint16_t, uint8_t> &stream_pids) = 0;

    virtual void OnParseError(const std::string &error) = 0;
    virtual void OnSyncLoss() = 0;
};

} // namespace lmshao::rtsp::mpegts

#endif // RTSP_TS_CALLBACK_H
