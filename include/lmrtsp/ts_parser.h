/**
 * @author SHAO Liming <lmshao@163.com>
 * @copyright Copyright (c) 2025 SHAO Liming
 * @license MIT
 *
 * SPDX-License-Identifier: MIT
 */

#ifndef LMSHAO_LMRTSP_TS_PARSER_H
#define LMSHAO_LMRTSP_TS_PARSER_H

#include <cstdint>

namespace lmshao::lmrtsp {

/**
 * @brief TS packet information extracted from MPEG-TS packet
 */
struct TSPacketInfo {
    bool has_pcr = false;              ///< Whether PCR is present in this packet
    uint64_t pcr = 0;                  ///< PCR value in 27MHz ticks (0 if not present)
    uint16_t pid = 0;                  ///< Packet ID
    bool has_adaptation_field = false; ///< Whether adaptation field is present
    bool discontinuity = false;        ///< Discontinuity indicator (PCR may be discontinuous)
    bool random_access = false;        ///< Random access indicator (key frame)
};

/**
 * @brief MPEG-TS packet parser utility class
 *
 * This class provides methods to parse MPEG-TS packets and extract
 * PCR (Program Clock Reference) and other timing information.
 */
class TSParser {
public:
    /**
     * @brief Parse TS packet header and extract PCR if present
     *
     * @param packet_data TS packet data (must be at least 188 bytes)
     * @param info Output packet information structure
     * @return true if packet is valid and parsed successfully
     */
    static bool ParsePacket(const uint8_t *packet_data, TSPacketInfo &info);

    /**
     * @brief Convert PCR (27MHz) to RTP timestamp (90kHz)
     *
     * PCR is in 27MHz clock units, RTP timestamp is in 90kHz clock units.
     * Conversion formula: rtp_ts = (pcr * 90) / 27000 = pcr / 300
     *
     * @param pcr PCR value in 27MHz ticks
     * @return RTP timestamp in 90kHz clock units
     */
    static uint32_t PCRToRTPTimestamp(uint64_t pcr);

    /**
     * @brief Calculate RTP timestamp increment from PCR interval
     *
     * @param pcr1 First PCR value (27MHz ticks)
     * @param pcr2 Second PCR value (27MHz ticks)
     * @param packet_count Number of packets between pcr1 and pcr2
     * @return RTP timestamp increment per packet (90kHz clock), or 0 if invalid
     */
    static uint32_t CalculateRTPIncrementFromPCR(uint64_t pcr1, uint64_t pcr2, uint32_t packet_count);

    /**
     * @brief Check if PCR discontinuity occurred
     *
     * @param prev_pcr Previous PCR value (27MHz ticks)
     * @param curr_pcr Current PCR value (27MHz ticks)
     * @param max_interval Maximum expected PCR interval in 27MHz ticks (default: 0.1s = 2700000)
     * @return true if discontinuity detected
     */
    static bool IsPCRDiscontinuous(uint64_t prev_pcr, uint64_t curr_pcr, uint64_t max_interval = 2700000);

private:
    /**
     * @brief Extract PCR from adaptation field
     *
     * @param adaptation_field_data Pointer to adaptation field data (after length byte)
     * @param adaptation_field_length Length of adaptation field (including length byte)
     * @param pcr Output PCR value (27MHz ticks)
     * @return true if PCR was extracted successfully
     */
    static bool ExtractPCR(const uint8_t *adaptation_field_data, uint8_t adaptation_field_length, uint64_t &pcr);
};

} // namespace lmshao::lmrtsp

#endif // LMSHAO_LMRTSP_TS_PARSER_H
