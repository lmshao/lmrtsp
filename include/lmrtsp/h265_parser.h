/**
 * @author SHAO Liming <lmshao@163.com>
 * @copyright Copyright (c) 2025 SHAO Liming
 * @license MIT
 *
 * SPDX-License-Identifier: MIT
 */

#ifndef LMSHAO_LMRTSP_H265_PARSER_H
#define LMSHAO_LMRTSP_H265_PARSER_H

#include <lmcore/data_buffer.h>

#include <cstdint>
#include <memory>
#include <string>

namespace lmshao::lmrtsp {

/**
 * @brief Video information extracted from H.265 SPS
 */
struct H265VideoInfo {
    int32_t width = 0;             // Video width in pixels
    int32_t height = 0;            // Video height in pixels
    int32_t profile_idc = 0;       // Profile indication
    int32_t level_idc = 0;         // Level indication
    int32_t chroma_format_idc = 0; // Chroma format (0=monochrome, 1=4:2:0, 2=4:2:2, 3=4:4:4)
    int32_t bit_depth_luma = 8;    // Bit depth for luma samples
    int32_t bit_depth_chroma = 8;  // Bit depth for chroma samples
    bool valid = false;            // True if parsing was successful
};

/**
 * @brief H.265/HEVC bitstream parser utility class
 *
 * This class provides methods to parse H.265 VPS/SPS/PPS (Video/Sequence/Picture Parameter Set)
 * to extract video resolution and other parameters.
 */
class H265Parser {
public:
    /**
     * @brief Parse H.265 SPS to extract video information
     *
     * @param sps SPS data buffer (without start code)
     * @return Video information structure
     */
    static H265VideoInfo ParseSPS(const std::shared_ptr<lmcore::DataBuffer> &sps);

    /**
     * @brief Get video resolution from SPS
     *
     * @param sps SPS data buffer (without start code)
     * @param width Output: video width
     * @param height Output: video height
     * @return true if successful, false otherwise
     */
    static bool GetResolution(const std::shared_ptr<lmcore::DataBuffer> &sps, int32_t &width, int32_t &height);

    /**
     * @brief Remove start code from NAL unit if present
     *
     * @param data NAL unit data buffer
     * @return Data buffer without start code
     */
    static std::shared_ptr<lmcore::DataBuffer> RemoveStartCode(const std::shared_ptr<lmcore::DataBuffer> &data);

    /**
     * @brief Check if data starts with H.265 start code (0x00000001 or 0x000001)
     *
     * @param data Data buffer to check
     * @return true if starts with start code
     */
    static bool HasStartCode(const std::shared_ptr<lmcore::DataBuffer> &data);

    /**
     * @brief Get NAL unit type from DataBuffer
     *
     * @param data NAL unit data buffer (with or without start code)
     * @return NAL unit type (0-63), or -1 if invalid
     */
    static int32_t GetNaluType(const std::shared_ptr<lmcore::DataBuffer> &data);

    /**
     * @brief Check if NAL unit is a key frame (IDR or CRA)
     *
     * @param data NAL unit data buffer (with or without start code)
     * @return true if it's an IDR or CRA frame
     */
    static bool IsKeyFrame(const std::shared_ptr<lmcore::DataBuffer> &data);

    /**
     * @brief Extract VPS, SPS and PPS from H.265 data
     *
     * @param data H.265 data buffer
     * @param vps Output VPS data buffer (without start code)
     * @param sps Output SPS data buffer (without start code)
     * @param pps Output PPS data buffer (without start code)
     * @return true if VPS, SPS and PPS found
     */
    static bool ExtractVPSSPSPPS(const std::shared_ptr<lmcore::DataBuffer> &data,
                                 std::shared_ptr<lmcore::DataBuffer> &vps, std::shared_ptr<lmcore::DataBuffer> &sps,
                                 std::shared_ptr<lmcore::DataBuffer> &pps);

    /**
     * @brief Get profile name string from profile IDC
     *
     * @param profile_idc Profile IDC value
     * @return Profile name (e.g., "Main", "Main 10")
     */
    static std::string GetProfileName(int32_t profile_idc);

    /**
     * @brief Get level string from level IDC
     *
     * @param level_idc Level IDC value
     * @return Level string (e.g., "4.0", "5.1")
     */
    static std::string GetLevelString(int32_t level_idc);

private:
    /**
     * @brief Read unsigned Exp-Golomb coded value
     */
    static uint32_t ReadUE(const uint8_t *buf, uint32_t len, uint32_t &pos);

    /**
     * @brief Read signed Exp-Golomb coded value
     */
    static int32_t ReadSE(const uint8_t *buf, uint32_t len, uint32_t &pos);

    /**
     * @brief Read fixed-length bits from bitstream
     */
    static uint32_t ReadBits(uint32_t bit_count, const uint8_t *buf, uint32_t &pos);

    /**
     * @brief Remove emulation prevention bytes (0x03) from RBSP
     */
    static void RemoveEmulationPrevention(uint8_t *buf, uint32_t &size);
};

} // namespace lmshao::lmrtsp

#endif // LMSHAO_LMRTSP_H265_PARSER_H
