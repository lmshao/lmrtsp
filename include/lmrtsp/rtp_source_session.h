/**
 * @author SHAO Liming <lmshao@163.com>
 * @copyright Copyright (c) 2025 SHAO Liming
 * @license MIT
 *
 * SPDX-License-Identifier: MIT
 */

#ifndef LMSHAO_LMRTSP_RTP_SOURCE_SESSION_H
#define LMSHAO_LMRTSP_RTP_SOURCE_SESSION_H

#include <memory>
#include <string>

#include "lmrtsp/media_types.h"
#include "lmrtsp/transport_config.h"

namespace lmshao::lmrtsp {

// Forward declarations
class IRtpTransportAdapter;
class IRtpPacketizer;
class IRtpPacketizerListener;

struct RtpSourceSessionConfig {
    std::string session_id; // Unique session identifier
    uint32_t ssrc = 0;      // RTP synchronization source identifier, 0 means auto-generate

    MediaType video_type = MediaType::H264; // Video codec type
    uint8_t video_payload_type = 96;        // Video RTP payload type

    TransportConfig transport;
    uint32_t mtu_size = 1400; // Maximum transmission unit

    bool enable_rtcp = false;          // Enable RTCP
    uint32_t send_buffer_size = 65536; // Send buffer size (bytes)
};

class RtpSourceSession {
public:
    RtpSourceSession();
    ~RtpSourceSession();

    // Initialize session with configuration
    bool Initialize(const RtpSourceSessionConfig &config);

    // Session control
    bool Start(); // Start RTP session
    void Stop();  // Stop RTP session

    // Media frame sending
    bool SendFrame(const std::shared_ptr<MediaFrame> &frame);

private:
    // Forward declaration for listener
    class PacketizerListener;

    RtpSourceSessionConfig config_;
    bool initialized_ = false;
    bool running_ = false;

    // RTP state
    uint16_t sequence_number_ = 0;
    uint32_t timestamp_ = 0;

    // Transport and packetizers
    std::unique_ptr<IRtpTransportAdapter> transport_adapter_;
    std::unique_ptr<IRtpPacketizer> video_packetizer_;
    std::shared_ptr<IRtpPacketizerListener> video_listener_;
};

} // namespace lmshao::lmrtsp
#endif // LMSHAO_LMRTSP_RTP_SOURCE_SESSION_H
