/**
 * @author SHAO Liming <lmshao@163.com>
 * @copyright Copyright (c) 2025 SHAO Liming
 * @license MIT
 *
 * SPDX-License-Identifier: MIT
 */

#ifndef LMSHAO_LMRTSP_INTERNAL_LOGGER_H
#define LMSHAO_LMRTSP_INTERNAL_LOGGER_H

#include <lmrtsp/lmrtsp_logger.h>

#include <mutex>

namespace lmshao::lmrtsp {

/**
 * @brief Get RTSP logger with auto-initialization for internal use
 * This function ensures the logger is initialized before first use
 */
inline lmshao::lmcore::Logger &GetLmrtspLoggerWithAutoInit()
{
    static std::once_flag initFlag;
    std::call_once(initFlag, []() {
        lmshao::lmcore::LoggerRegistry::RegisterModule<LmrtspModuleTag>("RTSP");
        InitLmrtspLogger();
    });
    return lmshao::lmcore::LoggerRegistry::GetLogger<LmrtspModuleTag>();
}

// Internal RTSP logging macros with auto-initialization and module tagging
#define RTSP_LOGD(fmt, ...)                                                                                            \
    do {                                                                                                               \
        auto &logger = lmshao::lmrtsp::GetLmrtspLoggerWithAutoInit();                                                  \
        if (logger.ShouldLog(lmshao::lmcore::LogLevel::kDebug)) {                                                      \
            logger.LogWithModuleTag<lmshao::lmrtsp::LmrtspModuleTag>(lmshao::lmcore::LogLevel::kDebug, __FILE__,       \
                                                                     __LINE__, __FUNCTION__, fmt, ##__VA_ARGS__);      \
        }                                                                                                              \
    } while (0)

#define RTSP_LOGI(fmt, ...)                                                                                            \
    do {                                                                                                               \
        auto &logger = lmshao::lmrtsp::GetLmrtspLoggerWithAutoInit();                                                  \
        if (logger.ShouldLog(lmshao::lmcore::LogLevel::kInfo)) {                                                       \
            logger.LogWithModuleTag<lmshao::lmrtsp::LmrtspModuleTag>(lmshao::lmcore::LogLevel::kInfo, __FILE__,        \
                                                                     __LINE__, __FUNCTION__, fmt, ##__VA_ARGS__);      \
        }                                                                                                              \
    } while (0)

#define RTSP_LOGW(fmt, ...)                                                                                            \
    do {                                                                                                               \
        auto &logger = lmshao::lmrtsp::GetLmrtspLoggerWithAutoInit();                                                  \
        if (logger.ShouldLog(lmshao::lmcore::LogLevel::kWarn)) {                                                       \
            logger.LogWithModuleTag<lmshao::lmrtsp::LmrtspModuleTag>(lmshao::lmcore::LogLevel::kWarn, __FILE__,        \
                                                                     __LINE__, __FUNCTION__, fmt, ##__VA_ARGS__);      \
        }                                                                                                              \
    } while (0)

#define RTSP_LOGE(fmt, ...)                                                                                            \
    do {                                                                                                               \
        auto &logger = lmshao::lmrtsp::GetLmrtspLoggerWithAutoInit();                                                  \
        if (logger.ShouldLog(lmshao::lmcore::LogLevel::kError)) {                                                      \
            logger.LogWithModuleTag<lmshao::lmrtsp::LmrtspModuleTag>(lmshao::lmcore::LogLevel::kError, __FILE__,       \
                                                                     __LINE__, __FUNCTION__, fmt, ##__VA_ARGS__);      \
        }                                                                                                              \
    } while (0)

#define RTSP_LOGF(fmt, ...)                                                                                            \
    do {                                                                                                               \
        auto &logger = lmshao::lmrtsp::GetLmrtspLoggerWithAutoInit();                                                  \
        if (logger.ShouldLog(lmshao::lmcore::LogLevel::kFatal)) {                                                      \
            logger.LogWithModuleTag<lmshao::lmrtsp::LmrtspModuleTag>(lmshao::lmcore::LogLevel::kFatal, __FILE__,       \
                                                                     __LINE__, __FUNCTION__, fmt, ##__VA_ARGS__);      \
        }                                                                                                              \
    } while (0)

} // namespace lmshao::lmrtsp

#endif // LMSHAO_LMRTSP_INTERNAL_LOGGER_H
