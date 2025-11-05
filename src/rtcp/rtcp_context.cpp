/**
 * @author SHAO Liming <lmshao@163.com>
 * @copyright Copyright (c) 2025 SHAO Liming
 * @license MIT
 *
 * SPDX-License-Identifier: MIT
 */

#include "lmrtsp/rtcp_context.h"

#include <lmcore/byte_order.h>
#include <lmcore/data_buffer.h>
#include <lmcore/time_utils.h>

#include <cmath>
#include <cstring>

#include "internal_logger.h"
#include "lmrtsp/rtcp_packet.h"

namespace lmshao::lmrtsp {

void RtcpContext::Initialize(uint32_t rtcpSsrc, uint32_t rtpSsrc)
{
    rtcpSsrc_ = rtcpSsrc;
    rtpSsrc_ = rtpSsrc;
}

void RtcpContext::OnRtcp(const lmcore::DataBuffer &buffer)
{
    // Base implementation - to be overridden
    (void)buffer;
}

void RtcpContext::OnRtp(uint16_t seq, uint32_t timestamp, uint64_t ntpTimestampMs, uint32_t sampleRate, size_t bytes)
{
    (void)seq;
    lastRtpTimestamp_ = timestamp;
    lastNtpTimestampMs_ = ntpTimestampMs;
    totalBytes_ += bytes;
    totalPackets_++;
    (void)sampleRate;
}

std::shared_ptr<lmcore::DataBuffer> RtcpContext::CreateRtcpSr()
{
    return nullptr;
}

std::shared_ptr<lmcore::DataBuffer> RtcpContext::CreateRtcpRr()
{
    return nullptr;
}

std::shared_ptr<lmcore::DataBuffer> RtcpContext::CreateRtcpSdes(const std::string &cname, const std::string &name)
{
    if (rtcpSsrc_ == 0) {
        LMRTSP_LOGE("RTCP context not initialized");
        return nullptr;
    }

    if (cname.empty()) {
        LMRTSP_LOGW("CNAME is empty, SDES not created");
        return nullptr;
    }

    // Build SDES chunk
    SdesChunk chunk(rtcpSsrc_);
    chunk.AddItem(SdesType::CNAME, cname);

    if (!name.empty()) {
        chunk.AddItem(SdesType::NAME, name);
    }

    std::vector<SdesChunk> chunks = {chunk};
    auto sdes = RtcpSdes::Create(chunks);
    if (!sdes) {
        return nullptr;
    }

    size_t sdesSize = sdes->GetSize();
    auto buffer = lmcore::DataBuffer::Create(sdesSize);
    buffer->Append(sdes.get(), sdesSize);

    LMRTSP_LOGD("Created SDES: SSRC=0x%08x, CNAME=%s", rtcpSsrc_, cname.c_str());
    return buffer;
}

std::shared_ptr<lmcore::DataBuffer> RtcpContext::CreateRtcpBye(const std::string &reason)
{
    if (rtcpSsrc_ == 0) {
        LMRTSP_LOGE("RTCP context not initialized");
        return nullptr;
    }

    std::vector<uint32_t> ssrcs = {rtcpSsrc_};
    auto bye = RtcpBye::Create(ssrcs, reason);
    if (!bye) {
        return nullptr;
    }

    size_t byeSize = bye->GetSize();
    auto buffer = lmcore::DataBuffer::Create(byeSize);
    buffer->Append(bye.get(), byeSize);

    LMRTSP_LOGD("Created BYE: SSRC=0x%08x, reason=%s", rtcpSsrc_, reason.empty() ? "(none)" : reason.c_str());
    return buffer;
}

std::shared_ptr<lmcore::DataBuffer> RtcpContext::CreateCompoundPacket(const std::string &cname, const std::string &name)
{
    // Create SR or RR first
    auto srOrRr = CreateRtcpSr();
    if (!srOrRr) {
        srOrRr = CreateRtcpRr();
    }
    if (!srOrRr) {
        return nullptr;
    }

    // Create SDES
    auto sdes = CreateRtcpSdes(cname, name);
    if (!sdes) {
        return srOrRr; // Return SR/RR only if SDES failed
    }

    // Combine into compound packet
    size_t totalSize = srOrRr->Size() + sdes->Size();
    auto buffer = lmcore::DataBuffer::Create(totalSize);

    buffer->Append(srOrRr);
    buffer->Append(sdes);

    LMRTSP_LOGD("Created compound packet: SR/RR + SDES, total size=%zu", totalSize);
    return buffer;
}

RtcpSenderContext::Ptr RtcpSenderContext::Create()
{
    return std::make_shared<RtcpSenderContext>();
}

void RtcpSenderContext::OnRtcp(const lmcore::DataBuffer &buffer)
{
    const uint8_t *data = buffer.Data();
    size_t size = buffer.Size();

    if (!data || size < sizeof(RtcpHeader)) {
        LMRTSP_LOGW("Invalid RTCP packet: data=%p, size=%zu", data, size);
        return;
    }

    const auto *header = reinterpret_cast<const RtcpHeader *>(data);

    // Check version
    if (header->version != RTCP_VERSION) {
        LMRTSP_LOGW("Invalid RTCP version: %d", header->version);
        return;
    }

    RtcpType type = static_cast<RtcpType>(header->packetType);

    switch (type) {
        case RtcpType::RR: {
            if (size >= sizeof(RtcpReceiverReport)) {
                const auto *rr = reinterpret_cast<const RtcpReceiverReport *>(data);
                ProcessReceiverReport(rr);
            }
            break;
        }
        case RtcpType::SR:
            LMRTSP_LOGD("Received SR (unexpected for sender context)");
            break;
        case RtcpType::BYE:
            LMRTSP_LOGI("Received BYE");
            break;
        default:
            LMRTSP_LOGD("Received RTCP packet type: %d", static_cast<int>(type));
            break;
    }
}

void RtcpSenderContext::ProcessReceiverReport(const RtcpReceiverReport *rr)
{
    if (!rr) {
        return;
    }

    uint64_t currentTimeMs = lmcore::TimeUtils::GetCurrentTimeMs();
    uint32_t senderSsrc = lmcore::ByteOrder::NetworkToHost32(rr->ssrc);

    // Process each report block
    auto blocks = const_cast<RtcpReceiverReport *>(rr)->GetReportBlocks();
    for (auto *block : blocks) {
        if (!block) {
            continue;
        }

        uint32_t lsr = lmcore::ByteOrder::NetworkToHost32(block->lastSr);
        uint32_t dlsr = lmcore::ByteOrder::NetworkToHost32(block->delaySinceLastSr);

        // Calculate RTT if we have sent an SR
        auto it = senderReportNtpMap_.find(lsr);
        if (it != senderReportNtpMap_.end() && dlsr > 0) {
            uint64_t srSentTimeMs = it->second;
            // DLSR is in units of 1/65536 seconds
            uint64_t dlsrMs = (static_cast<uint64_t>(dlsr) * 1000) / 65536;
            uint64_t rttMs = currentTimeMs - srSentTimeMs - dlsrMs;

            rttMap_[senderSsrc] = static_cast<uint32_t>(rttMs);
            LMRTSP_LOGD("RTT for SSRC 0x%08x: %u ms", senderSsrc, static_cast<uint32_t>(rttMs));
        }
    }

    receiverReportTimeMap_[senderSsrc] = currentTimeMs;
}

std::shared_ptr<lmcore::DataBuffer> RtcpSenderContext::CreateRtcpSr()
{
    if (rtcpSsrc_ == 0) {
        LMRTSP_LOGE("RTCP context not initialized");
        return nullptr;
    }

    auto sr = RtcpSenderReport::Create(0);
    if (!sr) {
        return nullptr;
    }

    uint64_t currentTimeMs = lmcore::TimeUtils::GetCurrentTimeMs();

    sr->SetSsrc(rtcpSsrc_)
        .SetNtpTimestamp(currentTimeMs)
        .SetRtpTimestamp(lastRtpTimestamp_)
        .SetCounts(static_cast<uint32_t>(totalPackets_), static_cast<uint32_t>(totalBytes_));

    // Store LSR for RTT calculation
    uint32_t lsr = RtcpUtils::GetLsrFromNtp(sr->ntpTimestampH, sr->ntpTimestampL);
    senderReportNtpMap_[lsr] = currentTimeMs;

    // Create data buffer
    size_t srSize = sr->GetSize();
    auto buffer = lmcore::DataBuffer::Create(srSize);
    buffer->Append(sr.get(), srSize);

    LMRTSP_LOGD("Created SR: SSRC=0x%08x, packets=%u, bytes=%u", rtcpSsrc_, static_cast<uint32_t>(totalPackets_),
                static_cast<uint32_t>(totalBytes_));

    return buffer;
}

uint32_t RtcpSenderContext::GetRtt(uint32_t ssrc) const
{
    auto it = rttMap_.find(ssrc);
    return (it != rttMap_.end()) ? it->second : 0;
}

uint32_t RtcpSenderContext::GetAverageRtt() const
{
    if (rttMap_.empty()) {
        return 0;
    }

    uint64_t totalRtt = 0;
    for (const auto &pair : rttMap_) {
        totalRtt += pair.second;
    }

    return static_cast<uint32_t>(totalRtt / rttMap_.size());
}

RtcpReceiverContext::Ptr RtcpReceiverContext::Create()
{
    return std::make_shared<RtcpReceiverContext>();
}

void RtcpReceiverContext::OnRtcp(const lmcore::DataBuffer &buffer)
{
    const uint8_t *data = buffer.Data();
    size_t size = buffer.Size();

    if (!data || size < sizeof(RtcpHeader)) {
        LMRTSP_LOGW("Invalid RTCP packet: data=%p, size=%zu", data, size);
        return;
    }

    const auto *header = reinterpret_cast<const RtcpHeader *>(data);

    // Check version
    if (header->version != RTCP_VERSION) {
        LMRTSP_LOGW("Invalid RTCP version: %d", header->version);
        return;
    }

    RtcpType type = static_cast<RtcpType>(header->packetType);

    switch (type) {
        case RtcpType::SR: {
            if (size >= sizeof(RtcpSenderReport)) {
                const auto *sr = reinterpret_cast<const RtcpSenderReport *>(data);
                ProcessSenderReport(sr);
            }
            break;
        }
        case RtcpType::RR:
            LMRTSP_LOGD("Received RR (unexpected for receiver context)");
            break;
        case RtcpType::BYE:
            LMRTSP_LOGI("Received BYE");
            break;
        default:
            LMRTSP_LOGD("Received RTCP packet type: %d", static_cast<int>(type));
            break;
    }
}

void RtcpReceiverContext::ProcessSenderReport(const RtcpSenderReport *sr)
{
    if (!sr) {
        return;
    }

    uint32_t ntpH = lmcore::ByteOrder::NetworkToHost32(sr->ntpTimestampH);
    uint32_t ntpL = lmcore::ByteOrder::NetworkToHost32(sr->ntpTimestampL);

    lastSrLsr_ = RtcpUtils::GetLsrFromNtp(ntpH, ntpL);
    lastSrNtpMs_ = lmcore::TimeUtils::GetCurrentTimeMs();

    LMRTSP_LOGD("Processed SR: SSRC=0x%08x, LSR=0x%08x", lmcore::ByteOrder::NetworkToHost32(sr->ssrc), lastSrLsr_);
}

void RtcpReceiverContext::OnRtp(uint16_t seq, uint32_t timestamp, uint64_t ntpTimestampMs, uint32_t sampleRate,
                                size_t bytes)
{
    RtcpContext::OnRtp(seq, timestamp, ntpTimestampMs, sampleRate, bytes);

    if (!seqInitialized_) {
        InitSequence(seq);
    } else {
        UpdateSequence(seq);
    }

    UpdateJitter(timestamp, ntpTimestampMs, sampleRate);
}

void RtcpReceiverContext::InitSequence(uint16_t seq)
{
    baseSeq_ = seq;
    maxSeq_ = seq;
    lastSeq_ = seq;
    cycles_ = 0;
    seqInitialized_ = true;
    LMRTSP_LOGD("Initialized sequence tracking: baseSeq=%u", seq);
}

void RtcpReceiverContext::UpdateSequence(uint16_t seq)
{
    uint16_t udelta = seq - maxSeq_;

    // Source is not valid until MIN_SEQUENTIAL packets with
    // sequential sequence numbers have been received
    constexpr uint16_t MAX_DROPOUT = 3000;
    constexpr uint16_t MAX_MISORDER = 100;

    if (udelta < MAX_DROPOUT) {
        // In order, with permissible gap
        if (seq < maxSeq_) {
            // Sequence number wrapped - count another 64K cycle
            cycles_++;
            LMRTSP_LOGD("Sequence wrapped: cycles=%u", cycles_);
        }
        maxSeq_ = seq;
    } else if (udelta <= 65536 - MAX_MISORDER) {
        // The sequence number made a very large jump
        LMRTSP_LOGW("Sequence jump detected: last=%u, current=%u", maxSeq_, seq);
    } else {
        // Duplicate or reordered packet
        LMRTSP_LOGD("Reordered/duplicate packet: seq=%u, maxSeq=%u", seq, maxSeq_);
    }

    lastSeq_ = seq;
}

void RtcpReceiverContext::UpdateJitter(uint32_t timestamp, uint64_t ntpTimestampMs, uint32_t sampleRate)
{
    if (lastArrivalTimeMs_ == 0) {
        lastArrivalTimeMs_ = ntpTimestampMs;
        return;
    }

    // Calculate jitter according to RFC 3550
    int64_t arrivalDiff = static_cast<int64_t>(ntpTimestampMs - lastArrivalTimeMs_);
    int64_t timestampDiff = static_cast<int64_t>(timestamp - lastRtpTimestamp_);

    // Convert to same units (timestamp units)
    double arrivalDiffTimestamp = (arrivalDiff * sampleRate) / 1000.0;
    double d = std::abs(arrivalDiffTimestamp - timestampDiff);

    // J(i) = J(i-1) + (|D(i-1,i)| - J(i-1))/16
    jitter_ += (d - jitter_) / 16.0;

    lastArrivalTimeMs_ = ntpTimestampMs;
}

std::shared_ptr<lmcore::DataBuffer> RtcpReceiverContext::CreateRtcpRr()
{
    if (rtcpSsrc_ == 0 || rtpSsrc_ == 0) {
        LMRTSP_LOGE("RTCP context not initialized");
        return nullptr;
    }

    auto rr = RtcpReceiverReport::Create(1);
    if (!rr) {
        return nullptr;
    }

    rr->ssrc = lmcore::ByteOrder::HostToNetwork32(rtcpSsrc_);

    auto blocks = rr->GetReportBlocks();
    if (!blocks.empty()) {
        auto *block = blocks[0];

        // Fill report block
        block->ssrc = lmcore::ByteOrder::HostToNetwork32(rtpSsrc_);

        // Calculate extended highest sequence number
        uint32_t extendedMax = (static_cast<uint32_t>(cycles_) << 16) | maxSeq_;
        block->extendedSeqNum = lmcore::ByteOrder::HostToNetwork32(extendedMax);

        // Calculate packet loss
        size_t expected = extendedMax - baseSeq_ + 1;
        size_t received = totalPackets_;
        size_t lost = (expected > received) ? (expected - received) : 0;

        block->cumulativeLost = lmcore::ByteOrder::HostToNetwork32(static_cast<uint32_t>(lost) & 0xFFFFFF);

        // Calculate fraction lost
        size_t expectedInterval = expected - lastExpected_;
        size_t receivedInterval = received - lastCyclePackets_;
        size_t lostInterval = (expectedInterval > receivedInterval) ? (expectedInterval - receivedInterval) : 0;

        uint8_t fractionLost = 0;
        if (expectedInterval > 0) {
            fractionLost = static_cast<uint8_t>((lostInterval * 256) / expectedInterval);
        }
        block->fractionLost = fractionLost;

        // Interarrival jitter
        block->jitter = lmcore::ByteOrder::HostToNetwork32(static_cast<uint32_t>(jitter_));

        // LSR and DLSR
        block->lastSr = lmcore::ByteOrder::HostToNetwork32(lastSrLsr_);
        if (lastSrNtpMs_ > 0) {
            uint64_t currentTimeMs = lmcore::TimeUtils::GetCurrentTimeMs();
            uint64_t dlsrMs = currentTimeMs - lastSrNtpMs_;
            // Convert to 1/65536 seconds
            uint32_t dlsr = static_cast<uint32_t>((dlsrMs * 65536) / 1000);
            block->delaySinceLastSr = lmcore::ByteOrder::HostToNetwork32(dlsr);
        } else {
            block->delaySinceLastSr = 0;
        }

        // Update for next interval
        lastExpected_ = expected;
        lastCyclePackets_ = received;
    }

    // Create data buffer
    size_t rrSize = rr->GetSize();
    auto buffer = lmcore::DataBuffer::Create(rrSize);
    buffer->Append(rr.get(), rrSize);

    LMRTSP_LOGD("Created RR: SSRC=0x%08x, lost=%zu, jitter=%u", rtcpSsrc_, GetLost(), static_cast<uint32_t>(jitter_));

    return buffer;
}

size_t RtcpReceiverContext::GetLost() const
{
    if (!seqInitialized_) {
        return 0;
    }

    uint32_t extendedMax = (static_cast<uint32_t>(cycles_) << 16) | maxSeq_;
    size_t expected = extendedMax - baseSeq_ + 1;
    size_t received = totalPackets_;

    return (expected > received) ? (expected - received) : 0;
}

size_t RtcpReceiverContext::GetLostInterval() const
{
    if (!seqInitialized_) {
        return 0;
    }

    uint32_t extendedMax = (static_cast<uint32_t>(cycles_) << 16) | maxSeq_;
    size_t expected = extendedMax - baseSeq_ + 1;
    size_t expectedInterval = expected - lastExpected_;
    size_t receivedInterval = totalPackets_ - lastCyclePackets_;

    return (expectedInterval > receivedInterval) ? (expectedInterval - receivedInterval) : 0;
}

size_t RtcpReceiverContext::GetExpectedPackets() const
{
    if (!seqInitialized_) {
        return 0;
    }

    uint32_t extendedMax = (static_cast<uint32_t>(cycles_) << 16) | maxSeq_;
    return extendedMax - baseSeq_ + 1;
}

size_t RtcpReceiverContext::GetExpectedPacketsInterval() const
{
    if (!seqInitialized_) {
        return 0;
    }

    uint32_t extendedMax = (static_cast<uint32_t>(cycles_) << 16) | maxSeq_;
    size_t expected = extendedMax - baseSeq_ + 1;
    return expected - lastExpected_;
}

double RtcpReceiverContext::GetLossRate() const
{
    size_t expected = GetExpectedPackets();
    if (expected == 0) {
        return 0.0;
    }

    size_t lost = GetLost();
    return static_cast<double>(lost) / static_cast<double>(expected);
}

uint32_t RtcpReceiverContext::GetJitter() const
{
    return static_cast<uint32_t>(jitter_);
}

} // namespace lmshao::lmrtsp
