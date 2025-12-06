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
#include <string>

#include "lmcore/data_buffer.h"

namespace lmshao::lmrtsp {

/**
 * @brief RTP Payload Type enumeration
 */
enum class MediaType : uint8_t {
    // Static payload types (RFC 3551)
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

/**
 * @brief Codec name constants
 *
 */
namespace Codec {
constexpr const char *H264 = "H264";
constexpr const char *H265 = "H265";
constexpr const char *AAC = "AAC";
constexpr const char *MP2T = "MP2T";
constexpr const char *MKV = "MKV";
constexpr const char *PCMU = "PCMU";
constexpr const char *PCMA = "PCMA";
constexpr const char *MPA = "MPA";
constexpr const char *JPEG = "JPEG";
constexpr const char *H261 = "H261";
constexpr const char *MPV = "MPV";
} // namespace Codec

namespace MediaKind {
constexpr const char *VIDEO = "video";
constexpr const char *AUDIO = "audio";
constexpr const char *MULTI = "multi"; // For multi-track containers (e.g., MKV)
} // namespace MediaKind

/**
 * @brief Convert MediaType enum to codec name string
 *
 * @param type RTP payload type
 * @return Codec name string, or empty string if unknown
 */
inline const char *MediaTypeToCodec(MediaType type)
{
    switch (type) {
        case MediaType::H264:
            return Codec::H264;
        case MediaType::H265:
            return Codec::H265;
        case MediaType::AAC:
            return Codec::AAC;
        case MediaType::MP2T:
            return Codec::MP2T;
        case MediaType::PCMU:
            return Codec::PCMU;
        case MediaType::PCMA:
            return Codec::PCMA;
        case MediaType::MPA:
            return Codec::MPA;
        case MediaType::JPEG:
            return Codec::JPEG;
        case MediaType::H261:
            return Codec::H261;
        case MediaType::MPV:
            return Codec::MPV;
        default:
            return "";
    }
}

/**
 * @brief Convert codec name string to MediaType enum
 *
 */
inline MediaType CodecToMediaType(const std::string &codec)
{
    if (codec == Codec::H264)
        return MediaType::H264;
    if (codec == Codec::H265)
        return MediaType::H265;
    if (codec == Codec::AAC)
        return MediaType::AAC;
    if (codec == Codec::MP2T)
        return MediaType::MP2T;
    if (codec == Codec::PCMU)
        return MediaType::PCMU;
    if (codec == Codec::PCMA)
        return MediaType::PCMA;
    if (codec == Codec::MPA)
        return MediaType::MPA;
    if (codec == Codec::JPEG)
        return MediaType::JPEG;
    if (codec == Codec::H261)
        return MediaType::H261;
    if (codec == Codec::MPV)
        return MediaType::MPV;
    return MediaType::UNKNOWN;
}

/**
 * @brief Determine media kind (video/audio) from codec name
 *
 */
inline const char *CodecToMediaKind(const std::string &codec)
{
    if (codec == Codec::H264 || codec == Codec::H265 || codec == Codec::MP2T || codec == Codec::MPV ||
        codec == Codec::JPEG || codec == Codec::H261) {
        return MediaKind::VIDEO;
    }
    if (codec == Codec::AAC || codec == Codec::PCMU || codec == Codec::PCMA || codec == Codec::MPA) {
        return MediaKind::AUDIO;
    }
    return "";
}

/**
 * @brief Determine media kind (video/audio) from MediaType enum
 *
 */
inline const char *MediaTypeToMediaKind(MediaType type)
{
    switch (type) {
        case MediaType::H264:
        case MediaType::H265:
        case MediaType::MP2T:
        case MediaType::MPV:
        case MediaType::JPEG:
        case MediaType::H261:
            return MediaKind::VIDEO;
        case MediaType::AAC:
        case MediaType::PCMU:
        case MediaType::PCMA:
        case MediaType::MPA:
            return MediaKind::AUDIO;
        default:
            return "";
    }
}

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