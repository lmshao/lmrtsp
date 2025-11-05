/**
 * @author SHAO Liming <lmshao@163.com>
 * @copyright Copyright (c) 2025 SHAO Liming
 * @license MIT
 *
 * SPDX-License-Identifier: MIT
 */

#include "h265_file_reader.h"

#include <iostream>

#include "lmrtsp/h265_parser.h"

H265FileReader::H265FileReader(const std::string &filename)
    : filename_(filename), frame_rate_(25), frame_count_(0), parameter_sets_extracted_(false),
      read_buffer_(BUFFER_SIZE), buffer_pos_(0), buffer_end_(0), eof_reached_(false)
{
}

H265FileReader::~H265FileReader()
{
    Close();
}

bool H265FileReader::Open()
{
    if (file_.is_open()) {
        return true;
    }

    file_.open(filename_, std::ios::binary);
    if (!file_.is_open()) {
        std::cerr << "Failed to open H.265 file: " << filename_ << std::endl;
        return false;
    }

    buffer_pos_ = 0;
    buffer_end_ = 0;
    eof_reached_ = false;

    ExtractParameterSets();
    AnalyzeFile();
    Reset();

    std::cout << "Opened H.265 file: " << filename_ << ", Frame rate: " << frame_rate_
              << " fps, Estimated frames: " << frame_count_ << std::endl;

    return true;
}

void H265FileReader::Close()
{
    if (file_.is_open()) {
        file_.close();
    }
    buffer_pos_ = 0;
    buffer_end_ = 0;
    eof_reached_ = false;
}

bool H265FileReader::IsOpen() const
{
    return file_.is_open();
}

bool H265FileReader::ReadFrame(MediaFrame &frame)
{
    if (!file_.is_open() || eof_reached_) {
        return false;
    }

    std::vector<uint8_t> nalu_data;
    uint8_t byte;
    uint32_t start_code_candidate = 0;
    bool found_start = false;

    while (file_.read(reinterpret_cast<char *>(&byte), 1)) {
        start_code_candidate = (start_code_candidate << 8) | byte;

        if (start_code_candidate == 0x00000001) {
            found_start = true;
            break;
        } else if ((start_code_candidate & 0x00FFFFFF) == 0x000001) {
            found_start = true;
            break;
        }
    }

    if (!found_start) {
        eof_reached_ = true;
        return false;
    }

    uint32_t next_start_candidate = 0;

    while (file_.read(reinterpret_cast<char *>(&byte), 1)) {
        next_start_candidate = (next_start_candidate << 8) | byte;

        if (next_start_candidate == 0x00000001) {
            file_.seekg(-4, std::ios::cur);
            break;
        } else if ((next_start_candidate & 0x00FFFFFF) == 0x000001) {
            file_.seekg(-3, std::ios::cur);
            break;
        }

        nalu_data.push_back(byte);
    }

    if (nalu_data.empty()) {
        return false;
    }

    uint8_t start_code[] = {0x00, 0x00, 0x00, 0x01};
    std::vector<uint8_t> frame_with_startcode;
    frame_with_startcode.reserve(4 + nalu_data.size());
    frame_with_startcode.insert(frame_with_startcode.end(), start_code, start_code + 4);
    frame_with_startcode.insert(frame_with_startcode.end(), nalu_data.begin(), nalu_data.end());

    frame.data = lmshao::lmcore::DataBuffer::Create(frame_with_startcode.size());
    frame.data->Assign(frame_with_startcode.data(), frame_with_startcode.size());

    return true;
}

void H265FileReader::Reset()
{
    if (file_.is_open()) {
        file_.clear();
        file_.seekg(0, std::ios::beg);
        buffer_pos_ = 0;
        buffer_end_ = 0;
        eof_reached_ = false;
    }
}

std::vector<uint8_t> H265FileReader::GetVPS() const
{
    return vps_;
}

std::vector<uint8_t> H265FileReader::GetSPS() const
{
    return sps_;
}

std::vector<uint8_t> H265FileReader::GetPPS() const
{
    return pps_;
}

uint32_t H265FileReader::GetFrameRate() const
{
    return frame_rate_;
}

size_t H265FileReader::GetFrameCount() const
{
    return frame_count_;
}

bool H265FileReader::GetResolution(uint32_t &width, uint32_t &height) const
{
    if (sps_.empty()) {
        width = 1280;
        height = 720;
        return false;
    }

    auto sps_buffer = lmshao::lmcore::DataBuffer::Create(sps_.size());
    sps_buffer->Assign(sps_.data(), sps_.size());

    int32_t w = 0, h = 0;
    if (lmshao::lmrtsp::H265Parser::GetResolution(sps_buffer, w, h)) {
        width = static_cast<uint32_t>(w);
        height = static_cast<uint32_t>(h);
        return true;
    }

    width = 1280;
    height = 720;
    return false;
}

