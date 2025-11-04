/**
 * @author SHAO Liming <lmshao@163.com>
 * @copyright Copyright (c) 2025 SHAO Liming
 * @license MIT
 *
 * SPDX-License-Identifier: MIT
 */

#include "session_manager.h"

#include <algorithm>
#include <iostream>

SessionManager &SessionManager::GetInstance()
{
    static SessionManager instance;
    return instance;
}

SessionManager::~SessionManager()
{
    StopAllSessions();
    std::cout << "SessionManager destroyed, total sessions created: " << total_sessions_created_.load()
              << ", finished: " << total_sessions_finished_.load() << std::endl;
}

bool SessionManager::StartSession(std::shared_ptr<RtspServerSession> session, const std::string &file_path,
                                  uint32_t frame_rate)
{
    if (!session) {
        std::cout << "Cannot start session: invalid RtspServerSession" << std::endl;
        return false;
    }

    std::string session_id = session->GetSessionId();

    std::lock_guard<std::mutex> lock(sessions_mutex_);

    // Check if session already exists
    auto it = active_sessions_.find(session_id);
    if (it != active_sessions_.end()) {
        std::cout << "Session " << session_id << " already exists, stopping existing worker" << std::endl;
        it->second->Stop();
        active_sessions_.erase(it);
    }

    // Create new worker thread
    auto worker = std::make_shared<SessionWorkerThread>(session, file_path, frame_rate);

    if (!worker->Start()) {
        std::cout << "Failed to start worker thread for session: " << session_id << std::endl;
        return false;
    }

    // Add to active sessions
    active_sessions_[session_id] = worker;
    total_sessions_created_++;

    std::cout << "Session " << session_id << " started, file: " << file_path << ", fps: " << frame_rate
              << ", total active: " << active_sessions_.size() << std::endl;

    return true;
}

bool SessionManager::StopSession(const std::string &session_id)
{
    std::lock_guard<std::mutex> lock(sessions_mutex_);

    auto it = active_sessions_.find(session_id);
    if (it == active_sessions_.end()) {
        std::cout << "Session " << session_id << " not found for stopping" << std::endl;
        return false;
    }

    std::cout << "Stopping session: " << session_id << std::endl;

    it->second->Stop();
    active_sessions_.erase(it);
    total_sessions_finished_++;

    std::cout << "Session " << session_id << " stopped, remaining active: " << active_sessions_.size() << std::endl;

    return true;
}

bool SessionManager::IsSessionActive(const std::string &session_id) const
{
    std::lock_guard<std::mutex> lock(sessions_mutex_);

    auto it = active_sessions_.find(session_id);
    if (it == active_sessions_.end()) {
        return false;
    }

    return it->second->IsRunning();
}

std::shared_ptr<SessionWorkerThread> SessionManager::GetSessionWorker(const std::string &session_id) const
{
    std::lock_guard<std::mutex> lock(sessions_mutex_);

    auto it = active_sessions_.find(session_id);
    if (it != active_sessions_.end()) {
        return it->second;
    }

    return nullptr;
}

size_t SessionManager::GetActiveSessionCount() const
{
    std::lock_guard<std::mutex> lock(sessions_mutex_);
    return active_sessions_.size();
}

std::vector<std::string> SessionManager::GetActiveSessionIds() const
{
    std::lock_guard<std::mutex> lock(sessions_mutex_);

    std::vector<std::string> session_ids;
    session_ids.reserve(active_sessions_.size());

    for (const auto &pair : active_sessions_) {
        session_ids.push_back(pair.first);
    }

    return session_ids;
}

size_t SessionManager::CleanupFinishedSessions()
{
    std::lock_guard<std::mutex> lock(sessions_mutex_);

    size_t cleaned_count = 0;
    auto it = active_sessions_.begin();

    while (it != active_sessions_.end()) {
        if (!it->second->IsRunning()) {
            std::cout << "Cleaning up finished session: " << it->first << std::endl;
            it->second->Stop(); // Ensure proper cleanup
            it = active_sessions_.erase(it);
            total_sessions_finished_++;
            cleaned_count++;
        } else {
            ++it;
        }
    }

    if (cleaned_count > 0) {
        std::cout << "Cleaned up " << cleaned_count
                  << " finished sessions, remaining active: " << active_sessions_.size() << std::endl;
    }

    return cleaned_count;
}

void SessionManager::StopAllSessions()
{
    std::lock_guard<std::mutex> lock(sessions_mutex_);

    std::cout << "Stopping all " << active_sessions_.size() << " active sessions" << std::endl;

    for (auto &pair : active_sessions_) {
        std::cout << "Stopping session: " << pair.first << std::endl;
        pair.second->Stop();
    }

    total_sessions_finished_ += active_sessions_.size();
    active_sessions_.clear();

    std::cout << "All sessions stopped" << std::endl;
}

bool SessionManager::SeekSessionToFrame(const std::string &session_id, size_t frame_index)
{
    auto worker = GetSessionWorker(session_id);
    if (worker) {
        return worker->SeekToFrame(frame_index);
    }

    std::cout << "Session " << session_id << " not found for seek to frame" << std::endl;
    return false;
}

bool SessionManager::SeekSessionToTime(const std::string &session_id, double timestamp)
{
    auto worker = GetSessionWorker(session_id);
    if (worker) {
        return worker->SeekToTime(timestamp);
    }

    std::cout << "Session " << session_id << " not found for seek to time" << std::endl;
    return false;
}

bool SessionManager::ResetSession(const std::string &session_id)
{
    auto worker = GetSessionWorker(session_id);
    if (worker) {
        worker->Reset();
        return true;
    }

    std::cout << "Session " << session_id << " not found for reset" << std::endl;
    return false;
}

bool SessionManager::SetSessionFrameRate(const std::string &session_id, uint32_t fps)
{
    auto worker = GetSessionWorker(session_id);
    if (worker) {
        worker->SetFrameRate(fps);
        return true;
    }

    std::cout << "Session " << session_id << " not found for setting frame rate" << std::endl;
    return false;
}

void SessionManager::RemoveSessionLocked(const std::string &session_id)
{
    auto it = active_sessions_.find(session_id);
    if (it != active_sessions_.end()) {
        it->second->Stop();
        active_sessions_.erase(it);
        total_sessions_finished_++;
    }
}