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
inline void InitLmrtspLogger(lmcore::LogLevel level =
#if defined(_DEBUG) || defined(DEBUG) || !defined(NDEBUG)
                                 lmcore::LogLevel::kDebug,
#else
                                 lmcore::LogLevel::kWarn,
#endif
                             lmcore::LogOutput output = lmcore::LogOutput::CONSOLE, const std::string &filename = "")
{
    // Register module if not already registered
    lmcore::LoggerRegistry::RegisterModule<LmrtspModuleTag>("LMRTSP");
    lmcore::LoggerRegistry::InitLogger<LmrtspModuleTag>(level, output, filename);
}

} // namespace lmshao::lmrtsp

#endif // LMSHAO_LMRTSP_LOGGER_H