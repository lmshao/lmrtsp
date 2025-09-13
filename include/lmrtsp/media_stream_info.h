#ifndef LMSHAO_LMRTSP_MEDIA_STREAM_INFO_H
#define LMSHAO_LMRTSP_MEDIA_STREAM_INFO_H

#include <cstdint>
#include <string>
#include <vector>

namespace lmshao::lmrtsp {

/**
 * @brief Media stream information structure
 */
struct MediaStreamInfo {
    // Basic information
    std::string stream_path;
    std::string media_type;
    std::string codec;

    // Video parameters
    uint32_t width = 0;
    uint32_t height = 0;
    uint32_t frame_rate = 30;
    uint32_t bitrate = 0;

    // Audio parameters
    uint32_t sample_rate = 0;
    uint32_t channels = 0;
    uint32_t bits_per_sample = 0;

    // Encoding parameters
    std::string profile_level;
    std::vector<uint8_t> sps;
    std::vector<uint8_t> pps;
    std::vector<uint8_t> vps;

    // RTP parameters
    uint8_t payload_type = 96;
    uint32_t ssrc = 0;
    uint32_t clock_rate = 90000;

    // Transport parameters
    uint16_t rtp_port = 0;
    uint16_t rtcp_port = 0;
    std::string multicast_ip;
    uint8_t ttl = 64;

    // Control parameters
    bool enabled = true;
    uint32_t max_packet_size = 1400;

    // Constructor
    MediaStreamInfo() = default;

    // Validation method
    bool IsValid() const { return !stream_path.empty() && !media_type.empty() && !codec.empty(); }

    std::string GenerateSDPMediaLine() const
    {
        if (media_type == "video") {
            return "m=video 0 RTP/AVP " + std::to_string(payload_type);
        }

        if (media_type == "audio") {
            return "m=audio 0 RTP/AVP " + std::to_string(payload_type);
        }
        return "";
    }

    std::vector<std::string> GenerateSDPAttributes() const
    {
        std::vector<std::string> attributes;
        attributes.push_back("a=rtpmap:" + std::to_string(payload_type) + " " + codec + "/" +
                             std::to_string(clock_rate));
        if (!profile_level.empty()) {
            attributes.push_back("a=fmtp:" + std::to_string(payload_type) + " profile-level-id=" + profile_level);
        }
        attributes.push_back("a=control:track" + std::to_string(payload_type));
        return attributes;
    }
};

} // namespace lmshao::lmrtsp

#endif // LMSHAO_LMRTSP_MEDIA_STREAM_INFO_H