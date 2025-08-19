/**
 * @author SHAO Liming <lmshao@163.com>
 * @copyright Copyright (c) 2025 SHAO Liming
 * @license MIT
 *
 * SPDX-License-Identifier: MIT
 */

#include "mpegts/adts_header.h"

#include <cstring>

namespace lmshao::rtsp::mpegts {

static const int SAMPLING_FREQUENCY_TABLE[] = {96000, 88200, 64000, 48000, 44100, 32000, 24000,
                                               22050, 16000, 12000, 11025, 8000,  7350};

ADTSHeader::ADTSHeader() = default;

ADTSHeader::ADTSHeader(int sample_rate, int channels, int frame_length)
{
    SetSampleRate(sample_rate);
    SetChannels(channels);
    SetFrameLength(frame_length);
}

bool ADTSHeader::Parse(const uint8_t *data, size_t size)
{
    if (size < 7) {
        return false;
    }

    sync_word_ = (data[0] << 4) | (data[1] >> 4);
    if (sync_word_ != 0xFFF) {
        return false;
    }

    id_ = (data[1] >> 3) & 0x01;
    layer_ = (data[1] >> 1) & 0x03;
    protection_absent_ = data[1] & 0x01;

    profile_ = (data[2] >> 6) & 0x03;
    sampling_frequency_index_ = (data[2] >> 2) & 0x0F;
    private_bit_ = (data[2] >> 1) & 0x01;
    channel_configuration_ = ((data[2] & 0x01) << 2) | ((data[3] >> 6) & 0x03);

    original_copy_ = (data[3] >> 5) & 0x01;
    home_ = (data[3] >> 4) & 0x01;
    copyright_identification_bit_ = (data[3] >> 3) & 0x01;
    copyright_identification_start_ = (data[3] >> 2) & 0x01;

    aac_frame_length_ =
        ((uint16_t)(data[3] & 0x03) << 11) | ((uint16_t)data[4] << 3) | ((uint16_t)(data[5] >> 5) & 0x07);

    adts_buffer_fullness_ = ((uint16_t)(data[5] & 0x1F) << 6) | ((uint16_t)(data[6] >> 2) & 0x3F);
    number_of_raw_data_blocks_in_frame_ = data[6] & 0x03;

    return true;
}

int ADTSHeader::Generate(uint8_t *buffer) const
{
    buffer[0] = 0xFF;
    buffer[1] = 0xF0 | (id_ << 3) | (layer_ << 1) | (protection_absent_ ? 1 : 0);
    buffer[2] =
        (profile_ << 6) | (sampling_frequency_index_ << 2) | (private_bit_ << 1) | (channel_configuration_ >> 2);
    buffer[3] = ((channel_configuration_ & 0x03) << 6) | (original_copy_ << 5) | (home_ << 4) |
                (copyright_identification_bit_ << 3) | (copyright_identification_start_ << 2) |
                ((aac_frame_length_ >> 11) & 0x03);
    buffer[4] = (aac_frame_length_ >> 3) & 0xFF;
    buffer[5] = ((aac_frame_length_ & 0x07) << 5) | ((adts_buffer_fullness_ >> 6) & 0x1F);
    buffer[6] = ((adts_buffer_fullness_ & 0x3F) << 2) | number_of_raw_data_blocks_in_frame_;
    return 7;
}

int ADTSHeader::GetSampleRate() const
{
    return GetSampleRateFromIndex(sampling_frequency_index_);
}

int ADTSHeader::GetChannels() const
{
    return channel_configuration_;
}

int ADTSHeader::GetFrameLength() const
{
    return aac_frame_length_;
}

int ADTSHeader::GetProfile() const
{
    return profile_;
}

bool ADTSHeader::IsProtectionAbsent() const
{
    return protection_absent_;
}

int ADTSHeader::GetHeaderSize() const
{
    return protection_absent_ ? 7 : 9;
}

void ADTSHeader::SetSampleRate(int sample_rate)
{
    sampling_frequency_index_ = GetIndexFromSampleRate(sample_rate);
}

void ADTSHeader::SetChannels(int channels)
{
    channel_configuration_ = channels;
}

void ADTSHeader::SetFrameLength(int frame_length)
{
    aac_frame_length_ = frame_length;
}

void ADTSHeader::SetProfile(int profile)
{
    profile_ = profile;
}

void ADTSHeader::SetProtectionAbsent(bool protection_absent)
{
    protection_absent_ = protection_absent;
}

int ADTSHeader::GetSampleRateFromIndex(int index)
{
    if (index >= 0 && index < sizeof(SAMPLING_FREQUENCY_TABLE) / sizeof(SAMPLING_FREQUENCY_TABLE[0])) {
        return SAMPLING_FREQUENCY_TABLE[index];
    }
    return 0;
}

int ADTSHeader::GetIndexFromSampleRate(int sample_rate)
{
    for (int i = 0; i < sizeof(SAMPLING_FREQUENCY_TABLE) / sizeof(SAMPLING_FREQUENCY_TABLE[0]); ++i) {
        if (SAMPLING_FREQUENCY_TABLE[i] == sample_rate) {
            return i;
        }
    }
    return 0; // Default to 0 if not found
}

} // namespace lmshao::rtsp::mpegts