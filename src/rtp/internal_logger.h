/**
 * @author SHAO Liming <lmshao@163.com>
 * @copyright Copyright (c) 2025 SHAO Liming
 * @license MIT
 *
 * SPDX-License-Identifier: MIT
 */

#ifndef LMSHAO_LMRTP_INTERNAL_LOGGER_H
#define LMSHAO_LMRTP_INTERNAL_LOGGER_H

#include <lmrtp/lmrtp_logger.h>

#include <mutex>

namespace lmshao::lmrtp {

/**
 * @brief Get RTP logger with auto-initialization for internal use
 * This function ensures the logger is initialized before first use
 */
inline lmshao::lmcore::Logger &GetLmrtpLoggerWithAutoInit()
{
    static std::once_flag initFlag;
    std::call_once(initFlag, []() {
        lmshao::lmcore::LoggerRegistry::RegisterModule<LmrtpModuleTag>("RTP");
        InitLmrtpLogger();
    });
    return lmshao::lmcore::LoggerRegistry::GetLogger<LmrtpModuleTag>();
}

// Internal RTP logging macros with auto-initialization and module tagging
#define RTP_LOGD(fmt, ...)                                                                                             \
    do {                                                                                                               \
        auto &logger = lmshao::lmrtp::GetLmrtpLoggerWithAutoInit();                                                    \
        if (logger.ShouldLog(lmshao::lmcore::LogLevel::kDebug)) {                                                      \
            logger.LogWithModuleTag<lmshao::lmrtp::LmrtpModuleTag>(lmshao::lmcore::LogLevel::kDebug, __FILE__,         \
                                                                   __LINE__, __FUNCTION__, fmt, ##__VA_ARGS__);        \
        }                                                                                                              \
    } while (0)

#define RTP_LOGI(fmt, ...)                                                                                             \
    do {                                                                                                               \
        auto &logger = lmshao::lmrtp::GetLmrtpLoggerWithAutoInit();                                                    \
        if (logger.ShouldLog(lmshao::lmcore::LogLevel::kInfo)) {                                                       \
            logger.LogWithModuleTag<lmshao::lmrtp::LmrtpModuleTag>(lmshao::lmcore::LogLevel::kInfo, __FILE__,          \
                                                                   __LINE__, __FUNCTION__, fmt, ##__VA_ARGS__);        \
        }                                                                                                              \
    } while (0)

#define RTP_LOGW(fmt, ...)                                                                                             \
    do {                                                                                                               \
        auto &logger = lmshao::lmrtp::GetLmrtpLoggerWithAutoInit();                                                    \
        if (logger.ShouldLog(lmshao::lmcore::LogLevel::kWarn)) {                                                       \
            logger.LogWithModuleTag<lmshao::lmrtp::LmrtpModuleTag>(lmshao::lmcore::LogLevel::kWarn, __FILE__,          \
                                                                   __LINE__, __FUNCTION__, fmt, ##__VA_ARGS__);        \
        }                                                                                                              \
    } while (0)

#define RTP_LOGE(fmt, ...)                                                                                             \
    do {                                                                                                               \
        auto &logger = lmshao::lmrtp::GetLmrtpLoggerWithAutoInit();                                                    \
        if (logger.ShouldLog(lmshao::lmcore::LogLevel::kError)) {                                                      \
            logger.LogWithModuleTag<lmshao::lmrtp::LmrtpModuleTag>(lmshao::lmcore::LogLevel::kError, __FILE__,         \
                                                                   __LINE__, __FUNCTION__, fmt, ##__VA_ARGS__);        \
        }                                                                                                              \
    } while (0)

#define RTP_LOGF(fmt, ...)                                                                                             \
    do {                                                                                                               \
        auto &logger = lmshao::lmrtp::GetLmrtpLoggerWithAutoInit();                                                    \
        if (logger.ShouldLog(lmshao::lmcore::LogLevel::kFatal)) {                                                      \
            logger.LogWithModuleTag<lmshao::lmrtp::LmrtpModuleTag>(lmshao::lmcore::LogLevel::kFatal, __FILE__,         \
                                                                   __LINE__, __FUNCTION__, fmt, ##__VA_ARGS__);        \
        }                                                                                                              \
    } while (0)

} // namespace lmshao::lmrtp

#endif // LMSHAO_LMRTP_INTERNAL_LOGGER_H
