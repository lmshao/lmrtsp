/**
 * @author SHAO Liming <lmshao@163.com>
 * @copyright Copyright (c) 2025 SHAO Liming
 * @license MIT
 *
 * SPDX-License-Identifier: MIT
 */

#ifndef LMSHAO_LMRTSP_RTCP_CONTEXT_H
#define LMSHAO_LMRTSP_RTCP_CONTEXT_H

#include <cstdint>
#include <map>
#include <memory>
#include <vector>

#include "rtcp_def.h"
#include "rtcp_packet.h"

namespace lmshao::lmcore {
class DataBuffer;
}

namespace lmshao::lmrtsp {

/**
 * Base class for RTCP context management
 * Handles RTP/RTCP statistics and packet generation
 */
class RtcpContext {
public:
    virtual ~RtcpContext() = default;

    /**
     * Initialize context with SSRC values
     * @param rtcpSsrc SSRC for RTCP packets
     * @param rtpSsrc SSRC for RTP stream
     */
    virtual void Initialize(uint32_t rtcpSsrc, uint32_t rtpSsrc);

    /**
     * Process incoming RTCP packet
     * @param rtcp RTCP header pointer
     * @param size Packet size in bytes
     */
    virtual void OnRtcp(const uint8_t *data, size_t size);

    /**
     * Process outgoing RTP packet (for statistics)
     * @param seq RTP sequence number
     * @param timestamp RTP timestamp
     * @param ntpTimestampMs NTP timestamp in milliseconds
     * @param sampleRate Sample rate (e.g., 90000 for video)
     * @param bytes RTP packet size in bytes
     */
    virtual void OnRtp(uint16_t seq, uint32_t timestamp, uint64_t ntpTimestampMs, uint32_t sampleRate, size_t bytes);

    /**
     * Create RTCP Sender Report
     * @return Data buffer containing SR packet, or nullptr if not initialized
     */
    virtual std::shared_ptr<lmcore::DataBuffer> CreateRtcpSr();

    /**
     * Create RTCP Receiver Report
     * @return Data buffer containing RR packet, or nullptr if not initialized
     */
    virtual std::shared_ptr<lmcore::DataBuffer> CreateRtcpRr();

    /**
     * Create RTCP SDES packet with CNAME
     * @param cname Canonical name (e.g., "user@host")
     * @param name Optional user name
     * @return Data buffer containing SDES packet, or nullptr if not initialized
     */
    virtual std::shared_ptr<lmcore::DataBuffer> CreateRtcpSdes(const std::string &cname, const std::string &name = "");

    /**
     * Create RTCP BYE packet
     * @param reason Optional reason for leaving
     * @return Data buffer containing BYE packet, or nullptr if not initialized
     */
    virtual std::shared_ptr<lmcore::DataBuffer> CreateRtcpBye(const std::string &reason = "");

    /**
     * Create compound RTCP packet (SR/RR + SDES)
     * @param cname Canonical name for SDES
     * @param name Optional user name for SDES
     * @return Data buffer containing compound packet, or nullptr if not initialized
     */
    virtual std::shared_ptr<lmcore::DataBuffer> CreateCompoundPacket(const std::string &cname,
                                                                     const std::string &name = "");

    /**
     * Get total lost packets
     */
    virtual size_t GetLost() const { return 0; }

    /**
     * Get lost packets in current interval
     */
    virtual size_t GetLostInterval() const { return 0; }

    /**
     * Get total expected packets
     */
    virtual size_t GetExpectedPackets() const { return 0; }

    /**
     * Get expected packets in current interval
     */
    virtual size_t GetExpectedPacketsInterval() const { return 0; }

protected:
    uint32_t rtcpSsrc_ = 0;           // SSRC for RTCP packets
    uint32_t rtpSsrc_ = 0;            // SSRC for RTP stream
    uint32_t lastRtpTimestamp_ = 0;   // Last RTP timestamp
    size_t totalBytes_ = 0;           // Total bytes sent/received
    size_t totalPackets_ = 0;         // Total packets sent/received
    uint64_t lastNtpTimestampMs_ = 0; // Last NTP timestamp in milliseconds
};

/**
 * RTCP Sender Context
 * Manages RTCP for RTP sender (generates SR, processes RR)
 */
class RtcpSenderContext : public RtcpContext {
public:
    using Ptr = std::shared_ptr<RtcpSenderContext>;

