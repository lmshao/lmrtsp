/**
 * @author SHAO Liming <lmshao@163.com>
 * @copyright Copyright (c) 2025 SHAO Liming
 * @license MIT
 *
 * SPDX-License-Identifier: MIT
 */

#ifndef LMSHAO_RTSP_LOGGER_H
#define LMSHAO_RTSP_LOGGER_H

#include <coreutils/logger.h>

#include <iostream>
#include <mutex>

namespace lmshao::rtsp {

// RTSP module tag for template specialization
struct RtspModuleTag {};

// RTSP module registration will be done lazily in GetRtspLogger()

// Convenience macros for RTSP logging - now use Registry pattern
#define RTSP_LOGD(fmt, ...)                                                                                            \
    do {                                                                                                               \
        auto &logger = lmshao::rtsp::GetRtspLogger();                                                                  \
        if (logger.ShouldLog(lmshao::coreutils::LogLevel::kDebug)) {                                                   \
            logger.LogWithModuleTag<lmshao::rtsp::RtspModuleTag>(lmshao::coreutils::LogLevel::kDebug, __FILE__,        \
                                                                 __LINE__, __FUNCTION__, fmt, ##__VA_ARGS__);          \
        }                                                                                                              \
    } while (0)

#define RTSP_LOGI(fmt, ...)                                                                                            \
    do {                                                                                                               \
        auto &logger = lmshao::rtsp::GetRtspLogger();                                                                  \
        if (logger.ShouldLog(lmshao::coreutils::LogLevel::kInfo)) {                                                    \
            logger.LogWithModuleTag<lmshao::rtsp::RtspModuleTag>(lmshao::coreutils::LogLevel::kInfo, __FILE__,         \
                                                                 __LINE__, __FUNCTION__, fmt, ##__VA_ARGS__);          \
        }                                                                                                              \
    } while (0)

#define RTSP_LOGW(fmt, ...)                                                                                            \
    do {                                                                                                               \
        auto &logger = lmshao::rtsp::GetRtspLogger();                                                                  \
        if (logger.ShouldLog(lmshao::coreutils::LogLevel::kWarn)) {                                                    \
            logger.LogWithModuleTag<lmshao::rtsp::RtspModuleTag>(lmshao::coreutils::LogLevel::kWarn, __FILE__,         \
                                                                 __LINE__, __FUNCTION__, fmt, ##__VA_ARGS__);          \
        }                                                                                                              \
    } while (0)

#define RTSP_LOGE(fmt, ...)                                                                                            \
    do {                                                                                                               \
        auto &logger = lmshao::rtsp::GetRtspLogger();                                                                  \
        if (logger.ShouldLog(lmshao::coreutils::LogLevel::kError)) {                                                   \
            logger.LogWithModuleTag<lmshao::rtsp::RtspModuleTag>(lmshao::coreutils::LogLevel::kError, __FILE__,        \
                                                                 __LINE__, __FUNCTION__, fmt, ##__VA_ARGS__);          \
        }                                                                                                              \
    } while (0)

#define RTSP_LOGF(fmt, ...)                                                                                            \
    do {                                                                                                               \
        auto &logger = lmshao::rtsp::GetRtspLogger();                                                                  \
        if (logger.ShouldLog(lmshao::coreutils::LogLevel::kFatal)) {                                                   \
            logger.LogWithModuleTag<lmshao::rtsp::RtspModuleTag>(lmshao::coreutils::LogLevel::kFatal, __FILE__,        \
                                                                 __LINE__, __FUNCTION__, fmt, ##__VA_ARGS__);          \
        }                                                                                                              \
    } while (0)

// Inline functions for RTSP logger management - now use Registry pattern
// Initialize RTSP logger with smart defaults based on build type
inline void InitRtspLogger(lmshao::coreutils::LogLevel level =
#if defined(_DEBUG) || defined(DEBUG) || !defined(NDEBUG)
                               lmshao::coreutils::LogLevel::kDebug,
#else
                               lmshao::coreutils::LogLevel::kWarn,
#endif
                           lmshao::coreutils::LogOutput output = lmshao::coreutils::LogOutput::CONSOLE,
                           const std::string &filename = "")
{
    lmshao::coreutils::LoggerRegistry::InitLogger<RtspModuleTag>(level, output, filename);
}

inline void SetRtspLogLevel(lmshao::coreutils::LogLevel level)
{
    auto &logger = lmshao::coreutils::LoggerRegistry::GetLogger<RtspModuleTag>();
    logger.SetLevel(level);
}

inline lmshao::coreutils::LogLevel GetRtspLogLevel()
{
    auto &logger = lmshao::coreutils::LoggerRegistry::GetLogger<RtspModuleTag>();
    return logger.GetLevel();
}

inline void SetRtspLogOutput(lmshao::coreutils::LogOutput output)
{
    auto &logger = lmshao::coreutils::LoggerRegistry::GetLogger<RtspModuleTag>();
    logger.SetOutput(output);
}

inline void SetRtspLogFile(const std::string &filename)
{
    auto &logger = lmshao::coreutils::LoggerRegistry::GetLogger<RtspModuleTag>();
    logger.SetOutputFile(filename);
}

// Get RTSP logger with auto-registration
inline lmshao::coreutils::Logger &GetRtspLogger()
{
    return lmshao::coreutils::LoggerRegistry::GetLoggerWithRegistration<RtspModuleTag>("RTSP");
}

} // namespace lmshao::rtsp

#endif // LMSHAO_RTSP_LOGGER_H