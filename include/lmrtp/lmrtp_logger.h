/**
 * @author SHAO Liming <lmshao@163.com>
 * @copyright Copyright (c) 2025 SHAO Liming
 * @license MIT
 *
 * SPDX-License-Identifier: MIT
 */

#ifndef LMSHAO_LMRTP_LMRTP_LOGGER_H
#define LMSHAO_LMRTP_LMRTP_LOGGER_H

#include <lmcore/logger.h>

namespace lmshao::lmrtp {

// Module tag for Lmrtp
struct LmrtpModuleTag {};

/**
 * @brief Initialize Lmrtp logger with specified settings
 * @param level Log level (default: Debug in debug builds, Warn in release builds)
 * @param output Output destination (default: CONSOLE)
 * @param filename Log file name (optional)
 */
inline void InitLmrtpLogger(lmshao::lmcore::LogLevel level =
#if defined(_DEBUG) || defined(DEBUG) || !defined(NDEBUG)
                                lmshao::lmcore::LogLevel::kDebug,
#else
                                lmshao::lmcore::LogLevel::kWarn,
#endif
                            lmshao::lmcore::LogOutput output = lmshao::lmcore::LogOutput::CONSOLE,
                            const std::string &filename = "")
{
    // Register module if not already registered
    lmshao::lmcore::LoggerRegistry::RegisterModule<LmrtpModuleTag>("RTP");
    lmshao::lmcore::LoggerRegistry::InitLogger<LmrtpModuleTag>(level, output, filename);
}

} // namespace lmshao::lmrtp

#endif // LMSHAO_LMRTP_LMRTP_LOGGER_H
