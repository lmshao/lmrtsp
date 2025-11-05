/**
 * @author SHAO Liming <lmshao@163.com>
 * @copyright Copyright (c) 2025 SHAO Liming
 * @license MIT
 *
 * SPDX-License-Identifier: MIT
 */

#ifndef LMSHAO_LMRTSP_RTCP_PACKET_H
#define LMSHAO_LMRTSP_RTCP_PACKET_H

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "rtcp_def.h"

namespace lmshao::lmrtsp {

/**
 * RTCP Header (RFC 3550)
 *
 *  0                   1                   2                   3
 *  0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 * |V=2|P|    RC   |       PT      |             length            |
 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 */
struct RtcpHeader {
    uint8_t count : 5;   // Reception report count (RC) or Source count (SC)
    uint8_t padding : 1; // Padding flag
    uint8_t version : 2; // Version (always 2)
    uint8_t packetType;  // Packet type
    uint16_t length;     // Length in 32-bit words minus one

    /**
     * Set packet size in bytes
     */
    void SetSize(size_t sizeBytes);

    /**
     * Get packet size in bytes
     */
    size_t GetSize() const;

    /**
     * Get padding size in bytes
     */
    size_t GetPaddingSize() const;
};

/**
 * RTCP Report Block (RFC 3550)
 * Used in SR and RR packets
 */
struct RtcpReportBlock {
    uint32_t ssrc;                // SSRC of source
    uint32_t fractionLost : 8;    // Fraction lost since last SR/RR
    uint32_t cumulativeLost : 24; // Cumulative number of packets lost
    uint32_t extendedSeqNum;      // Extended highest sequence number received
    uint32_t jitter;              // Interarrival jitter
    uint32_t lastSr;              // Last SR timestamp (LSR)
    uint32_t delaySinceLastSr;    // Delay since last SR (DLSR)
};

/**
 * RTCP Sender Report (SR) Packet (RFC 3550)
 *
 *  0                   1                   2                   3
 *  0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 * |V=2|P|    RC   |   PT=SR=200   |             length            |
 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 * |                         SSRC of sender                        |
 * +=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+
 * |              NTP timestamp, most significant word             |
 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 * |             NTP timestamp, least significant word             |
 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 * |                         RTP timestamp                         |
 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 * |                     sender's packet count                     |
 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 * |                      sender's octet count                     |
 * +=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+
 * |                 report block(s) (variable)                    |
 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 */
struct RtcpSenderReport : public RtcpHeader {
    uint32_t ssrc;          // SSRC of sender
    uint32_t ntpTimestampH; // NTP timestamp - high 32 bits
    uint32_t ntpTimestampL; // NTP timestamp - low 32 bits
    uint32_t rtpTimestamp;  // RTP timestamp
    uint32_t packetCount;   // Sender's packet count
    uint32_t octetCount;    // Sender's octet count

    /**
     * Create SR packet with specified number of report blocks
     */
    static std::shared_ptr<RtcpSenderReport> Create(size_t reportCount = 0);

    /**
     * Set SSRC
     */
    RtcpSenderReport &SetSsrc(uint32_t ssrc);

    /**
     * Set NTP timestamp from system time (milliseconds)
     */
    RtcpSenderReport &SetNtpTimestamp(uint64_t unixTimeMs);

    /**
     * Set RTP timestamp
     */
    RtcpSenderReport &SetRtpTimestamp(uint32_t timestamp);

    /**
     * Set packet and octet counts
     */
    RtcpSenderReport &SetCounts(uint32_t packets, uint32_t octets);

    /**
     * Get report blocks
     */
    std::vector<RtcpReportBlock *> GetReportBlocks();

