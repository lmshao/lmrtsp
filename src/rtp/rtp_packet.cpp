/**
 * @author SHAO Liming <lmshao@163.com>
 * @copyright Copyright (c) 2025 SHAO Liming
 * @license MIT
 *
 * SPDX-License-Identifier: MIT
 */

#include "lmrtsp/rtp_packet.h"

#include "lmcore/byte_order.h"
#include "lmcore/data_buffer.h"

namespace lmshao::lmrtsp {

static bool ParseRtpFromBytes(RtpPacket &pkt, const uint8_t *data, size_t size)
{
    if (data == nullptr || size < 12)
        return false;

    uint8_t b0 = data[0];
    uint8_t b1 = data[1];
    pkt.version = (b0 >> 6) & 0x03;
    pkt.padding = (b0 >> 5) & 0x01;
    pkt.extension = (b0 >> 4) & 0x01;
    pkt.csrc_count = b0 & 0x0F;
    pkt.marker = (b1 >> 7) & 0x01;
    pkt.payload_type = b1 & 0x7F;

    pkt.sequence_number = lmcore::ByteOrder::ReadBE16(data + 2);
    pkt.timestamp = lmcore::ByteOrder::ReadBE32(data + 4);
    pkt.ssrc = lmcore::ByteOrder::ReadBE32(data + 8);

    if (pkt.version != 2 || pkt.csrc_count > 15)
        return false;

    size_t offset = 12;
    if (size < offset + pkt.csrc_count * 4)
        return false;

    pkt.csrc_list.clear();
    for (size_t i = 0; i < pkt.csrc_count; ++i) {
        uint32_t csrc = lmcore::ByteOrder::ReadBE32(data + offset);
        pkt.csrc_list.push_back(csrc);
        offset += 4;
    }

    pkt.extension_profile = 0;
    pkt.extension_data.clear();
    if (pkt.extension) {
        if (size < offset + 4)
            return false; // need profile + length
        pkt.extension_profile = lmcore::ByteOrder::ReadBE16(data + offset);
        uint16_t ext_len_words = lmcore::ByteOrder::ReadBE16(data + offset + 2);
        offset += 4;
        size_t ext_len_bytes = static_cast<size_t>(ext_len_words) * 4;
        if (size < offset + ext_len_bytes)
            return false;
        pkt.extension_data.assign(data + offset, data + offset + ext_len_bytes);
        offset += ext_len_bytes;
    }

    size_t payload_len = size > offset ? (size - offset) : 0;
    if (payload_len) {
        pkt.payload = lmcore::DataBuffer::PoolAlloc(payload_len);
        pkt.payload->Assign(data + offset, payload_len);
    } else {
        pkt.payload.reset();
    }
    return true;
}

size_t RtpPacket::HeaderSize() const
{
    size_t len = 12;                            // Base RTP header size
    len += static_cast<size_t>(csrc_count) * 4; // CSRC list
    if (extension) {
        len += 4;                     // Extension header (profile + length)
        len += extension_data.size(); // Extension data
    }
    return len;
}

size_t RtpPacket::Size() const
{
    return HeaderSize() + (payload ? payload->Size() : 0);
}

bool RtpPacket::Validate() const
{
    if (version != 2)
        return false;
    if (csrc_count != csrc_list.size())
        return false;
    if (csrc_count > 15)
        return false;
    if (extension) {
        if (extension_data.size() % 4 != 0)
            return false; // length must be multiple of 4 bytes
    }
    return true;
}

std::shared_ptr<lmcore::DataBuffer> RtpPacket::Serialize() const
{
    if (!Validate())
        return nullptr;
    auto buf = lmcore::DataBuffer::PoolAlloc(Size());

    // First byte: V(2), P(1), X(1), CC(4)
    uint8_t b0 = static_cast<uint8_t>(((version & 0x03) << 6) | ((padding & 0x01) << 5) | ((extension & 0x01) << 4) |
                                      (csrc_count & 0x0F));
    // Second byte: M(1), PT(7)
    uint8_t b1 = static_cast<uint8_t>(((marker & 0x01) << 7) | (payload_type & 0x7F));
    buf->Append(&b0, 1);
    buf->Append(&b1, 1);

    // Sequence number, timestamp, SSRC (big-endian using ByteOrder)
    uint8_t seqbuf[2];
    lmcore::ByteOrder::WriteBE16(seqbuf, sequence_number);
    buf->Append(seqbuf, 2);
    uint8_t tsbuf[4];
    lmcore::ByteOrder::WriteBE32(tsbuf, timestamp);
    buf->Append(tsbuf, 4);
    uint8_t ssrcbuf[4];
    lmcore::ByteOrder::WriteBE32(ssrcbuf, ssrc);
    buf->Append(ssrcbuf, 4);

    // CSRC list
    for (size_t i = 0; i < csrc_list.size(); ++i) {
        uint8_t csrcbuf[4];
        lmcore::ByteOrder::WriteBE32(csrcbuf, static_cast<uint32_t>(csrc_list[i]));
        buf->Append(csrcbuf, 4);
    }

    // Extension header (profile + length in 32-bit words) and data
    if (extension) {
        uint8_t extProfileBuf[2];
        lmcore::ByteOrder::WriteBE16(extProfileBuf, static_cast<uint16_t>(extension_profile));
        buf->Append(extProfileBuf, 2);
        uint16_t ext_len_words = static_cast<uint16_t>(extension_data.size() / 4);
        uint8_t extLenBuf[2];
        lmcore::ByteOrder::WriteBE16(extLenBuf, ext_len_words);
        buf->Append(extLenBuf, 2);
        if (!extension_data.empty()) {
            buf->Append(extension_data.data(), extension_data.size());
        }
    }

    // Payload
    if (payload && payload->Size()) {
        buf->Append(payload);
    }
    return buf;
}

bool RtpPacket::Parse(char *data, size_t length)
{
    return ParseRtpFromBytes(*this, reinterpret_cast<const uint8_t *>(data), length);
}

bool RtpPacket::Parse(const lmcore::DataBuffer &buf)
{
    return ParseRtpFromBytes(*this, buf.Data(), buf.Size());
}

std::shared_ptr<RtpPacket> RtpPacket::Deserialize(char *data, size_t length)
{
    auto pkt = std::make_shared<RtpPacket>();
    if (!pkt->Parse(data, length)) {
        return nullptr;
    }
    return pkt;
}

std::shared_ptr<RtpPacket> RtpPacket::Deserialize(const lmcore::DataBuffer &buf)
{
    auto pkt = std::make_shared<RtpPacket>();
    if (!pkt->Parse(buf)) {
        return nullptr;
    }
    return pkt;
}

} // namespace lmshao::lmrtsp