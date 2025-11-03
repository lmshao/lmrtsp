/**
 * @author SHAO Liming <lmshao@163.com>
 * @copyright Copyright (c) 2025 SHAO Liming
 * @license MIT
 *
 * SPDX-License-Identifier: MIT
 */

#include "lmrtsp/h264_parser.h"

#include <cmath>
#include <cstring>
#include <vector>

namespace lmshao::lmrtsp {

namespace {
// Internal helper functions using raw pointers for actual parsing logic

uint32_t ReadUE(const uint8_t *buf, uint32_t len, uint32_t &pos)
{
    // Count leading zeros
    uint32_t zero_count = 0;
    while (pos < len * 8) {
        if (buf[pos / 8] & (0x80 >> (pos % 8))) {
            break;
        }
        zero_count++;
        pos++;
    }
    pos++; // Skip the '1' bit

    // Read the remaining bits
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
        // Check for 0x000003 pattern
        if (buf[i] == 0x00 && buf[i + 1] == 0x00 && buf[i + 2] == 0x03) {
            // Remove the 0x03 byte
            for (uint32_t j = i + 2; j < tmp_size - 1; j++) {
                buf[j] = buf[j + 1];
            }
            size--;
            tmp_size = size;
        }
    }
}

void SkipScalingList(const uint8_t *buf, uint32_t len, uint32_t &pos, int32_t size_of_scaling_list)
{
    int32_t last_scale = 8;
    int32_t next_scale = 8;

    for (int32_t j = 0; j < size_of_scaling_list; j++) {
        if (next_scale != 0) {
            int32_t delta_scale = ReadSE(buf, len, pos);
            next_scale = (last_scale + delta_scale) & 0xff;
        }
        last_scale = (next_scale == 0) ? last_scale : next_scale;
    }
}

H264VideoInfo ParseSPSInternal(const uint8_t *sps, size_t size)
{
    H264VideoInfo info;

    if (!sps || size < 4) {
        return info;
    }

    // Make a copy to remove emulation prevention bytes
    std::vector<uint8_t> sps_copy(sps, sps + size);
    uint32_t sps_size = static_cast<uint32_t>(size);

    RemoveEmulationPrevention(sps_copy.data(), sps_size);

    const uint8_t *buf = sps_copy.data();
    uint32_t len = sps_size;
    uint32_t pos = 0;

    // Parse NAL unit header
    ReadBits(1, buf, pos);                          // forbidden_zero_bit
    ReadBits(2, buf, pos);                          // nal_ref_idc
    uint32_t nal_unit_type = ReadBits(5, buf, pos); // nal_unit_type

    // Check if this is SPS (type 7)
    if (nal_unit_type != 7) {
        return info;
    }

    // Parse SPS
    info.profile_idc = ReadBits(8, buf, pos);
    ReadBits(8, buf, pos); // constraint flags and reserved bits
    info.level_idc = ReadBits(8, buf, pos);
    ReadUE(buf, len, pos); // seq_parameter_set_id

    // High profile check
    if (info.profile_idc == 100 || info.profile_idc == 110 || info.profile_idc == 122 || info.profile_idc == 244 ||
        info.profile_idc == 44 || info.profile_idc == 83 || info.profile_idc == 86 || info.profile_idc == 118 ||
        info.profile_idc == 128 || info.profile_idc == 138 || info.profile_idc == 139 || info.profile_idc == 134 ||
        info.profile_idc == 135) {

        info.chroma_format_idc = ReadUE(buf, len, pos);

        if (info.chroma_format_idc == 3) {
            ReadBits(1, buf, pos); // separate_colour_plane_flag
        }

        int32_t bit_depth_luma_minus8 = ReadUE(buf, len, pos);
        info.bit_depth_luma = 8 + bit_depth_luma_minus8;

        int32_t bit_depth_chroma_minus8 = ReadUE(buf, len, pos);
        info.bit_depth_chroma = 8 + bit_depth_chroma_minus8;

        ReadBits(1, buf, pos); // qpprime_y_zero_transform_bypass_flag

        uint32_t seq_scaling_matrix_present_flag = ReadBits(1, buf, pos);

        if (seq_scaling_matrix_present_flag) {
            int32_t scaling_list_count = (info.chroma_format_idc != 3) ? 8 : 12;
            for (int32_t i = 0; i < scaling_list_count; i++) {
                uint32_t seq_scaling_list_present_flag = ReadBits(1, buf, pos);
                if (seq_scaling_list_present_flag) {
                    int32_t size_of_scaling_list = (i < 6) ? 16 : 64;
                    SkipScalingList(buf, len, pos, size_of_scaling_list);
                }
            }
        }
    } else {
        info.chroma_format_idc = 1; // Default 4:2:0
    }

    ReadUE(buf, len, pos); // log2_max_frame_num_minus4

    uint32_t pic_order_cnt_type = ReadUE(buf, len, pos);

    if (pic_order_cnt_type == 0) {
        ReadUE(buf, len, pos); // log2_max_pic_order_cnt_lsb_minus4
    } else if (pic_order_cnt_type == 1) {
        ReadBits(1, buf, pos); // delta_pic_order_always_zero_flag
        ReadSE(buf, len, pos); // offset_for_non_ref_pic
        ReadSE(buf, len, pos); // offset_for_top_to_bottom_field
        uint32_t num_ref_frames = ReadUE(buf, len, pos);

        // Skip offset_for_ref_frame
        for (uint32_t i = 0; i < num_ref_frames; i++) {
            ReadSE(buf, len, pos);
        }
    }

    ReadUE(buf, len, pos); // max_num_ref_frames
    ReadBits(1, buf, pos); // gaps_in_frame_num_value_allowed_flag

    uint32_t pic_width_in_mbs_minus1 = ReadUE(buf, len, pos);
    uint32_t pic_height_in_map_units_minus1 = ReadUE(buf, len, pos);

    info.width = (pic_width_in_mbs_minus1 + 1) * 16;
    info.height = (pic_height_in_map_units_minus1 + 1) * 16;

    uint32_t frame_mbs_only_flag = ReadBits(1, buf, pos);
    info.frame_mbs_only_flag = (frame_mbs_only_flag != 0);

    if (!frame_mbs_only_flag) {
        ReadBits(1, buf, pos); // mb_adaptive_frame_field_flag
    }

    ReadBits(1, buf, pos); // direct_8x8_inference_flag

    uint32_t frame_cropping_flag = ReadBits(1, buf, pos);

    if (frame_cropping_flag) {
        uint32_t frame_crop_left_offset = ReadUE(buf, len, pos);
        uint32_t frame_crop_right_offset = ReadUE(buf, len, pos);
        uint32_t frame_crop_top_offset = ReadUE(buf, len, pos);
        uint32_t frame_crop_bottom_offset = ReadUE(buf, len, pos);

        int32_t crop_unit_x = 2;
        int32_t crop_unit_y = 2 * (2 - frame_mbs_only_flag);

        info.width -= crop_unit_x * (frame_crop_left_offset + frame_crop_right_offset);
        info.height -= crop_unit_y * (frame_crop_top_offset + frame_crop_bottom_offset);
    }

    info.valid = true;
    return info;
}

} // anonymous namespace

