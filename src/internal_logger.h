/**
 * @author SHAO Liming <lmshao@163.com>
 * @copyright Copyright (c) 2025 SHAO Liming
 * @license MIT
 *
 * SPDX-License-Identifier: MIT
 */

#ifndef LMSHAO_LMRTSP_INTERNAL_LOGGER_H
#define LMSHAO_LMRTSP_INTERNAL_LOGGER_H

#include <mutex>

#include "lmrtsp/lmrtsp_logger.h"

namespace lmshao::lmrtsp {

/**
 * @brief Get LMRTSP logger with automatic initialization
 */
inline lmcore::Logger &GetLmrtspLoggerWithAutoInit()
{
    static std::once_flag initFlag;
    std::call_once(initFlag, []() { lmcore::LoggerRegistry::RegisterModule<LmrtspModuleTag>("LMRTSP"); });
    return lmcore::LoggerRegistry::GetLogger<LmrtspModuleTag>();
}

// Internal LMRTSP logging macros with auto-initialization and module tagging
#define LMRTSP_LOGD(fmt, ...)                                                                                          \
    do {                                                                                                               \
        thread_local auto &logger = lmshao::lmrtsp::GetLmrtspLoggerWithAutoInit();                                     \
        if (logger.ShouldLog(lmcore::LogLevel::kDebug)) {                                                              \
            logger.LogWithModuleTag<lmshao::lmrtsp::LmrtspModuleTag>(lmcore::LogLevel::kDebug, __FILE__, __LINE__,     \
                                                                     __FUNCTION__, fmt, ##__VA_ARGS__);                \
        }                                                                                                              \
    } while (0)

#define LMRTSP_LOGI(fmt, ...)                                                                                          \
    do {                                                                                                               \
        thread_local auto &logger = lmshao::lmrtsp::GetLmrtspLoggerWithAutoInit();                                     \
        if (logger.ShouldLog(lmcore::LogLevel::kInfo)) {                                                               \
            logger.LogWithModuleTag<lmshao::lmrtsp::LmrtspModuleTag>(lmcore::LogLevel::kInfo, __FILE__, __LINE__,      \
                                                                     __FUNCTION__, fmt, ##__VA_ARGS__);                \
        }                                                                                                              \
    } while (0)

#define LMRTSP_LOGW(fmt, ...)                                                                                          \
    do {                                                                                                               \
        thread_local auto &logger = lmshao::lmrtsp::GetLmrtspLoggerWithAutoInit();                                     \
        if (logger.ShouldLog(lmcore::LogLevel::kWarn)) {                                                               \
            logger.LogWithModuleTag<lmshao::lmrtsp::LmrtspModuleTag>(lmcore::LogLevel::kWarn, __FILE__, __LINE__,      \
                                                                     __FUNCTION__, fmt, ##__VA_ARGS__);                \
        }                                                                                                              \
    } while (0)

#define LMRTSP_LOGE(fmt, ...)                                                                                          \
    do {                                                                                                               \
        thread_local auto &logger = lmshao::lmrtsp::GetLmrtspLoggerWithAutoInit();                                     \
        if (logger.ShouldLog(lmcore::LogLevel::kError)) {                                                              \
            logger.LogWithModuleTag<lmshao::lmrtsp::LmrtspModuleTag>(lmcore::LogLevel::kError, __FILE__, __LINE__,     \
                                                                     __FUNCTION__, fmt, ##__VA_ARGS__);                \
        }                                                                                                              \
    } while (0)

#define LMRTSP_LOGF(fmt, ...)                                                                                          \
    do {                                                                                                               \
        thread_local auto &logger = lmshao::lmrtsp::GetLmrtspLoggerWithAutoInit();                                     \
        if (logger.ShouldLog(lmcore::LogLevel::kFatal)) {                                                              \
            logger.LogWithModuleTag<lmshao::lmrtsp::LmrtspModuleTag>(lmcore::LogLevel::kFatal, __FILE__, __LINE__,     \
                                                                     __FUNCTION__, fmt, ##__VA_ARGS__);                \
        }                                                                                                              \
    } while (0)

} // namespace lmshao::lmrtsp

#endif // LMSHAO_LMRTSP_INTERNAL_LOGGER_H
