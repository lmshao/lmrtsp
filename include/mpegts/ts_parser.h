/**
 * @author SHAO Liming <lmshao@163.com>
 * @copyright Copyright (c) 2025 SHAO Liming
 * @license MIT
 *
 * SPDX-License-Identifier: MIT
 */

#ifndef LMSHAO_MPEGTS_TS_PARSER_H
#define LMSHAO_MPEGTS_TS_PARSER_H

#include <map>
#include <memory>
#include <set>
#include <vector>

#include "ts_callback.h"
#include "ts_types.h"

using namespace lmshao::coreutils;

namespace lmshao::mpegts {

class TSStreamParser {
public:
    TSStreamParser();
    ~TSStreamParser();

    void SetCallback(std::shared_ptr<TSDemuxerListener> callback);

    void ParseData(const uint8_t *data, size_t size);
    void ParsePacket(const TSPacket &packet);

    void Start();
    void Stop();
    void Reset();

    void EnableVideoStream(uint16_t pid, bool enable = true);
    void EnableAudioStream(uint16_t pid, bool enable = true);
    void EnablePCRParsing(bool enable = true);

    bool IsRunning() const;
    size_t GetParsedPackets() const;
    std::vector<uint16_t> GetActiveVideoPIDs() const;
    std::vector<uint16_t> GetActiveAudioPIDs() const;

private:
    std::shared_ptr<TSDemuxerListener> callback_;
    std::map<uint16_t, std::shared_ptr<DataBuffer>> stream_buffers_;
    std::map<uint16_t, uint64_t> last_pcr_;
    std::set<uint16_t> enabled_video_pids_;
    std::set<uint16_t> enabled_audio_pids_;
    std::set<uint16_t> pmt_pids_; // PMT PID list dynamically obtained from PAT
    bool running_;
    size_t parsed_packets_;
    bool pcr_parsing_enabled_;

    void ProcessPAT(const std::shared_ptr<DataBuffer> &data);
    void ProcessPMT(uint16_t pid, const std::shared_ptr<DataBuffer> &data);
    void ProcessStreamData(uint16_t pid, const std::shared_ptr<DataBuffer> &data);
    void ProcessPCR(uint16_t pid, uint64_t pcr);

    bool IsVideoPID(uint16_t pid) const;
    bool IsAudioPID(uint16_t pid) const;
    bool HasPCR(const TSPacket &packet) const;
    uint64_t ExtractPCR(const TSPacket &packet);
};

} // namespace lmshao::mpegts

#endif // LMSHAO_MPEGTS_TS_PARSER_H
