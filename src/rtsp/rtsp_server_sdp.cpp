/**
 * @author SHAO Liming <lmshao@163.com>
 * @copyright Copyright (c) 2025 SHAO Liming
 * @license MIT
 *
 * SPDX-LICENSE-Identifier: MIT
 */

#include "internal_logger.h"
#include "lmcore/base64.h"
#include "lmcore/hex.h"
#include "lmrtsp/media_types.h"
#include "lmrtsp/rtsp_server.h"

using namespace lmshao::lmcore;

namespace lmshao::lmrtsp {

// Helper function to generate SDP for a single track
static std::string GenerateTrackSDP(const std::shared_ptr<MediaStreamInfo> &track_info, int track_index)
{
    std::string sdp;

    if (track_info->media_type == MediaKind::VIDEO) {
        // Use UDP mode (RTP/AVP), not TCP
        sdp += "m=video 0 RTP/AVP " + std::to_string(track_info->payload_type) + "\r\n";
        sdp += "a=rtpmap:" + std::to_string(track_info->payload_type) + " " + track_info->codec + "/" +
               std::to_string(track_info->clock_rate) + "\r\n";

        // Add fmtp with H.264 parameters if available
        if (track_info->codec == "H264" && !track_info->sps.empty() && !track_info->pps.empty()) {
            // Extract profile-level-id from SPS (bytes 1-3)
            std::string profileLevelId = "42001f"; // Default: Baseline Profile Level 3.1
            if (track_info->sps.size() >= 4) {
                std::vector<uint8_t> profile_bytes = {track_info->sps[1], track_info->sps[2], track_info->sps[3]};
                profileLevelId = Hex::Encode(profile_bytes);
            }

            // Base64 encode SPS and PPS
            std::string spsBase64 = Base64::Encode(track_info->sps);
            std::string ppsBase64 = Base64::Encode(track_info->pps);

            sdp += "a=fmtp:" + std::to_string(track_info->payload_type) +
                   " packetization-mode=1;profile-level-id=" + profileLevelId + ";sprop-parameter-sets=" + spsBase64 +
                   "," + ppsBase64 + "\r\n";
        }
        // Add fmtp with H.265 parameters if available (RFC 7798)
        else if (track_info->codec == "H265" && !track_info->vps.empty() && !track_info->sps.empty() &&
                 !track_info->pps.empty()) {
            // Base64 encode VPS, SPS and PPS
            std::string vpsBase64 = Base64::Encode(track_info->vps);
            std::string spsBase64 = Base64::Encode(track_info->sps);
            std::string ppsBase64 = Base64::Encode(track_info->pps);

            sdp += "a=fmtp:" + std::to_string(track_info->payload_type) + " sprop-vps=" + vpsBase64 +
                   ";sprop-sps=" + spsBase64 + ";sprop-pps=" + ppsBase64 + "\r\n";
        }

        if (track_info->width > 0 && track_info->height > 0) {
            sdp += "a=framerate:" + std::to_string(track_info->frame_rate) + "\r\n";
        }

        // Media-level control attribute - use relative path
        sdp += "a=control:track" + std::to_string(track_index) + "\r\n";

    } else if (track_info->media_type == MediaKind::AUDIO) {
        sdp += "m=audio 0 RTP/AVP " + std::to_string(track_info->payload_type) + "\r\n";

        // Use RFC 3640 compliant codec name for AAC
        std::string codec_name = track_info->codec;
        if (codec_name == "AAC") {
            codec_name = "mpeg4-generic"; // RFC 3640 standard name
        }

        sdp += "a=rtpmap:" + std::to_string(track_info->payload_type) + " " + codec_name + "/" +
               std::to_string(track_info->sample_rate);

        // Add channel information to rtpmap
        if (track_info->channels > 0) {
            sdp += "/" + std::to_string(track_info->channels);
        }
        sdp += "\r\n";

        // Add fmtp for AAC (RFC 3640)
        if (track_info->codec == "AAC") {
            // Generate AudioSpecificConfig for AAC-LC
            // Format: profile(5bits) + sampling_freq_index(4bits) + channel_config(4bits)
            // AAC-LC profile = 2 (0b00010)
            int sampling_freq_index = 15; // default invalid
            if (track_info->sample_rate == 96000)
                sampling_freq_index = 0;
            else if (track_info->sample_rate == 88200)
                sampling_freq_index = 1;
            else if (track_info->sample_rate == 64000)
                sampling_freq_index = 2;
            else if (track_info->sample_rate == 48000)
                sampling_freq_index = 3;
            else if (track_info->sample_rate == 44100)
                sampling_freq_index = 4;
            else if (track_info->sample_rate == 32000)
                sampling_freq_index = 5;
            else if (track_info->sample_rate == 24000)
                sampling_freq_index = 6;
            else if (track_info->sample_rate == 22050)
                sampling_freq_index = 7;
            else if (track_info->sample_rate == 16000)
                sampling_freq_index = 8;
            else if (track_info->sample_rate == 12000)
                sampling_freq_index = 9;
            else if (track_info->sample_rate == 11025)
                sampling_freq_index = 10;
            else if (track_info->sample_rate == 8000)
                sampling_freq_index = 11;

            // AudioSpecificConfig: profile(5) + freq_index(4) + channel(4) = 13 bits, pad to 16 bits
            uint16_t config = (2 << 11) | (sampling_freq_index << 7) | (track_info->channels << 3);
            char config_hex[8];
            snprintf(config_hex, sizeof(config_hex), "%04X", config);

            // RFC 3640 fmtp parameters
            sdp +=
                "a=fmtp:" + std::to_string(track_info->payload_type) +
                " streamtype=5;profile-level-id=1;mode=AAC-hbr;sizelength=13;indexlength=3;indexdeltalength=3;config=" +
                std::string(config_hex) + "\r\n";
        }

        // Media-level control attribute - use relative path
        sdp += "a=control:track" + std::to_string(track_index) + "\r\n";
    }

    return sdp;
}

// SDP generation implementation with multi-track support
std::string RtspServer::GenerateSDP(const std::string &stream_path, const std::string &server_ip, uint16_t server_port)
{
    // Extract path from full RTSP URL if needed
    std::string path = stream_path;
    if (stream_path.find("rtsp://") == 0) {
        // Find the path part after host:port
        size_t schemeEnd = stream_path.find("://");
        if (schemeEnd != std::string::npos) {
            size_t hostStart = schemeEnd + 3;
            size_t pathStart = stream_path.find('/', hostStart);
            if (pathStart != std::string::npos) {
                path = stream_path.substr(pathStart);
            }
        }
    }

    auto stream_info = GetMediaStream(path);
    if (!stream_info) {
        LMRTSP_LOGE("Media stream not found: %s (original: %s)", path.c_str(), stream_path.c_str());
        return "";
    }

    // Generate SDP header
    std::string sdp;
    sdp += "v=0\r\n";
    sdp += "o=- 0 0 IN IP4 " + server_ip + "\r\n";
    sdp += "s=RTSP Session\r\n";
    sdp += "c=IN IP4 " + server_ip + "\r\n";
    sdp += "t=0 0\r\n";
    sdp += "a=range:npt=0-\r\n"; // Add range attribute for VLC compatibility

    // Session-level control attribute - use wildcard
    sdp += "a=control:*\r\n";

    // Check if this stream has multiple sub-tracks (multi-track container like MKV)
    if (!stream_info->sub_tracks.empty()) {
        LMRTSP_LOGD("Generating multi-track SDP for %zu tracks", stream_info->sub_tracks.size());

        // Generate SDP for each sub-track
        for (size_t i = 0; i < stream_info->sub_tracks.size(); ++i) {
            sdp += GenerateTrackSDP(stream_info->sub_tracks[i], static_cast<int>(i));
        }
    } else {
        // Single track stream (legacy behavior)
        LMRTSP_LOGD("Generating single-track SDP");
        sdp += GenerateTrackSDP(stream_info, 0);
    }

    return sdp;
}

} // namespace lmshao::lmrtsp
