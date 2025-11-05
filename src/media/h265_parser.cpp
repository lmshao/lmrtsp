/**
 * @author SHAO Liming <lmshao@163.com>
 * @copyright Copyright (c) 2025 SHAO Liming
 * @license MIT
 *
 * SPDX-License-Identifier: MIT
 */

#include "lmrtsp/h265_parser.h"

#include <cmath>
#include <cstring>
#include <vector>

namespace lmshao::lmrtsp {

namespace {
// Internal helper functions

uint32_t ReadUE(const uint8_t *buf, uint32_t len, uint32_t &pos)
{
    uint32_t zero_count = 0;
    while (pos < len * 8) {
        if (buf[pos / 8] & (0x80 >> (pos % 8))) {
            break;
        }
        zero_count++;
        pos++;
    }
    pos++;

    uint64_t value = 0;
    for (uint32_t i = 0; i < zero_count; i++) {
        value <<= 1;
        if (buf[pos / 8] & (0x80 >> (pos % 8))) {
            value += 1;
        }
        pos++;
    }

    return static_cast<uint32_t>((1ULL << zero_count) - 1 + value);
}

int32_t ReadSE(const uint8_t *buf, uint32_t len, uint32_t &pos)
{
    uint32_t ue_val = ReadUE(buf, len, pos);
    double k = ue_val;
    int32_t value = static_cast<int32_t>(std::ceil(k / 2.0));
    if (ue_val % 2 == 0) {
        value = -value;
    }
    return value;
}

uint32_t ReadBits(uint32_t bit_count, const uint8_t *buf, uint32_t &pos)
{
    uint32_t value = 0;
    for (uint32_t i = 0; i < bit_count; i++) {
        value <<= 1;
        if (buf[pos / 8] & (0x80 >> (pos % 8))) {
            value += 1;
        }
        pos++;
    }
    return value;
}

void RemoveEmulationPrevention(uint8_t *buf, uint32_t &size)
{
    uint32_t tmp_size = size;
    for (uint32_t i = 0; i < (tmp_size - 2); i++) {
        if (buf[i] == 0x00 && buf[i + 1] == 0x00 && buf[i + 2] == 0x03) {
            for (uint32_t j = i + 2; j < tmp_size - 1; j++) {
                buf[j] = buf[j + 1];
            }
            size--;
            tmp_size = size;
        }
    }
}

H265VideoInfo ParseSPSInternal(const uint8_t *sps, size_t size)
{
    H265VideoInfo info;

    if (size < 15) {
        return info;
    }

    std::vector<uint8_t> rbsp(sps, sps + size);
    uint32_t rbsp_size = size;
    RemoveEmulationPrevention(rbsp.data(), rbsp_size);

    uint32_t pos = 0;
    const uint8_t *buf = rbsp.data();

    // Skip NAL header (2 bytes)
    pos += 16;

    // sps_video_parameter_set_id (4 bits)
    ReadBits(4, buf, pos);

    // sps_max_sub_layers_minus1 (3 bits)
    uint32_t sps_max_sub_layers_minus1 = ReadBits(3, buf, pos);

    // sps_temporal_id_nesting_flag (1 bit)
    ReadBits(1, buf, pos);

    // Parse profile_tier_level
    // general_profile_space (2 bits)
    ReadBits(2, buf, pos);
    // general_tier_flag (1 bit)
    ReadBits(1, buf, pos);
    // general_profile_idc (5 bits)
    info.profile_idc = ReadBits(5, buf, pos);

    // general_profile_compatibility_flag[32]
    for (int i = 0; i < 32; i++) {
        ReadBits(1, buf, pos);
    }

    // general_progressive_source_flag, general_interlaced_source_flag,
    // general_non_packed_constraint_flag, general_frame_only_constraint_flag
    ReadBits(4, buf, pos);

    // Skip 43 or 44 bits of constraint flags
    ReadBits(43, buf, pos);
    ReadBits(1, buf, pos);

    // general_level_idc (8 bits)
    info.level_idc = ReadBits(8, buf, pos);

    // Skip sub_layer info
    std::vector<uint8_t> sub_layer_profile_present_flag(sps_max_sub_layers_minus1);
    std::vector<uint8_t> sub_layer_level_present_flag(sps_max_sub_layers_minus1);

    for (uint32_t i = 0; i < sps_max_sub_layers_minus1; i++) {
        sub_layer_profile_present_flag[i] = ReadBits(1, buf, pos);
        sub_layer_level_present_flag[i] = ReadBits(1, buf, pos);
    }

    if (sps_max_sub_layers_minus1 > 0) {
        for (uint32_t i = sps_max_sub_layers_minus1; i < 8; i++) {
            ReadBits(2, buf, pos); // reserved_zero_2bits
        }
    }

    for (uint32_t i = 0; i < sps_max_sub_layers_minus1; i++) {
        if (sub_layer_profile_present_flag[i]) {
            ReadBits(2, buf, pos); // sub_layer_profile_space
            ReadBits(1, buf, pos); // sub_layer_tier_flag
            ReadBits(5, buf, pos); // sub_layer_profile_idc
            for (int j = 0; j < 32; j++) {
                ReadBits(1, buf, pos);
            }
            ReadBits(4, buf, pos);
            ReadBits(43, buf, pos);
            ReadBits(1, buf, pos);
        }
        if (sub_layer_level_present_flag[i]) {
            ReadBits(8, buf, pos); // sub_layer_level_idc
        }
    }

    // sps_seq_parameter_set_id
    ReadUE(buf, rbsp_size, pos);

    // chroma_format_idc
    info.chroma_format_idc = ReadUE(buf, rbsp_size, pos);

    if (info.chroma_format_idc == 3) {
        ReadBits(1, buf, pos); // separate_colour_plane_flag
    }

    // pic_width_in_luma_samples
    uint32_t pic_width_in_luma_samples = ReadUE(buf, rbsp_size, pos);

    // pic_height_in_luma_samples
    uint32_t pic_height_in_luma_samples = ReadUE(buf, rbsp_size, pos);

    // conformance_window_flag
    uint32_t conformance_window_flag = ReadBits(1, buf, pos);

    uint32_t conf_win_left_offset = 0;
    uint32_t conf_win_right_offset = 0;
    uint32_t conf_win_top_offset = 0;
    uint32_t conf_win_bottom_offset = 0;

    if (conformance_window_flag) {
        conf_win_left_offset = ReadUE(buf, rbsp_size, pos);
        conf_win_right_offset = ReadUE(buf, rbsp_size, pos);
        conf_win_top_offset = ReadUE(buf, rbsp_size, pos);
        conf_win_bottom_offset = ReadUE(buf, rbsp_size, pos);
    }

    // bit_depth_luma_minus8
    info.bit_depth_luma = 8 + ReadUE(buf, rbsp_size, pos);

    // bit_depth_chroma_minus8
    info.bit_depth_chroma = 8 + ReadUE(buf, rbsp_size, pos);

    // Calculate actual resolution
    uint32_t sub_width_c = (info.chroma_format_idc == 1 || info.chroma_format_idc == 2) ? 2 : 1;
    uint32_t sub_height_c = (info.chroma_format_idc == 1) ? 2 : 1;

    info.width = pic_width_in_luma_samples - sub_width_c * (conf_win_left_offset + conf_win_right_offset);
    info.height = pic_height_in_luma_samples - sub_height_c * (conf_win_top_offset + conf_win_bottom_offset);

    info.valid = true;
    return info;
}

} // namespace

H265VideoInfo H265Parser::ParseSPS(const std::shared_ptr<lmcore::DataBuffer> &sps)
{
    if (!sps || sps->Size() < 15) {
        return {};
    }

    auto data = RemoveStartCode(sps);
    return ParseSPSInternal(data->Data(), data->Size());
}

bool H265Parser::GetResolution(const std::shared_ptr<lmcore::DataBuffer> &sps, int32_t &width, int32_t &height)
{
    auto info = ParseSPS(sps);
    if (info.valid) {
        width = info.width;
        height = info.height;
        return true;
    }
    return false;
}

std::shared_ptr<lmcore::DataBuffer> H265Parser::RemoveStartCode(const std::shared_ptr<lmcore::DataBuffer> &data)
{
    if (!data || data->Size() < 3) {
        return data;
    }

    const uint8_t *buf = data->Data();
    size_t size = data->Size();

    size_t offset = 0;
    if (size >= 4 && buf[0] == 0x00 && buf[1] == 0x00 && buf[2] == 0x00 && buf[3] == 0x01) {
        offset = 4;
    } else if (size >= 3 && buf[0] == 0x00 && buf[1] == 0x00 && buf[2] == 0x01) {
        offset = 3;
    }

    if (offset > 0) {
        auto result = lmcore::DataBuffer::Create(size - offset);
        result->Assign(buf + offset, size - offset);
        return result;
    }

    return data;
}

bool H265Parser::HasStartCode(const std::shared_ptr<lmcore::DataBuffer> &data)
{
    if (!data || data->Size() < 3) {
        return false;
    }

    const uint8_t *buf = data->Data();
    size_t size = data->Size();

    if (size >= 4 && buf[0] == 0x00 && buf[1] == 0x00 && buf[2] == 0x00 && buf[3] == 0x01) {
        return true;
    }

    if (size >= 3 && buf[0] == 0x00 && buf[1] == 0x00 && buf[2] == 0x01) {
        return true;
    }

    return false;
}

int32_t H265Parser::GetNaluType(const std::shared_ptr<lmcore::DataBuffer> &data)
{
    if (!data || data->Size() < 1) {
        return -1;
    }

    auto nalu_data = RemoveStartCode(data);
    if (!nalu_data || nalu_data->Size() < 2) {
        return -1;
    }

    // H.265 NAL unit type is in bits [1-6] of the first byte after start code
    // NAL header is 2 bytes: |F|Type(6)|LayerId(6)|TID(3)|
    uint8_t first_byte = nalu_data->Data()[0];
    return (first_byte >> 1) & 0x3F; // Extract 6 bits
}

bool H265Parser::IsKeyFrame(const std::shared_ptr<lmcore::DataBuffer> &data)
{
    int32_t nalu_type = GetNaluType(data);
    // H.265 NALU types for key frames:
    // 19-20: IDR frames (IDR_W_RADL, IDR_N_LP)
    // 21: CRA (Clean Random Access)
    return (nalu_type >= 19 && nalu_type <= 21);
}

bool H265Parser::ExtractVPSSPSPPS(const std::shared_ptr<lmcore::DataBuffer> &data,
                                  std::shared_ptr<lmcore::DataBuffer> &vps, std::shared_ptr<lmcore::DataBuffer> &sps,
                                  std::shared_ptr<lmcore::DataBuffer> &pps)
{
    if (!data || data->Size() < 10) {
        return false;
    }

    const uint8_t *buf = data->Data();
    size_t size = data->Size();

    bool found_vps = false;
    bool found_sps = false;
    bool found_pps = false;

    for (size_t i = 0; i < size - 4; i++) {
        // Find start code
        if (buf[i] != 0x00 || buf[i + 1] != 0x00) {
            continue;
        }

        size_t start_code_len = 0;
        if (buf[i + 2] == 0x01) {
            start_code_len = 3;
        } else if (buf[i + 2] == 0x00 && i + 3 < size && buf[i + 3] == 0x01) {
            start_code_len = 4;
        } else {
            continue;
        }

        size_t nalu_start = i + start_code_len;
        if (nalu_start >= size) {
            break;
        }

        // Get NAL unit type (6 bits after the start code)
        uint8_t nalu_type = (buf[nalu_start] >> 1) & 0x3F;

        // Find next start code to get NAL unit length
        size_t nalu_end = size;
        for (size_t j = nalu_start + 1; j < size - 3; j++) {
            if (buf[j] == 0x00 && buf[j + 1] == 0x00 &&
                (buf[j + 2] == 0x01 || (buf[j + 2] == 0x00 && j + 3 < size && buf[j + 3] == 0x01))) {
                nalu_end = j;
                break;
            }
        }

        size_t nalu_len = nalu_end - nalu_start;

        // H.265 NAL unit types: VPS=32, SPS=33, PPS=34
        if (nalu_type == 32 && !found_vps) { // VPS
            vps = lmcore::DataBuffer::Create(nalu_len);
            vps->Assign(buf + nalu_start, nalu_len);
            found_vps = true;
        } else if (nalu_type == 33 && !found_sps) { // SPS
            sps = lmcore::DataBuffer::Create(nalu_len);
            sps->Assign(buf + nalu_start, nalu_len);
            found_sps = true;
        } else if (nalu_type == 34 && !found_pps) { // PPS
            pps = lmcore::DataBuffer::Create(nalu_len);
            pps->Assign(buf + nalu_start, nalu_len);
            found_pps = true;
        }

        if (found_vps && found_sps && found_pps) {
            return true;
        }

        i = nalu_end - 1;
    }

    return found_vps && found_sps && found_pps;
}

std::string H265Parser::GetProfileName(int32_t profile_idc)
{
    switch (profile_idc) {
        case 1:
            return "Main";
        case 2:
            return "Main 10";
        case 3:
            return "Main Still Picture";
        case 4:
            return "Format Range Extensions";
        default:
            return "Unknown";
    }
}

std::string H265Parser::GetLevelString(int32_t level_idc)
{
    int32_t major = level_idc / 30;
    int32_t minor = (level_idc % 30) / 3;
    return std::to_string(major) + "." + std::to_string(minor);
}

} // namespace lmshao::lmrtsp
