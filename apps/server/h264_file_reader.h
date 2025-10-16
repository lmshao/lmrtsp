/**
 * @author SHAO Liming <lmshao@163.com>
 * @copyright Copyright (c) 2025 SHAO Liming
 * @license MIT
 *
 * SPDX-License-Identifier: MIT
 */

#ifndef H264_FILE_READER_H
#define H264_FILE_READER_H

#include <fstream>
#include <string>
#include <vector>

#include "lmrtsp/i_rtp_packetizer.h"

using namespace lmshao::lmrtsp;

/**
 * @brief H.264 file reader class
 * Reads H.264 elementary stream files and extracts NAL units
 */
class H264FileReader {
public:
    /**
     * @brief Constructor
     * @param filename Path to the H.264 file
     */
    explicit H264FileReader(const std::string &filename);

    /**
     * @brief Destructor
     */
    ~H264FileReader();

    /**
     * @brief Open the H.264 file
     * @return true if successful, false otherwise
     */
    bool Open();

    /**
     * @brief Close the H.264 file
     */
    void Close();

    /**
     * @brief Check if file is open
     * @return true if file is open
     */
    bool IsOpen() const;

    /**
     * @brief Read next frame (NALU) from file
     * @param frame Output frame data
     * @return true if frame was read successfully, false if EOF or error
     */
    bool ReadFrame(MediaFrame &frame);

    /**
     * @brief Reset file position to beginning
     */
    void Reset();

    /**
     * @brief Get SPS (Sequence Parameter Set) data
     * @return SPS data or empty vector if not found
     */
    std::vector<uint8_t> GetSPS() const;

    /**
     * @brief Get PPS (Picture Parameter Set) data
     * @return PPS data or empty vector if not found
     */
    std::vector<uint8_t> GetPPS() const;

    /**
     * @brief Get frame rate from file analysis
     * @return Estimated frame rate (default 25 fps)
     */
    uint32_t GetFrameRate() const;

    /**
     * @brief Get total frame count (estimated)
     * @return Estimated frame count
     */
    size_t GetFrameCount() const;

    /**
     * @brief Get video resolution
     * @param width Output width
     * @param height Output height
     * @return true if resolution could be determined
     */
    bool GetResolution(uint32_t &width, uint32_t &height) const;

    /**
     * @brief Get estimated video duration in seconds
     * @return Estimated duration
     */
    double GetDuration() const;

    /**
     * @brief Read next frame data
     * @param frame_data Output frame data
     * @return true if frame was read successfully
     */
    bool GetNextFrame(std::vector<uint8_t> &frame_data);

private:
    /**
     * @brief Find next NAL unit start code
     * @param buffer Buffer to search in
     * @param start_pos Starting position
     * @param buffer_size Buffer size
     * @return Position of start code or -1 if not found
     */
    int FindStartCode(const uint8_t *buffer, size_t start_pos, size_t buffer_size);

    /**
     * @brief Parse and extract parameter sets from file
     */
    void ExtractParameterSets();

    /**
     * @brief Analyze file to determine frame rate and count
     */
    void AnalyzeFile();

private:
    std::string filename_;
    std::ifstream file_;
    std::vector<uint8_t> sps_;
    std::vector<uint8_t> pps_;
    uint32_t frame_rate_;
    size_t frame_count_;
    bool parameter_sets_extracted_;

    // Buffer for reading file data
    static constexpr size_t BUFFER_SIZE = 64 * 1024; // 64KB buffer
    std::vector<uint8_t> read_buffer_;
    size_t buffer_pos_;
    size_t buffer_end_;
    bool eof_reached_;
};

#endif // H264_FILE_READER_H
