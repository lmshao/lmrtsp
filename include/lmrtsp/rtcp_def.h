/**
 * @author SHAO Liming <lmshao@163.com>
 * @copyright Copyright (c) 2025 SHAO Liming
 * @license MIT
 *
 * SPDX-License-Identifier: MIT
 */

#ifndef LMSHAO_LMRTSP_RTCP_DEF_H
#define LMSHAO_LMRTSP_RTCP_DEF_H

#include <cstdint>

namespace lmshao::lmrtsp {

/**
 * RTCP packet types (RFC 3550)
 */
enum class RtcpType : uint8_t {
    FIR = 192,     // Full INTRA-frame Request (H.261)
    NACK = 193,    // Negative Acknowledgment
    SMPTETC = 194, // SMPTE time-code mapping
    IJ = 195,      // Extended inter-arrival jitter report
    SR = 200,      // Sender Report
    RR = 201,      // Receiver Report
    SDES = 202,    // Source Description
    BYE = 203,     // Goodbye
    APP = 204,     // Application-defined
    RTPFB = 205,   // Generic RTP Feedback (RFC 4585)
    PSFB = 206,    // Payload-specific Feedback (RFC 4585)
    XR = 207,      // Extended Report (RFC 3611)
    AVB = 208,     // AVB RTCP packet (IEEE 1733)
    RSI = 209,     // Receiver Summary Information
    TOKEN = 210    // Port Mapping
};

/**
 * SDES item types (RFC 3550)
 */
enum class SdesType : uint8_t {
    END = 0,   // End of SDES list
    CNAME = 1, // Canonical End-Point Identifier
    NAME = 2,  // User Name
    EMAIL = 3, // Electronic Mail Address
    PHONE = 4, // Phone Number
    LOC = 5,   // Geographic User Location
    TOOL = 6,  // Application or Tool Name
    NOTE = 7,  // Notice/Status
    PRIV = 8   // Private Extensions
};

/**
 * Payload-specific Feedback Message Types (RFC 4585)
 */
enum class PsfbType : uint8_t {
    PLI = 1,  // Picture Loss Indication
    SLI = 2,  // Slice Loss Indication
    RPSI = 3, // Reference Picture Selection Indication
    FIR = 4,  // Full Intra Request Command
    TSTR = 5, // Temporal-Spatial Trade-off Request
    TSTN = 6, // Temporal-Spatial Trade-off Notification
    VBCM = 7, // Video Back Channel Message
    REMB = 15 // Receiver Estimated Maximum Bitrate
};

/**
 * Transport layer Feedback Message Types (RFC 4585)
 */
enum class RtpfbType : uint8_t {
    NACK = 1,        // Generic NACK
    TMMBR = 3,       // Temporary Maximum Media Stream Bit Rate Request
    TMMBN = 4,       // Temporary Maximum Media Stream Bit Rate Notification
    RTCP_SR_REQ = 5, // RTCP Rapid Resynchronization Request
    RAMS = 6,        // Rapid Acquisition of Multicast Sessions
    TLLEI = 7,       // Transport-Layer Third-Party Loss Early Indication
    RTCP_ECN_FB = 8, // RTCP ECN Feedback
    TWCC = 15        // Transport Wide Congestion Control
};

// RTCP constants
constexpr uint8_t RTCP_VERSION = 2;
constexpr size_t RTCP_HEADER_SIZE = 4;
constexpr size_t RTCP_SR_SIZE = 28;
constexpr size_t RTCP_RR_SIZE = 8;
constexpr size_t RTCP_REPORT_BLOCK_SIZE = 24;

// NTP constants
constexpr uint64_t NTP_OFFSET_US = 2208988800000000ULL; // NTP epoch offset in microseconds

} // namespace lmshao::lmrtsp

#endif // LMSHAO_LMRTSP_RTCP_DEF_H
