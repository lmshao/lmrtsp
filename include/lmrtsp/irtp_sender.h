#ifndef LMSHAO_LMRTSP_IRTP_SENDER_H
#define LMSHAO_LMRTSP_IRTP_SENDER_H

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace lmshao::lmrtsp {

struct RTPStatistics {
    uint64_t packets_sent = 0;
    uint64_t bytes_sent = 0;
    uint64_t packets_lost = 0;
    uint32_t jitter = 0;
    uint32_t rtt = 0;
    double loss_rate = 0.0;
    uint32_t bandwidth = 0;
    uint64_t last_sr_timestamp = 0;
    uint64_t last_rr_timestamp = 0;
};

struct RTPTransportParams {
    std::string client_ip;
    uint16_t client_rtp_port = 0;
    uint16_t client_rtcp_port = 0;
    uint16_t server_rtp_port = 0;
    uint16_t server_rtcp_port = 0;
    std::string transport_mode = "RTP/AVP/UDP";
    bool unicast = true;
    std::string multicast_ip;
    uint8_t ttl = 64;
    uint32_t ssrc = 0;
};

class IRTPSender {
public:
    virtual ~IRTPSender() = default;
    virtual bool Initialize(const RTPTransportParams &transport_params) = 0;
    virtual bool Start() = 0;
    virtual void Stop() = 0;
    virtual bool IsRunning() const = 0;
    virtual bool SendRTPPacket(const uint8_t *data, size_t size, uint32_t timestamp, bool marker = false) = 0;
    virtual bool SendRawRTPPacket(const uint8_t *rtp_packet, size_t size) = 0;
    virtual bool SendRTCPPacket(const uint8_t *rtcp_packet, size_t size) = 0;
    virtual RTPStatistics GetStatistics() const = 0;
    virtual RTPTransportParams GetTransportParams() const = 0;
    virtual void SetMTU(uint32_t mtu) = 0;
    virtual uint32_t GetMTU() const = 0;
    virtual void SetPayloadType(uint8_t payload_type) = 0;
    virtual uint8_t GetPayloadType() const = 0;
    virtual void SetSSRC(uint32_t ssrc) = 0;
    virtual uint32_t GetSSRC() const = 0;
    virtual uint16_t GetNextSequenceNumber() = 0;
    virtual uint32_t GetCurrentTimestamp(uint32_t clock_rate) const = 0;
    virtual void HandleRTCPPacket(const uint8_t *rtcp_packet, size_t size) = 0;
    virtual void SendRTCPSenderReport() = 0;
    virtual void SetBitrateLimit(uint32_t bitrate) = 0;
    virtual uint32_t GetBitrateLimit() const = 0;
    virtual std::string GetClientAddress() const = 0;
};

class IRTPSenderFactory {
public:
    virtual ~IRTPSenderFactory() = default;
    virtual std::shared_ptr<IRTPSender> CreateRTPSender(const std::string &stream_path,
                                                        const RTPTransportParams &transport_params) = 0;
    virtual void DestroyRTPSender(std::shared_ptr<IRTPSender> sender) = 0;
};

} // namespace lmshao::lmrtsp

#endif // LMSHAO_LMRTSP_IRTP_SENDER_H