double H265FileReader::GetDuration() const
{
    if (frame_rate_ > 0) {
        return static_cast<double>(frame_count_) / frame_rate_;
    }
    return 0.0;
}

bool H265FileReader::GetNextFrame(std::vector<uint8_t> &frame_data)
{
    MediaFrame frame;
    if (ReadFrame(frame)) {
        frame_data.clear();
        frame_data.insert(frame_data.end(), frame.data->Data(), frame.data->Data() + frame.data->Size());
        return true;
    }
    return false;
}

int H265FileReader::FindStartCode(const uint8_t *buffer, size_t start_pos, size_t buffer_size)
{
    if (buffer_size - start_pos < 3) {
        return -1;
    }

    for (size_t i = start_pos; i <= buffer_size - 3; ++i) {
        if (buffer[i] == 0x00 && buffer[i + 1] == 0x00 && buffer[i + 2] == 0x01) {
            return static_cast<int>(i);
        }
        if (i > 0 && buffer[i - 1] == 0x00 && buffer[i] == 0x00 && buffer[i + 1] == 0x00 && buffer[i + 2] == 0x01) {
            return static_cast<int>(i - 1);
        }
    }

    return -1;
}

void H265FileReader::ExtractParameterSets()
{
    if (!file_.is_open() || parameter_sets_extracted_) {
        return;
    }

    auto current_pos = file_.tellg();
    file_.seekg(0, std::ios::beg);

    std::vector<uint8_t> temp_buffer(BUFFER_SIZE);
    file_.read(reinterpret_cast<char *>(temp_buffer.data()), BUFFER_SIZE);
    size_t bytes_read = file_.gcount();

    auto data_buffer = lmshao::lmcore::DataBuffer::Create(bytes_read);
    data_buffer->Assign(temp_buffer.data(), bytes_read);

    std::shared_ptr<lmshao::lmcore::DataBuffer> vps_buffer, sps_buffer, pps_buffer;
    if (lmshao::lmrtsp::H265Parser::ExtractVPSSPSPPS(data_buffer, vps_buffer, sps_buffer, pps_buffer)) {
        vps_.assign(vps_buffer->Data(), vps_buffer->Data() + vps_buffer->Size());
        sps_.assign(sps_buffer->Data(), sps_buffer->Data() + sps_buffer->Size());
        pps_.assign(pps_buffer->Data(), pps_buffer->Data() + pps_buffer->Size());
        std::cout << "Found VPS, size: " << vps_.size() << " bytes" << std::endl;
        std::cout << "Found SPS, size: " << sps_.size() << " bytes" << std::endl;
        std::cout << "Found PPS, size: " << pps_.size() << " bytes" << std::endl;
    }

    file_.seekg(current_pos);
    parameter_sets_extracted_ = true;
}

void H265FileReader::AnalyzeFile()
{
    if (!file_.is_open()) {
        return;
    }

    auto current_pos = file_.tellg();

    file_.seekg(0, std::ios::end);
    size_t file_size = file_.tellg();
    file_.seekg(0, std::ios::beg);

    std::vector<uint8_t> temp_buffer(BUFFER_SIZE);
    size_t total_frames = 0;

    while (file_.good()) {
        file_.read(reinterpret_cast<char *>(temp_buffer.data()), BUFFER_SIZE);
        size_t bytes_read = file_.gcount();

        if (bytes_read == 0)
            break;

        auto data_buffer = lmshao::lmcore::DataBuffer::Create(bytes_read);
        data_buffer->Assign(temp_buffer.data(), bytes_read);

        for (size_t i = 0; i < bytes_read - 4; ++i) {
            if (temp_buffer[i] == 0x00 && temp_buffer[i + 1] == 0x00 && temp_buffer[i + 2] == 0x00 &&
                temp_buffer[i + 3] == 0x01) {

                auto nalu_buffer = lmshao::lmcore::DataBuffer::Create(bytes_read - i);
                nalu_buffer->Assign(temp_buffer.data() + i, bytes_read - i);

                int32_t nalu_type = lmshao::lmrtsp::H265Parser::GetNaluType(nalu_buffer);

                // H.265 frame NALU types: 0-9 (VCL), 16-21 (IDR/CRA/BLA)
                if ((nalu_type >= 0 && nalu_type <= 9) || (nalu_type >= 16 && nalu_type <= 21)) {
                    total_frames++;
                }
            }
        }
    }

    frame_count_ = total_frames;
    frame_rate_ = 25;

    file_.seekg(current_pos);

    std::cout << "File analysis complete. File size: " << file_size << " bytes, Estimated frames: " << frame_count_
              << std::endl;
}
