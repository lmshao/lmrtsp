/**
 * @author SHAO Liming <lmshao@163.com>
 * @copyright Copyright (c) 2025 SHAO Liming
 * @license MIT
 *
 * SPDX-License-Identifier: MIT
 */

#ifndef LMSHAO_LMRTSP_LMRTSP_LOGGER_H
#define LMSHAO_LMRTSP_LMRTSP_LOGGER_H

#include <lmcore/logger.h>

namespace lmshao::lmrtsp {

// Module tag for Lmrtsp
struct LmrtspModuleTag {};

/**
 * @brief Initialize Lmrtsp logger with specified settings
 * @param level Log level (default: Debug in debug builds, Warn in release builds)
 * @param output Output destination (default: CONSOLE)
 * @param filename Log file name (optional)
 */
inline void InitLmrtspLogger(lmshao::lmcore::LogLevel level =
#if defined(_DEBUG) || defined(DEBUG) || !defined(NDEBUG)
                                 lmshao::lmcore::LogLevel::kDebug,
#else
                                 lmshao::lmcore::LogLevel::kWarn,
#endif
                             lmshao::lmcore::LogOutput output = lmshao::lmcore::LogOutput::CONSOLE,
                             const std::string &filename = "")
{
    // Register module if not already registered
    lmshao::lmcore::LoggerRegistry::RegisterModule<LmrtspModuleTag>("RTSP");
    lmshao::lmcore::LoggerRegistry::InitLogger<LmrtspModuleTag>(level, output, filename);
}

} // namespace lmshao::lmrtsp

#endif // LMSHAO_LMRTSP_LOGGER_H