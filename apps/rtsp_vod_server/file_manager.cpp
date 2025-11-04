/**
 * @author SHAO Liming <lmshao@163.com>
 * @copyright Copyright (c) 2025 SHAO Liming
 * @license MIT
 *
 * SPDX-License-Identifier: MIT
 */

#include "file_manager.h"

#include <iostream>

FileManager &FileManager::GetInstance()
{
    static FileManager instance;
    return instance;
}

std::shared_ptr<lmshao::lmcore::MappedFile> FileManager::GetMappedFile(const std::string &file_path)
{
    std::lock_guard<std::mutex> lock(mutex_);

    // Check if file mapping already exists
    auto it = mapped_files_.find(file_path);
    if (it != mapped_files_.end()) {
        if (auto shared_file = it->second.lock()) {
            std::cout << "Reusing existing MappedFile for: " << file_path << std::endl;
            return shared_file;
        } else {
            // weak_ptr is expired, remove the entry
            mapped_files_.erase(it);
        }
    }

    // Create new MappedFile instance
    auto mapped_file = lmshao::lmcore::MappedFile::Open(file_path);
    if (!mapped_file) {
        std::cout << "Failed to open file: " << file_path << std::endl;
        return nullptr;
    }

    // Cache the weak_ptr
    mapped_files_[file_path] = mapped_file;
    std::cout << "Created new MappedFile for: " << file_path << ", size: " << mapped_file->Size() << " bytes"
              << std::endl;

    return mapped_file;
}

void FileManager::ReleaseMappedFile(const std::string &file_path)
{
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = mapped_files_.find(file_path);
    if (it != mapped_files_.end()) {
        mapped_files_.erase(it);
        std::cout << "Released MappedFile for: " << file_path << std::endl;
    }
}

size_t FileManager::GetCachedFileCount() const
{
    std::lock_guard<std::mutex> lock(mutex_);

    // Count active weak_ptrs without modifying the map
    size_t active_count = 0;
    for (const auto &pair : mapped_files_) {
        if (!pair.second.expired()) {
            ++active_count;
        }
    }

    return active_count;
}

void FileManager::ClearCache()
{
    std::lock_guard<std::mutex> lock(mutex_);
    mapped_files_.clear();
    std::cout << "Cleared all MappedFile cache" << std::endl;
}