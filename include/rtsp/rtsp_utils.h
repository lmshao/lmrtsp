/**
 * @author SHAO Liming <lmshao@163.com>
 * @copyright Copyright (c) 2025 SHAO Liming
 * @license MIT
 *
 * SPDX-License-Identifier: MIT
 */

#ifndef LMSHAO_RTSP_RTSP_UTILS_H
#define LMSHAO_RTSP_RTSP_UTILS_H

#include <string>
#include <vector>

namespace lmshao::rtsp {

/**
 * @brief RTSP utility functions for string processing
 */
class RTSPUtils {
public:
    /**
     * @brief Trim whitespace from both ends of a string
     * @param str Input string
     * @return Trimmed string
     */
    static std::string trim(const std::string &str);

    /**
     * @brief Convert string to lowercase
     * @param str Input string
     * @return Lowercase string
     */
    static std::string toLower(const std::string &str);

    /**
     * @brief Split string by a delimiter substring
     * @param str Input string
     * @param delimiter Delimiter string
     * @return Vector of substrings
     */
    static std::vector<std::string> split(const std::string &str, const std::string &delimiter);

private:
    // Prevent instantiation
    RTSPUtils() = delete;
    ~RTSPUtils() = delete;
};

} // namespace lmshao::rtsp

#endif // LMSHAO_RTSP_RTSP_UTILS_H
