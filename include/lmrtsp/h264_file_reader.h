/**
 * @author SHAO Liming <lmshao@163.com>
 * @copyright Copyright (c) 2025 SHAO Liming
 * @license MIT
 *
 * SPDX-License-Identifier: MIT
 */

#ifndef LMSHAO_LMRTSP_H264_FILE_READER_H
#define LMSHAO_LMRTSP_H264_FILE_READER_H

#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

namespace lmshao::lmrtsp {

/**
 * @brief H.264 NAL unit type
 */
enum class NalUnitType : uint8_t {
    UNSPECIFIED = 0,
    SLICE = 1,
    SLICE_DPA = 2,
    SLICE_DPB = 3,
    SLICE_DPC = 4,
    SLICE_IDR = 5,
    SEI = 6,
    SPS = 7,
    PPS = 8,
    AUD = 9,
    END_SEQUENCE = 10,
    END_STREAM = 11,
    FILLER = 12
};

/**
 * @brief H.264 NAL unit structure
 */
struct NalUnit {
    std::vector<uint8_t> data;
    NalUnitType type;
    uint32_t timestamp;
    bool is_keyframe;

    NalUnit() : type(NalUnitType::UNSPECIFIED), timestamp(0), is_keyframe(false) {}
};

/**
 * @brief H.264 file reader class
 *
 * This class reads H.264 files, extracts NAL units, and provides
 * frame-by-frame access for streaming.
 */
class H264FileReader {
public:
    H264FileReader();
    ~H264FileReader();

    /**
     * @brief Open H.264 file
     * @param filename Path to H.264 file
     * @return true if successful, false otherwise
     */
    bool Open(const std::string &filename);

    /**
     * @brief Close the file
     */
    void Close();

    /**
     * @brief Check if file is opened
     * @return true if file is opened
     */
    bool IsOpened() const;

    /**
     * @brief Get next frame
     * @param frame Output frame data
     * @return true if frame is available, false if EOF or error
     */
    bool GetNextFrame(std::vector<uint8_t> &frame);

    /**
     * @brief Get next NAL unit
     * @param nal_unit Output NAL unit
     * @return true if NAL unit is available, false if EOF or error
     */
    bool GetNextNalUnit(NalUnit &nal_unit);

    /**
     * @brief Reset to beginning of file
     */
    void Reset();

    /**
     * @brief Enable/disable loop mode
     * @param enable true to enable looping
     */
    void SetLoopMode(bool enable);

    /**
     * @brief Get SPS (Sequence Parameter Set)
     * @return SPS data
     */
    std::vector<uint8_t> GetSPS() const;

    /**
     * @brief Get PPS (Picture Parameter Set)
     * @return PPS data
     */
    std::vector<uint8_t> GetPPS() const;

    /**
     * @brief Get video width and height from SPS
     * @param width Output width
     * @param height Output height
     * @return true if successful
     */
    bool GetResolution(uint32_t &width, uint32_t &height) const;

    /**
     * @brief Get frame rate (estimated from file)
     * @return Estimated frame rate
     */
    uint32_t GetFrameRate() const;

    /**
     * @brief Get file duration in seconds (estimated)
     * @return Estimated duration
     */
    double GetDuration() const;

private:
    /**
     * @brief Find NAL unit start codes
     * @param data Buffer to search
     * @param size Buffer size
     * @param start_pos Start position
     * @return Position of NAL unit start, or -1 if not found
     */
    int FindNalUnitStart(const uint8_t *data, size_t size, size_t start_pos);

    /**
     * @brief Parse SPS to extract resolution
     * @param sps SPS data
     */
    void ParseSPS(const std::vector<uint8_t> &sps);

    /**
     * @brief Read and parse all NAL units from file
     */
    void ParseFile();

    /**
     * @brief Convert 3-byte or 4-byte start codes to length prefixes
     * @param data Input data with start codes
     * @param size Input data size
     * @return Data with length prefixes
     */
    std::vector<uint8_t> ConvertToLengthPrefix(const uint8_t *data, size_t size);

private:
    std::string filename_;
    std::vector<uint8_t> file_data_;
    std::vector<NalUnit> nal_units_;
    size_t current_nal_index_;

    // Video parameters
    std::vector<uint8_t> sps_;
    std::vector<uint8_t> pps_;
    uint32_t width_;
    uint32_t height_;
    uint32_t frame_rate_;
    double duration_;

    // Control flags
    std::atomic<bool> is_opened_;
    std::atomic<bool> loop_mode_;
    mutable std::mutex mutex_;
};

} // namespace lmshao::lmrtsp

#endif // LMSHAO_LMRTSP_H264_FILE_READER_H