H264VideoInfo H264Parser::ParseSPS(const std::shared_ptr<lmcore::DataBuffer> &sps)
{
    if (!sps || sps->Empty()) {
        return H264VideoInfo();
    }
    return ParseSPSInternal(sps->Data(), sps->Size());
}

bool H264Parser::GetResolution(const std::shared_ptr<lmcore::DataBuffer> &sps, int32_t &width, int32_t &height)
{
    if (!sps || sps->Empty()) {
        return false;
    }

    H264VideoInfo info = ParseSPSInternal(sps->Data(), sps->Size());
    if (info.valid) {
        width = info.width;
        height = info.height;
        return true;
    }
    return false;
}

std::shared_ptr<lmcore::DataBuffer> H264Parser::RemoveStartCode(const std::shared_ptr<lmcore::DataBuffer> &data)
{
    if (!data || data->Size() < 3) {
        return data;
    }

    const uint8_t *raw_data = data->Data();
    size_t start_pos = 0;

    // Check for 4-byte start code (0x00000001)
    if (data->Size() >= 4 && raw_data[0] == 0x00 && raw_data[1] == 0x00 && raw_data[2] == 0x00 && raw_data[3] == 0x01) {
        start_pos = 4;
    }
    // Check for 3-byte start code (0x000001)
    else if (raw_data[0] == 0x00 && raw_data[1] == 0x00 && raw_data[2] == 0x01) {
        start_pos = 3;
    }

    if (start_pos > 0) {
        auto result = lmcore::DataBuffer::Create(data->Size() - start_pos);
        result->Assign(raw_data + start_pos, data->Size() - start_pos);
        return result;
    }

    return data;
}

bool H264Parser::HasStartCode(const std::shared_ptr<lmcore::DataBuffer> &data)
{
    if (!data || data->Size() < 3) {
        return false;
    }

    const uint8_t *raw_data = data->Data();

    // Check for 3-byte start code (0x000001)
    if (raw_data[0] == 0x00 && raw_data[1] == 0x00 && raw_data[2] == 0x01) {
        return true;
    }

    // Check for 4-byte start code (0x00000001)
    if (data->Size() >= 4 && raw_data[0] == 0x00 && raw_data[1] == 0x00 && raw_data[2] == 0x00 && raw_data[3] == 0x01) {
        return true;
    }

    return false;
}

