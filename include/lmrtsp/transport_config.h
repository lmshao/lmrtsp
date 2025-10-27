/**
 * @author SHAO Liming <lmshao@163.com>
 * @copyright Copyright (c) 2025 SHAO Liming
 * @license MIT
 *
 * SPDX-License-Identifier: MIT
 */

#ifndef LMSHAO_LMRTSP_TRANSPORT_CONFIG_H
#define LMSHAO_LMRTSP_TRANSPORT_CONFIG_H

#include <cstdint>
#include <string>
#include <utility>

namespace lmshao::lmrtsp {

struct TransportConfig {
    enum class Type {
        UDP,
        TCP_INTERLEAVED
    };

    enum class Mode {
        SOURCE, ///< RTP Source (sender)
        SINK    ///< RTP Sink (receiver)
    };

    Type type = Type::UDP;
    Mode mode = Mode::SOURCE;
    std::string client_ip;
    uint16_t client_rtp_port = 0;
    uint16_t client_rtcp_port = 0;
    uint16_t server_rtp_port = 0;
    uint16_t server_rtcp_port = 0;
    uint8_t rtpChannel = 0;
    uint8_t rtcpChannel = 1;
    std::pair<uint8_t, uint8_t> interleavedChannels = {0, 1};
    bool unicast = true;
};

} // namespace lmshao::lmrtsp

#endif // LMSHAO_LMRTSP_TRANSPORT_CONFIG_H