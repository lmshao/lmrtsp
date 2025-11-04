/**
 * @author SHAO Liming <lmshao@163.com>
 * @copyright Copyright (c) 2025 SHAO Liming
 * @license MIT
 *
 * SPDX-License-Identifier: MIT
 */

#include "lmrtsp/rtcp_packet.h"

#include <arpa/inet.h>
#include <sys/time.h>

#include <cstring>

#include "internal_logger.h"

namespace lmshao::lmrtsp {

// ============================================================================
// RtcpHeader
// ============================================================================

void RtcpHeader::SetSize(size_t sizeBytes)
{
    // Length is in 32-bit words minus one
    length = htons((sizeBytes / 4) - 1);
}

size_t RtcpHeader::GetSize() const
{
    // Convert from 32-bit words to bytes
    return (ntohs(length) + 1) * 4;
}

size_t RtcpHeader::GetPaddingSize() const
{
    if (!padding) {
        return 0;
    }
    // Padding size is stored in the last byte of the packet
    const uint8_t *ptr = reinterpret_cast<const uint8_t *>(this);
    return ptr[GetSize() - 1];
}

// ============================================================================
// RtcpSenderReport
// ============================================================================

std::shared_ptr<RtcpSenderReport> RtcpSenderReport::Create(size_t reportCount)
{
    size_t totalSize = sizeof(RtcpSenderReport) + reportCount * sizeof(RtcpReportBlock);
    auto sr = std::shared_ptr<RtcpSenderReport>(reinterpret_cast<RtcpSenderReport *>(new uint8_t[totalSize]),
                                                [](RtcpSenderReport *p) { delete[] reinterpret_cast<uint8_t *>(p); });

    memset(sr.get(), 0, totalSize);
    sr->version = RTCP_VERSION;
    sr->padding = 0;
    sr->count = reportCount;
    sr->packetType = static_cast<uint8_t>(RtcpType::SR);
    sr->SetSize(totalSize);

    return sr;
}

RtcpSenderReport &RtcpSenderReport::SetSsrc(uint32_t ssrcValue)
{
    ssrc = htonl(ssrcValue);
    return *this;
}

RtcpSenderReport &RtcpSenderReport::SetNtpTimestamp(uint64_t unixTimeMs)
{
    RtcpUtils::UnixMsToNtp(unixTimeMs, ntpTimestampH, ntpTimestampL);
    ntpTimestampH = htonl(ntpTimestampH);
    ntpTimestampL = htonl(ntpTimestampL);
    return *this;
}

RtcpSenderReport &RtcpSenderReport::SetRtpTimestamp(uint32_t timestamp)
{
    rtpTimestamp = htonl(timestamp);
    return *this;
}

RtcpSenderReport &RtcpSenderReport::SetCounts(uint32_t packets, uint32_t octets)
{
    packetCount = htonl(packets);
    octetCount = htonl(octets);
    return *this;
}

std::vector<RtcpReportBlock *> RtcpSenderReport::GetReportBlocks()
{
    std::vector<RtcpReportBlock *> blocks;
    if (count == 0) {
        return blocks;
    }

    RtcpReportBlock *block =
        reinterpret_cast<RtcpReportBlock *>(reinterpret_cast<uint8_t *>(this) + sizeof(RtcpSenderReport));

    for (size_t i = 0; i < count; ++i) {
        blocks.push_back(block + i);
    }

    return blocks;
}

uint64_t RtcpSenderReport::GetNtpUnixMs() const
{
    return RtcpUtils::NtpToUnixMs(ntohl(ntpTimestampH), ntohl(ntpTimestampL));
}

// ============================================================================
// RtcpReceiverReport
// ============================================================================

std::shared_ptr<RtcpReceiverReport> RtcpReceiverReport::Create(size_t reportCount)
{
    size_t totalSize = sizeof(RtcpReceiverReport) + reportCount * sizeof(RtcpReportBlock);
    auto rr =
        std::shared_ptr<RtcpReceiverReport>(reinterpret_cast<RtcpReceiverReport *>(new uint8_t[totalSize]),
                                            [](RtcpReceiverReport *p) { delete[] reinterpret_cast<uint8_t *>(p); });

    memset(rr.get(), 0, totalSize);
    rr->version = RTCP_VERSION;
    rr->padding = 0;
    rr->count = reportCount;
    rr->packetType = static_cast<uint8_t>(RtcpType::RR);
    rr->SetSize(totalSize);

    return rr;
}

std::vector<RtcpReportBlock *> RtcpReceiverReport::GetReportBlocks()
{
    std::vector<RtcpReportBlock *> blocks;
    if (count == 0) {
        return blocks;
    }

    RtcpReportBlock *block =
        reinterpret_cast<RtcpReportBlock *>(reinterpret_cast<uint8_t *>(this) + sizeof(RtcpReceiverReport));

    for (size_t i = 0; i < count; ++i) {
        blocks.push_back(block + i);
    }

    return blocks;
}

// ============================================================================
// RtcpSdes
// ============================================================================

std::shared_ptr<RtcpSdes> RtcpSdes::Create(const std::vector<SdesChunk> &chunks)
{
    if (chunks.empty()) {
        return nullptr;
    }

    // Calculate total size
    size_t totalSize = sizeof(RtcpHeader);
    for (const auto &chunk : chunks) {
        totalSize += 4; // SSRC
        for (const auto &item : chunk.items) {
            totalSize += 1;                // Type
            totalSize += 1;                // Length
            totalSize += item.text.size(); // Text
        }
        totalSize += 1; // END marker
        // Pad to 32-bit boundary
        totalSize = (totalSize + 3) & ~3;
    }

    auto sdes = std::shared_ptr<RtcpSdes>(reinterpret_cast<RtcpSdes *>(new uint8_t[totalSize]),
                                          [](RtcpSdes *p) { delete[] reinterpret_cast<uint8_t *>(p); });

    memset(sdes.get(), 0, totalSize);
    sdes->version = RTCP_VERSION;
    sdes->padding = 0;
    sdes->count = chunks.size();
    sdes->packetType = static_cast<uint8_t>(RtcpType::SDES);
    sdes->SetSize(totalSize);

    // Fill in chunks
    uint8_t *ptr = reinterpret_cast<uint8_t *>(sdes.get()) + sizeof(RtcpHeader);
    for (const auto &chunk : chunks) {
        // Write SSRC
        *reinterpret_cast<uint32_t *>(ptr) = htonl(chunk.ssrc);
        ptr += 4;

        // Write SDES items
        for (const auto &item : chunk.items) {
            *ptr++ = static_cast<uint8_t>(item.type);
            *ptr++ = item.text.size();
            memcpy(ptr, item.text.data(), item.text.size());
            ptr += item.text.size();
        }

        // Write END marker
        *ptr++ = static_cast<uint8_t>(SdesType::END);

        // Pad to 32-bit boundary
        while (reinterpret_cast<uintptr_t>(ptr) % 4 != 0) {
            *ptr++ = 0;
        }
    }

    return sdes;
}

// Deprecated version for backward compatibility
std::shared_ptr<RtcpSdes> RtcpSdes::Create(const std::vector<std::pair<uint32_t, std::string>> &items)
{
    if (items.empty()) {
        return nullptr;
    }

    // Convert to new format
    std::vector<SdesChunk> chunks;
    for (const auto &item : items) {
        SdesChunk chunk(item.first);
        chunk.AddItem(SdesType::CNAME, item.second);
        chunks.push_back(chunk);
    }

    return Create(chunks);
}

// ============================================================================
// RtcpBye
// ============================================================================

std::shared_ptr<RtcpBye> RtcpBye::Create(const std::vector<uint32_t> &ssrcs, const std::string &reason)
{
    if (ssrcs.empty()) {
        return nullptr;
    }

    size_t totalSize = sizeof(RtcpHeader) + ssrcs.size() * 4;
    if (!reason.empty()) {
        totalSize += 1 + reason.size(); // Length byte + reason text
        // Pad to 32-bit boundary
        totalSize = (totalSize + 3) & ~3;
    }

    auto bye = std::shared_ptr<RtcpBye>(reinterpret_cast<RtcpBye *>(new uint8_t[totalSize]),
                                        [](RtcpBye *p) { delete[] reinterpret_cast<uint8_t *>(p); });

    memset(bye.get(), 0, totalSize);
    bye->version = RTCP_VERSION;
    bye->padding = 0;
    bye->count = ssrcs.size();
    bye->packetType = static_cast<uint8_t>(RtcpType::BYE);
    bye->SetSize(totalSize);

    // Write SSRCs
    uint8_t *ptr = reinterpret_cast<uint8_t *>(bye.get()) + sizeof(RtcpHeader);
    for (uint32_t ssrc : ssrcs) {
        *reinterpret_cast<uint32_t *>(ptr) = htonl(ssrc);
        ptr += 4;
    }

    // Write reason if provided
    if (!reason.empty()) {
        *ptr++ = reason.size();
        memcpy(ptr, reason.data(), reason.size());
    }

    return bye;
}

std::vector<uint32_t> RtcpBye::GetSsrcs() const
{
    std::vector<uint32_t> ssrcs;
    const uint32_t *ptr =
        reinterpret_cast<const uint32_t *>(reinterpret_cast<const uint8_t *>(this) + sizeof(RtcpHeader));

    for (size_t i = 0; i < count; ++i) {
        ssrcs.push_back(ntohl(ptr[i]));
    }

    return ssrcs;
}

std::string RtcpBye::GetReason() const
{
    const uint8_t *ptr = reinterpret_cast<const uint8_t *>(this) + sizeof(RtcpHeader) + count * 4;
    size_t packetEnd = GetSize();
    size_t reasonStart = sizeof(RtcpHeader) + count * 4;

    if (reasonStart >= packetEnd) {
        return "";
    }

    uint8_t reasonLen = *ptr++;
    if (reasonStart + 1 + reasonLen > packetEnd) {
        return "";
    }

    return std::string(reinterpret_cast<const char *>(ptr), reasonLen);
}

// ============================================================================
// RtcpFeedback
// ============================================================================

const uint8_t *RtcpFeedback::GetFci() const
{
    return reinterpret_cast<const uint8_t *>(this) + sizeof(RtcpFeedback);
}

size_t RtcpFeedback::GetFciSize() const
{
    return GetSize() - sizeof(RtcpFeedback);
}

std::shared_ptr<RtcpFeedback> RtcpFeedback::CreatePsfb(PsfbType fmt, const void *fci, size_t fciLen)
{
    size_t totalSize = sizeof(RtcpFeedback) + fciLen;
    auto fb = std::shared_ptr<RtcpFeedback>(reinterpret_cast<RtcpFeedback *>(new uint8_t[totalSize]),
                                            [](RtcpFeedback *p) { delete[] reinterpret_cast<uint8_t *>(p); });

    memset(fb.get(), 0, totalSize);
    fb->version = RTCP_VERSION;
    fb->padding = 0;
    fb->count = static_cast<uint8_t>(fmt); // FMT stored in count field
    fb->packetType = static_cast<uint8_t>(RtcpType::PSFB);
    fb->SetSize(totalSize);

    if (fci && fciLen > 0) {
        memcpy(const_cast<uint8_t *>(fb->GetFci()), fci, fciLen);
    }

    return fb;
}

std::shared_ptr<RtcpFeedback> RtcpFeedback::CreateRtpfb(RtpfbType fmt, const void *fci, size_t fciLen)
{
    size_t totalSize = sizeof(RtcpFeedback) + fciLen;
    auto fb = std::shared_ptr<RtcpFeedback>(reinterpret_cast<RtcpFeedback *>(new uint8_t[totalSize]),
                                            [](RtcpFeedback *p) { delete[] reinterpret_cast<uint8_t *>(p); });

    memset(fb.get(), 0, totalSize);
    fb->version = RTCP_VERSION;
    fb->padding = 0;
    fb->count = static_cast<uint8_t>(fmt); // FMT stored in count field
    fb->packetType = static_cast<uint8_t>(RtcpType::RTPFB);
    fb->SetSize(totalSize);

    if (fci && fciLen > 0) {
        memcpy(const_cast<uint8_t *>(fb->GetFci()), fci, fciLen);
    }

    return fb;
}

// ============================================================================
// NackItem
// ============================================================================

NackItem::NackItem(uint16_t packetId, uint16_t bitmask) : pid(htons(packetId)), blp(bitmask) {}

// ============================================================================
// RtcpUtils
// ============================================================================

namespace RtcpUtils {

void UnixMsToNtp(uint64_t unixMs, uint32_t &ntpH, uint32_t &ntpL)
{
    // Convert milliseconds to microseconds and add NTP epoch offset
    uint64_t ntpUs = unixMs * 1000 + NTP_OFFSET_US;

    // High 32 bits: seconds since 1900
    ntpH = ntpUs / 1000000;

    // Low 32 bits: fractional seconds (32-bit fixed-point)
    uint64_t fraction = ntpUs % 1000000;
    ntpL = (fraction << 32) / 1000000;
}

uint64_t NtpToUnixMs(uint32_t ntpH, uint32_t ntpL)
{
    // Convert NTP timestamp to microseconds
    uint64_t ntpUs = static_cast<uint64_t>(ntpH) * 1000000;
    ntpUs += (static_cast<uint64_t>(ntpL) * 1000000) >> 32;

    // Subtract NTP epoch offset and convert to milliseconds
    if (ntpUs < NTP_OFFSET_US) {
        return 0;
    }
    return (ntpUs - NTP_OFFSET_US) / 1000;
}

void GetCurrentNtpTimestamp(uint32_t &ntpH, uint32_t &ntpL)
{
    struct timeval tv;
    gettimeofday(&tv, nullptr);
    uint64_t unixMs = static_cast<uint64_t>(tv.tv_sec) * 1000 + tv.tv_usec / 1000;
    UnixMsToNtp(unixMs, ntpH, ntpL);
}

uint32_t GetLsrFromNtp(uint32_t ntpH, uint32_t ntpL)
{
    // LSR is the middle 32 bits of the NTP timestamp
    return ((ntpH & 0xFFFF) << 16) | ((ntpL >> 16) & 0xFFFF);
}

} // namespace RtcpUtils

} // namespace lmshao::lmrtsp
