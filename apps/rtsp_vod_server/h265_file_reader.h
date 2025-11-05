/**
 * @author SHAO Liming <lmshao@163.com>
 * @copyright Copyright (c) 2025 SHAO Liming
 * @license MIT
 *
 * SPDX-License-Identifier: MIT
 */

#ifndef H265_FILE_READER_H
#define H265_FILE_READER_H

#include <fstream>
#include <string>
#include <vector>

#include "lmrtsp/rtsp_media_stream_manager.h"

using namespace lmshao::lmrtsp;

/**
 * @brief H.265/HEVC file reader class
 */
class H265FileReader {
public:
    explicit H265FileReader(const std::string &filename);
    ~H265FileReader();

    bool Open();
    void Close();
    bool IsOpen() const;
    bool ReadFrame(MediaFrame &frame);
    void Reset();

    std::vector<uint8_t> GetVPS() const;
    std::vector<uint8_t> GetSPS() const;
    std::vector<uint8_t> GetPPS() const;
    uint32_t GetFrameRate() const;
    size_t GetFrameCount() const;
    bool GetResolution(uint32_t &width, uint32_t &height) const;
    double GetDuration() const;
    bool GetNextFrame(std::vector<uint8_t> &frame_data);

private:
    int FindStartCode(const uint8_t *buffer, size_t start_pos, size_t buffer_size);
    void ExtractParameterSets();
    void AnalyzeFile();

private:
    std::string filename_;
    std::ifstream file_;
    std::vector<uint8_t> vps_;
    std::vector<uint8_t> sps_;
    std::vector<uint8_t> pps_;
    uint32_t frame_rate_;
    size_t frame_count_;
    bool parameter_sets_extracted_;

    static constexpr size_t BUFFER_SIZE = 64 * 1024;
    std::vector<uint8_t> read_buffer_;
    size_t buffer_pos_;
    size_t buffer_end_;
    bool eof_reached_;
};

#endif // H265_FILE_READER_H
