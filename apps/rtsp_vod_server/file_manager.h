/**
 * @author SHAO Liming <lmshao@163.com>
 * @copyright Copyright (c) 2025 SHAO Liming
 * @license MIT
 *
 * SPDX-License-Identifier: MIT
 */

#ifndef LMSHAO_RTSP_FILE_MANAGER_H
#define LMSHAO_RTSP_FILE_MANAGER_H

#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>

#include "lmcore/mapped_file.h"

/**
 * @brief Global file manager for shared MappedFile instances
 *
 * This singleton class manages MappedFile instances to ensure that
 * multiple sessions can share the same file mapping efficiently.
 * Uses weak_ptr to automatically release unused file mappings.
 */
class FileManager {
public:
    /**
     * @brief Get the singleton instance
     * @return Reference to the FileManager instance
     */
    static FileManager &GetInstance();

    /**
     * @brief Get or create a MappedFile instance (thread-safe)
     * @param file_path Path to the file to map
     * @return Shared pointer to MappedFile, nullptr on failure
     */
    std::shared_ptr<lmshao::lmcore::MappedFile> GetMappedFile(const std::string &file_path);

    /**
     * @brief Release a MappedFile instance
     * @param file_path Path to the file to release
     */
    void ReleaseMappedFile(const std::string &file_path);

    /**
     * @brief Get the number of currently cached files
     * @return Number of cached MappedFile instances
     */
    size_t GetCachedFileCount() const;

    /**
     * @brief Clear all cached MappedFile instances
     */
    void ClearCache();

private:
    FileManager() = default;
    ~FileManager() = default;

    // Non-copyable and non-movable
    FileManager(const FileManager &) = delete;
    FileManager &operator=(const FileManager &) = delete;
    FileManager(FileManager &&) = delete;
    FileManager &operator=(FileManager &&) = delete;

    mutable std::mutex mutex_;
    std::unordered_map<std::string, std::weak_ptr<lmshao::lmcore::MappedFile>> mapped_files_;
};

#endif // LMSHAO_RTSP_FILE_MANAGER_H