int32_t H264Parser::GetNaluType(const std::shared_ptr<lmcore::DataBuffer> &data)
{
    if (!data || data->Empty()) {
        return -1;
    }

    const uint8_t *raw_data = data->Data();
    size_t size = data->Size();
    size_t offset = 0;

    // Skip start code if present
    if (size >= 4 && raw_data[0] == 0x00 && raw_data[1] == 0x00 && raw_data[2] == 0x00 && raw_data[3] == 0x01) {
        offset = 4;
    } else if (size >= 3 && raw_data[0] == 0x00 && raw_data[1] == 0x00 && raw_data[2] == 0x01) {
        offset = 3;
    }

    if (offset >= size) {
        return -1;
    }

    // NAL unit type is in the lower 5 bits of the first byte
    return raw_data[offset] & 0x1F;
}

bool H264Parser::IsKeyFrame(const std::shared_ptr<lmcore::DataBuffer> &data)
{
    int32_t nalu_type = GetNaluType(data);
    // NALU type 5 is IDR (Instantaneous Decoding Refresh) - key frame
    return nalu_type == 5;
}

bool H264Parser::ExtractSPSPPS(const std::shared_ptr<lmcore::DataBuffer> &data,
                               std::shared_ptr<lmcore::DataBuffer> &sps, std::shared_ptr<lmcore::DataBuffer> &pps)
{
    if (!data || data->Size() < 8) {
        return false;
    }

    const uint8_t *raw_data = data->Data();
    size_t size = data->Size();

    bool sps_found = false;
    bool pps_found = false;

    for (size_t i = 0; i < size - 4; ++i) {
        // Look for start code (4-byte: 0x00000001)
        if (raw_data[i] == 0x00 && raw_data[i + 1] == 0x00 && raw_data[i + 2] == 0x00 && raw_data[i + 3] == 0x01) {
            if (i + 4 >= size) {
                break;
            }

            uint8_t nalu_type = raw_data[i + 4] & 0x1F;

            // Only process SPS (7) and PPS (8)
            if (nalu_type != 7 && nalu_type != 8) {
                continue;
            }

            // Find end of this NALU
            size_t nalu_start = i + 4;
            size_t nalu_end = size;

            // Search for next start code
            for (size_t j = nalu_start + 1; j < size - 3; ++j) {
                // Check for 4-byte start code
                if (raw_data[j] == 0x00 && raw_data[j + 1] == 0x00 && raw_data[j + 2] == 0x00 &&
                    raw_data[j + 3] == 0x01) {
                    nalu_end = j;
                    break;
                }
                // Check for 3-byte start code
                if (raw_data[j] == 0x00 && raw_data[j + 1] == 0x00 && raw_data[j + 2] == 0x01) {
                    nalu_end = j;
                    break;
                }
            }

            size_t nalu_size = nalu_end - nalu_start;
            if (nalu_size > 0 && nalu_size < 1024) { // Sanity check
                auto buffer = lmcore::DataBuffer::Create(nalu_size);
                buffer->Assign(raw_data + nalu_start, nalu_size);

                if (nalu_type == 7) { // SPS
                    sps = buffer;
                    sps_found = true;
                } else if (nalu_type == 8) { // PPS
                    pps = buffer;
                    pps_found = true;
                }
            }

            // Early exit if both found
            if (sps_found && pps_found) {
                return true;
            }
        }
    }

    return sps_found && pps_found;
}

std::string H264Parser::GetProfileName(int32_t profile_idc)
{
    switch (profile_idc) {
        case 66:
            return "Baseline";
        case 77:
            return "Main";
        case 88:
            return "Extended";
        case 100:
            return "High";
        case 110:
            return "High 10";
        case 122:
            return "High 4:2:2";
        case 244:
            return "High 4:4:4";
        case 44:
            return "CAVLC 4:4:4";
        case 83:
            return "Scalable Baseline";
        case 86:
            return "Scalable High";
        case 118:
            return "Multiview High";
        case 128:
            return "Stereo High";
        default:
            return "Unknown";
    }
}

std::string H264Parser::GetLevelString(int32_t level_idc)
{
    // Level IDC is 10 times the actual level
    // e.g., 30 = Level 3.0, 42 = Level 4.2
    if (level_idc < 10 || level_idc > 62) {
        return "Unknown";
    }

    int major = level_idc / 10;
    int minor = level_idc % 10;

    return std::to_string(major) + "." + std::to_string(minor);
}

} // namespace lmshao::lmrtsp
