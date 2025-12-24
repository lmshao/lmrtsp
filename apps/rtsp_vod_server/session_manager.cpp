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

#include "session_aac_worker_thread.h"
#include "session_h264_worker_thread.h"
#include "session_h265_worker_thread.h"
#include "session_mkv_worker_thread.h"
#include "session_ts_worker_thread.h"

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
    // Legacy method for H264 - delegate to new unified method
    return StartSession(session, file_path, Codec::H264, frame_rate);
}

bool SessionManager::StartSession(std::shared_ptr<RtspServerSession> session, const std::string &file_path,
                                  const std::string &codec, uint32_t frame_rate, uint32_t bitrate,
                                  uint64_t track_number, int rtsp_track_index, const std::string &custom_session_id)
{
    if (!session) {
        std::cout << "Cannot start session: invalid RtspServerSession" << std::endl;
        return false;
    }

    std::string session_id = custom_session_id.empty() ? session->GetSessionId() : custom_session_id;

    std::lock_guard<std::mutex> lock(sessions_mutex_);

    // Check if session already exists
    auto it = active_sessions_.find(session_id);
    if (it != active_sessions_.end()) {
        std::cout << "Session " << session_id << " already exists, stopping existing worker" << std::endl;
        it->second->Stop();
        active_sessions_.erase(it);
    }

    std::shared_ptr<ISessionWorker> worker;

    // Create appropriate worker based on codec type
    if (codec == Codec::H264) {
        // Use rtsp_track_index for H264 if >= 0 (multi-track mode), otherwise -1 (single-track)
        int track_index = (rtsp_track_index >= 0) ? rtsp_track_index : -1;
        auto h264_worker = std::make_shared<SessionH264WorkerThread>(session, file_path, frame_rate, track_index);
        if (!h264_worker->Start()) {
            std::cout << "Failed to start H264 worker thread for session: " << session_id << std::endl;
            return false;
        }
        worker = std::make_shared<SessionWorkerWrapper>(h264_worker);
    } else if (codec == Codec::H265) {
        auto h265_worker = std::make_shared<SessionH265WorkerThread>(session, file_path, frame_rate);
        if (!h265_worker->Start()) {
            std::cout << "Failed to start H265 worker thread for session: " << session_id << std::endl;
            return false;
        }
        worker = std::make_shared<SessionWorkerWrapper>(h265_worker);
    } else if (codec == Codec::MP2T) {
        auto ts_worker = std::make_shared<SessionTSWorkerThread>(session, file_path, bitrate);
        if (!ts_worker->Start()) {
            std::cout << "Failed to start TS worker thread for session: " << session_id << std::endl;
            return false;
        }
        worker = std::make_shared<SessionWorkerWrapper>(ts_worker);
    } else if (codec == Codec::AAC) {
        auto aac_worker = std::make_shared<SessionAacWorkerThread>(session, file_path, frame_rate);
        if (!aac_worker->Start()) {
            std::cout << "Failed to start AAC worker thread for session: " << session_id << std::endl;
            return false;
        }
        worker = std::make_shared<SessionWorkerWrapper>(aac_worker);
    } else if (codec == Codec::MKV) {
        if (rtsp_track_index < 0) {
            std::cout << "MKV worker requires rtsp_track_index" << std::endl;
            return false;
        }
        auto mkv_worker =
            std::make_shared<SessionMkvWorkerThread>(session, file_path, track_number, rtsp_track_index, frame_rate);
        if (!mkv_worker->Start()) {
            std::cout << "Failed to start MKV worker thread for session: " << session_id << std::endl;
            return false;
        }
        worker = std::make_shared<SessionWorkerWrapper>(mkv_worker);
    } else {
        std::cout << "Unsupported codec: " << codec << " for session: " << session_id << std::endl;
        return false;
    }

    // Add to active sessions
    active_sessions_[session_id] = worker;
    total_sessions_created_++;

    std::cout << "Session " << session_id << " started, codec: " << codec << ", file: " << file_path
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

std::shared_ptr<ISessionWorker> SessionManager::GetWorker(const std::string &session_id) const
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
    auto worker_wrapper = GetWorker(session_id);
    if (!worker_wrapper) {
        std::cout << "Session " << session_id << " not found for seek to frame" << std::endl;
        return false;
    }

    // Try to cast to SessionWorkerWrapper and get H264/H265 worker
    auto wrapper = std::dynamic_pointer_cast<SessionWorkerWrapper>(worker_wrapper);
    if (wrapper) {
        // Try H264 worker
        auto h264_worker = wrapper->GetWorker<SessionH264WorkerThread>();
        if (h264_worker) {
            return h264_worker->SeekToFrame(frame_index);
        }
        // Try H265 worker
        auto h265_worker = wrapper->GetWorker<SessionH265WorkerThread>();
        if (h265_worker) {
            return h265_worker->SeekToFrame(frame_index);
        }
    }

    std::cout << "Session " << session_id << " does not support seek to frame" << std::endl;
    return false;
}

bool SessionManager::SeekSessionToTime(const std::string &session_id, double timestamp)
{
    auto worker_wrapper = GetWorker(session_id);
    if (!worker_wrapper) {
        std::cout << "Session " << session_id << " not found for seek to time" << std::endl;
        return false;
    }

    // Try to cast to SessionWorkerWrapper and get H264/H265 worker
    auto wrapper = std::dynamic_pointer_cast<SessionWorkerWrapper>(worker_wrapper);
    if (wrapper) {
        // Try H264 worker
        auto h264_worker = wrapper->GetWorker<SessionH264WorkerThread>();
        if (h264_worker) {
            return h264_worker->SeekToTime(timestamp);
        }
        // Try H265 worker
        auto h265_worker = wrapper->GetWorker<SessionH265WorkerThread>();
        if (h265_worker) {
            return h265_worker->SeekToTime(timestamp);
        }
    }

    std::cout << "Session " << session_id << " does not support seek to time" << std::endl;
    return false;
}

bool SessionManager::ResetSession(const std::string &session_id)
{
    auto worker = GetWorker(session_id);
    if (worker) {
        worker->Reset();
        return true;
    }

    std::cout << "Session " << session_id << " not found for reset" << std::endl;
    return false;
}

bool SessionManager::SetSessionFrameRate(const std::string &session_id, uint32_t fps)
{
    auto worker_wrapper = GetWorker(session_id);
    if (!worker_wrapper) {
        std::cout << "Session " << session_id << " not found for setting frame rate" << std::endl;
        return false;
    }

    // Try to cast to SessionWorkerWrapper and get H264/H265 worker
    auto wrapper = std::dynamic_pointer_cast<SessionWorkerWrapper>(worker_wrapper);
    if (wrapper) {
        // Try H264 worker
        auto h264_worker = wrapper->GetWorker<SessionH264WorkerThread>();
        if (h264_worker) {
            h264_worker->SetFrameRate(fps);
            return true;
        }
        // Try H265 worker
        auto h265_worker = wrapper->GetWorker<SessionH265WorkerThread>();
        if (h265_worker) {
            h265_worker->SetFrameRate(fps);
            return true;
        }
    }

    std::cout << "Session " << session_id << " does not support setting frame rate" << std::endl;
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