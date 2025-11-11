/**
 * @author SHAO Liming <lmshao@163.com>
 * @copyright Copyright (c) 2025 SHAO Liming
 * @license MIT
 *
 * SPDX-License-Identifier: MIT
 */

#ifndef LMSHAO_LMRTSP_IRTSP_CLIENT_LISTENER_H
#define LMSHAO_LMRTSP_IRTSP_CLIENT_LISTENER_H

#include <memory>
#include <string>

#include "lmrtsp/media_types.h"

namespace lmshao::lmrtsp {

struct MediaFrame;

/**
 * @brief RTSP client listener interface
 *
 * This interface defines listener methods for RTSP client to notify
 * upper layer applications about various events.
 */
class IRtspClientListener {
public:
    virtual ~IRtspClientListener() = default;

    /**
     * @brief Connection established event
     * @param server_url Server URL
     */
    virtual void OnConnected(const std::string &server_url) = 0;

    /**
     * @brief Connection lost event
     * @param server_url Server URL
     */
    virtual void OnDisconnected(const std::string &server_url) = 0;

    /**
     * @brief DESCRIBE response received
     * @param server_url Server URL
     * @param sdp SDP description
     */
    virtual void OnDescribeReceived(const std::string &server_url, const std::string &sdp) = 0;

    /**
     * @brief SETUP response received
     * @param server_url Server URL
     * @param session_id Session ID
     * @param transport Transport info
     */
    virtual void OnSetupReceived(const std::string &server_url, const std::string &session_id,
                                 const std::string &transport) = 0;

    /**
     * @brief PLAY response received
     * @param server_url Server URL
     * @param session_id Session ID
     * @param rtp_info RTP info
     */
    virtual void OnPlayReceived(const std::string &server_url, const std::string &session_id,
                                const std::string &rtp_info = "") = 0;

    /**
     * @brief PAUSE response received
     * @param server_url Server URL
     * @param session_id Session ID
     */
    virtual void OnPauseReceived(const std::string &server_url, const std::string &session_id) = 0;

    /**
     * @brief TEARDOWN response received
     * @param server_url Server URL
     * @param session_id Session ID
     */
    virtual void OnTeardownReceived(const std::string &server_url, const std::string &session_id) = 0;

    /**
     * @brief Media frame received
     * @param frame Media frame
     */
    virtual void OnFrame(const std::shared_ptr<MediaFrame> &frame) = 0;

    /**
     * @brief Error event
     * @param server_url Server URL
     * @param error_code Error code
     * @param error_message Error message
     */
    virtual void OnError(const std::string &server_url, int error_code, const std::string &error_message) = 0;

    /**
     * @brief Authentication required event
     * @param server_url Server URL
     * @param realm Authentication realm
     * @param nonce Authentication nonce
     */
    virtual void OnAuthenticationRequired(const std::string &server_url, const std::string &realm,
                                          const std::string &nonce)
    {
        // Default empty implementation
    }

    /**
     * @brief State changed event
     * @param server_url Server URL
     * @param old_state Previous state
     * @param new_state New state
     */
    virtual void OnStateChanged(const std::string &server_url, const std::string &old_state,
                                const std::string &new_state)
    {
        // Default empty implementation
    }
};

} // namespace lmshao::lmrtsp

#endif // LMSHAO_LMRTSP_IRTSP_CLIENT_LISTENER_H