    /**
     * Create sender context
     */
    static Ptr Create();

    /**
     * Process incoming RTCP packet (RR, XR, etc.)
     */
    void OnRtcp(const uint8_t *data, size_t size) override;

    /**
     * Create RTCP Sender Report
     */
    std::shared_ptr<lmcore::DataBuffer> CreateRtcpSr() override;

    /**
     * Get RTT (Round-Trip Time) for specific SSRC
     * @param ssrc SSRC of receiver
     * @return RTT in milliseconds
     */
    uint32_t GetRtt(uint32_t ssrc) const;

    /**
     * Get average RTT across all receivers
     * @return Average RTT in milliseconds
     */
    uint32_t GetAverageRtt() const;

private:
    void ProcessReceiverReport(const RtcpReceiverReport *rr);

    std::map<uint32_t, uint32_t> rttMap_;                // SSRC -> RTT (ms)
    std::map<uint32_t, uint64_t> senderReportNtpMap_;    // LSR -> NTP timestamp (ms)
    std::map<uint32_t, uint64_t> receiverReportTimeMap_; // SSRC -> RR receive time (ms)
};

/**
 * RTCP Receiver Context
 * Manages RTCP for RTP receiver (generates RR, processes SR)
 */
class RtcpReceiverContext : public RtcpContext {
public:
    using Ptr = std::shared_ptr<RtcpReceiverContext>;

    /**
     * Create receiver context
     */
    static Ptr Create();

    /**
     * Process incoming RTCP packet (SR, BYE, etc.)
     */
    void OnRtcp(const uint8_t *data, size_t size) override;

    /**
     * Process incoming RTP packet
     */
    void OnRtp(uint16_t seq, uint32_t timestamp, uint64_t ntpTimestampMs, uint32_t sampleRate, size_t bytes) override;

    /**
     * Create RTCP Receiver Report
     */
    std::shared_ptr<lmcore::DataBuffer> CreateRtcpRr() override;

    /**
     * Get total lost packets
     */
    size_t GetLost() const override;

    /**
     * Get lost packets in current interval
     */
    size_t GetLostInterval() const override;

    /**
     * Get total expected packets
     */
    size_t GetExpectedPackets() const override;

    /**
     * Get expected packets in current interval
     */
    size_t GetExpectedPacketsInterval() const override;

    /**
     * Get packet loss rate (0.0 - 1.0)
     */
    double GetLossRate() const;

    /**
     * Get interarrival jitter
     */
    uint32_t GetJitter() const;

private:
    void ProcessSenderReport(const RtcpSenderReport *sr);
    void InitSequence(uint16_t seq);
    void UpdateSequence(uint16_t seq);
    void UpdateJitter(uint32_t timestamp, uint64_t ntpTimestampMs, uint32_t sampleRate);

    // Sequence number tracking
    uint16_t maxSeq_ = 0;         // Highest sequence number seen
    uint16_t baseSeq_ = 0;        // Base sequence number
    uint16_t cycles_ = 0;         // Sequence number cycles
    uint16_t lastSeq_ = 0;        // Last sequence number
    bool seqInitialized_ = false; // Sequence tracking initialized

    // SR tracking
    uint32_t lastSrLsr_ = 0;   // Last SR timestamp (LSR format)
    uint64_t lastSrNtpMs_ = 0; // Last SR NTP timestamp (ms)

    // Jitter calculation
    double jitter_ = 0.0;            // Interarrival jitter
    uint64_t lastArrivalTimeMs_ = 0; // Last packet arrival time

    // Loss tracking
    size_t lastLost_ = 0;         // Lost packets at last interval
    size_t lastExpected_ = 0;     // Expected packets at last interval
    size_t lastCyclePackets_ = 0; // Packets in last cycle
};

} // namespace lmshao::lmrtsp

#endif // LMSHAO_LMRTSP_RTCP_CONTEXT_H
