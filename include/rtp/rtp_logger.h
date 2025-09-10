/**
 * @author SHAO Liming <lmshao@163.com>
 * @copyright Copyright (c) 2025 SHAO Liming
 * @license MIT
 *
 * SPDX-License-Identifier: MIT
 */

#ifndef LMSHAO_RTP_LOGGER_H
#define LMSHAO_RTP_LOGGER_H

#include <coreutils/logger.h>

#include <mutex>

namespace lmshao::rtp {

// RTP module tag for template specialization
struct RtpModuleTag {};

// Convenience macros for RTP logging - now use Registry pattern
#define RTP_LOGD(fmt, ...)                                                                                             \
    do {                                                                                                               \
        auto &logger = lmshao::rtp::GetRtpLoggerWithAutoInit();                                                        \
        if (logger.ShouldLog(lmshao::coreutils::LogLevel::kDebug)) {                                                   \
            logger.LogWithModuleTag<lmshao::rtp::RtpModuleTag>(lmshao::coreutils::LogLevel::kDebug, __FILE__,          \
                                                               __LINE__, __FUNCTION__, fmt, ##__VA_ARGS__);            \
        }                                                                                                              \
    } while (0)

#define RTP_LOGI(fmt, ...)                                                                                             \
    do {                                                                                                               \
        auto &logger = lmshao::rtp::GetRtpLoggerWithAutoInit();                                                        \
        if (logger.ShouldLog(lmshao::coreutils::LogLevel::kInfo)) {                                                    \
            logger.LogWithModuleTag<lmshao::rtp::RtpModuleTag>(lmshao::coreutils::LogLevel::kInfo, __FILE__, __LINE__, \
                                                               __FUNCTION__, fmt, ##__VA_ARGS__);                      \
        }                                                                                                              \
    } while (0)

#define RTP_LOGW(fmt, ...)                                                                                             \
    do {                                                                                                               \
        auto &logger = lmshao::rtp::GetRtpLoggerWithAutoInit();                                                        \
        if (logger.ShouldLog(lmshao::coreutils::LogLevel::kWarn)) {                                                    \
            logger.LogWithModuleTag<lmshao::rtp::RtpModuleTag>(lmshao::coreutils::LogLevel::kWarn, __FILE__, __LINE__, \
                                                               __FUNCTION__, fmt, ##__VA_ARGS__);                      \
        }                                                                                                              \
    } while (0)

#define RTP_LOGE(fmt, ...)                                                                                             \
    do {                                                                                                               \
        auto &logger = lmshao::rtp::GetRtpLoggerWithAutoInit();                                                        \
        if (logger.ShouldLog(lmshao::coreutils::LogLevel::kError)) {                                                   \
            logger.LogWithModuleTag<lmshao::rtp::RtpModuleTag>(lmshao::coreutils::LogLevel::kError, __FILE__,          \
                                                               __LINE__, __FUNCTION__, fmt, ##__VA_ARGS__);            \
        }                                                                                                              \
    } while (0)

// Inline functions for RTP logger management - now use Registry pattern
// Initialize RTP logger with smart defaults based on build type
inline void InitRtpLogger(lmshao::coreutils::LogLevel level =
#if defined(_DEBUG) || defined(DEBUG) || !defined(NDEBUG)
                              lmshao::coreutils::LogLevel::kDebug,
#else
                              lmshao::coreutils::LogLevel::kWarn,
#endif
                          lmshao::coreutils::LogOutput output = lmshao::coreutils::LogOutput::CONSOLE,
                          const std::string &filename = "")
{
    lmshao::coreutils::LoggerRegistry::InitLogger<RtpModuleTag>(level, output, filename);
}

inline void SetRtpLogLevel(lmshao::coreutils::LogLevel level)
{
    auto &logger = lmshao::coreutils::LoggerRegistry::GetLogger<RtpModuleTag>();
    logger.SetLevel(level);
}

inline lmshao::coreutils::LogLevel GetRtpLogLevel()
{
    auto &logger = lmshao::coreutils::LoggerRegistry::GetLogger<RtpModuleTag>();
    return logger.GetLevel();
}

inline void SetRtpLogOutput(lmshao::coreutils::LogOutput output)
{
    auto &logger = lmshao::coreutils::LoggerRegistry::GetLogger<RtpModuleTag>();
    logger.SetOutput(output);
}

inline void SetRtpLogFile(const std::string &filename)
{
    auto &logger = lmshao::coreutils::LoggerRegistry::GetLogger<RtpModuleTag>();
    logger.SetOutputFile(filename);
}

// Auto-initialization helper function
inline lmshao::coreutils::Logger &GetRtpLoggerWithAutoInit()
{
    static std::once_flag init_flag;
    std::call_once(init_flag, []() {
        // Auto-initialize with smart defaults based on build type
        InitRtpLogger();
    });
    return lmshao::coreutils::LoggerRegistry::GetLogger<RtpModuleTag>();
}

} // namespace lmshao::rtp

#endif // LMSHAO_RTP_LOGGER_H