    /**
     * Get NTP timestamp in Unix milliseconds
     */
    uint64_t GetNtpUnixMs() const;
};

/**
 * RTCP Receiver Report (RR) Packet (RFC 3550)
 *
 *  0                   1                   2                   3
 *  0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 * |V=2|P|    RC   |   PT=RR=201   |             length            |
 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 * |                     SSRC of packet sender                     |
 * +=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+
 * |                 report block(s) (variable)                    |
 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 */
struct RtcpReceiverReport : public RtcpHeader {
    uint32_t ssrc; // SSRC of packet sender

    /**
     * Create RR packet with specified number of report blocks
     */
    static std::shared_ptr<RtcpReceiverReport> Create(size_t reportCount = 0);

    /**
     * Get report blocks
     */
    std::vector<RtcpReportBlock *> GetReportBlocks();
};

/**
 * SDES Item (wire format)
 */
struct SdesItem {
    SdesType type;
    uint8_t length;
    char text[1]; // Variable length
};

/**
 * SDES Item Helper (for building SDES packets)
 */
struct SdesItemInfo {
    SdesType type;
    std::string text;

    SdesItemInfo(SdesType t, const std::string &txt) : type(t), text(txt) {}
};

/**
 * SDES Chunk (one SSRC with multiple items)
 */
struct SdesChunk {
    uint32_t ssrc;
    std::vector<SdesItemInfo> items;

    SdesChunk(uint32_t s) : ssrc(s) {}
    void AddItem(SdesType type, const std::string &text) { items.emplace_back(type, text); }
};

/**
 * RTCP Source Description (SDES) Packet (RFC 3550)
 */
struct RtcpSdes : public RtcpHeader {
    /**
     * Create SDES packet with multiple chunks
     */
    static std::shared_ptr<RtcpSdes> Create(const std::vector<SdesChunk> &chunks);

    /**
     * Create SDES packet (deprecated - for backward compatibility)
     */
    static std::shared_ptr<RtcpSdes> Create(const std::vector<std::pair<uint32_t, std::string>> &items);
};

/**
 * RTCP BYE Packet (RFC 3550)
 */
struct RtcpBye : public RtcpHeader {
    /**
     * Create BYE packet
     */
    static std::shared_ptr<RtcpBye> Create(const std::vector<uint32_t> &ssrcs, const std::string &reason = "");

    /**
     * Get SSRCs
     */
    std::vector<uint32_t> GetSsrcs() const;

    /**
     * Get reason for leaving
     */
    std::string GetReason() const;
};

/**
 * RTCP Feedback Message (RFC 4585)
 */
struct RtcpFeedback : public RtcpHeader {
    uint32_t senderSsrc; // SSRC of packet sender
    uint32_t mediaSsrc;  // SSRC of media source

    /**
     * Get FCI (Feedback Control Information) pointer
     */
    const uint8_t *GetFci() const;

    /**
     * Get FCI size
     */
    size_t GetFciSize() const;

    /**
     * Create PSFB packet
     */
    static std::shared_ptr<RtcpFeedback> CreatePsfb(PsfbType fmt, const void *fci = nullptr, size_t fciLen = 0);

    /**
     * Create RTPFB packet
     */
    static std::shared_ptr<RtcpFeedback> CreateRtpfb(RtpfbType fmt, const void *fci = nullptr, size_t fciLen = 0);
};

/**
 * NACK Feedback Control Information
 */
struct NackItem {
    uint16_t pid; // Packet ID
    uint16_t blp; // Bitmask of following lost packets

    NackItem(uint16_t packetId, uint16_t bitmask);
};

/**
 * Helper functions for NTP timestamp conversion
 */
namespace RtcpUtils {
/**
 * Calculate LSR (Last SR timestamp) from NTP timestamp
 * LSR is the middle 32 bits of the 64-bit NTP timestamp
 */
uint32_t GetLsrFromNtp(uint32_t ntpH, uint32_t ntpL);

} // namespace RtcpUtils

} // namespace lmshao::lmrtsp

#endif // LMSHAO_LMRTSP_RTCP_PACKET_H
