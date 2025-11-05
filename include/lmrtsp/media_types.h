/**
 * @author SHAO Liming <lmshao@163.com>
 * @copyright Copyright (c) 2025 SHAO Liming
 * @license MIT
 *
 * SPDX-License-Identifier: MIT
 */

#ifndef LMSHAO_LMRTSP_MEDIA_TYPES_H
#define LMSHAO_LMRTSP_MEDIA_TYPES_H

#include <cstdint>
#include <memory>

#include "lmcore/data_buffer.h"

namespace lmshao::lmrtsp {

// RTP Payload Types (subset)
enum class MediaType : uint8_t {
    // Static payload types
    PCMU = 0,  // G.711 mu-law
    PCMA = 8,  // G.711 A-law
    MPA = 14,  // MPEG audio
    JPEG = 26, // JPEG
    H261 = 31, // H.261 video
    MPV = 32,  // MPEG video
    MP2T = 33, // MPEG-2 TS

    // Dynamic payload types (commonly used in SDP)
    H264 = 96,
    AAC = 97,
    H265 = 98,
    UNKNOWN = 255,
};

struct VideoParam {
    uint32_t width = 0;
    uint32_t height = 0;
    uint32_t frame_rate = 0;
    bool is_key_frame = false;
};

struct AudioParam {
    uint32_t sample_rate = 0;
    uint32_t channels = 0;
};

struct MediaFrame {
    std::shared_ptr<lmcore::DataBuffer> data;
    uint32_t timestamp = 0;
    MediaType media_type = MediaType::H264;
    union {
        VideoParam video_param;
        AudioParam audio_param;
    };

    MediaFrame() { new (&video_param) VideoParam{}; }
};

} // namespace lmshao::lmrtsp

#endif // LMSHAO_LMRTSP_MEDIA_TYPES_H