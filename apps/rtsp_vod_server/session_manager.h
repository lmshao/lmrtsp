/**
 * @author SHAO Liming <lmshao@163.com>
 * @copyright Copyright (c) 2025 SHAO Liming
 * @license MIT
 *
 * SPDX-License-Identifier: MIT
 */

#ifndef LMSHAO_RTSP_SESSION_MANAGER_H
#define LMSHAO_RTSP_SESSION_MANAGER_H

#include <lmrtsp/media_types.h>
#include <lmrtsp/rtsp_server_session.h>

#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>

#include "isession_worker.h"

using namespace lmshao::lmrtsp;

/**
 * @brief Manager for RTSP session worker threads
 *
 * This class manages the lifecycle of all session worker thread instances:
 * - Creates worker threads when sessions start playing
 * - Tracks active sessions and their worker threads
 * - Cleans up finished or disconnected sessions
 * - Provides session statistics and control
 */
class SessionManager {
public:
    /**
     * @brief Get singleton instance
     * @return Reference to the singleton instance
     */
    static SessionManager &GetInstance();

    /**
     * @brief Destructor - cleanup all sessions
     */
    ~SessionManager();

    /**
     * @brief Start a worker thread for a session (H264)
     * @param session RTSP session
     * @param file_path Path to the media file
     * @param frame_rate Target frame rate
     * @return true if started successfully, false otherwise
     */
    bool StartSession(std::shared_ptr<RtspServerSession> session, const std::string &file_path,
                      uint32_t frame_rate = 25);

    /**
     * @brief Start a worker thread for a session with codec type
     * @param session RTSP session
     * @param file_path Path to the media file
     * @param codec Codec type (Codec::H264, Codec::H265, Codec::MP2T, Codec::AAC, Codec::MKV)
     * @param frame_rate Target frame rate (for video) or sample rate (for audio)
     * @param bitrate Target bitrate for MP2T (bps)
     * @param track_number Track number for MKV (0 for single track)
     * @param rtsp_track_index RTSP track index for MKV multi-track (0, 1, 2...)
     * @param custom_session_id Optional custom session ID (for multi-track sessions)
     * @return true if started successfully, false otherwise
     */
    bool StartSession(std::shared_ptr<RtspServerSession> session, const std::string &file_path,
                      const std::string &codec, uint32_t frame_rate = 25, uint32_t bitrate = 2000000,
                      uint64_t track_number = 0, int rtsp_track_index = -1, const std::string &custom_session_id = "");

    /**
     * @brief Stop a session worker thread
     * @param session_id Session ID to stop
     * @return true if stopped successfully, false if not found
     */
    bool StopSession(const std::string &session_id);

    /**
     * @brief Check if a session is active
     * @param session_id Session ID to check
     * @return true if session is active, false otherwise
     */
    bool IsSessionActive(const std::string &session_id) const;

    /**
     * @brief Get session worker interface
     * @param session_id Session ID
     * @return Shared pointer to ISessionWorker, nullptr if not found
     */
    std::shared_ptr<ISessionWorker> GetWorker(const std::string &session_id) const;

    /**
     * @brief Get number of active sessions
     * @return Number of active sessions
     */
    size_t GetActiveSessionCount() const;

    /**
     * @brief Get list of active session IDs
     * @return Vector of session ID strings
     */
    std::vector<std::string> GetActiveSessionIds() const;

    /**
     * @brief Cleanup finished or invalid sessions
     * @return Number of sessions cleaned up
     */
    size_t CleanupFinishedSessions();

    /**
     * @brief Stop all sessions and cleanup
     */
    void StopAllSessions();

    /**
     * @brief Seek session to specific frame
     * @param session_id Session ID
     * @param frame_index Target frame index
     * @return true if seek successful, false otherwise
     */
    bool SeekSessionToFrame(const std::string &session_id, size_t frame_index);

    /**
     * @brief Seek session to specific time
     * @param session_id Session ID
     * @param timestamp Target timestamp in seconds
     * @return true if seek successful, false otherwise
     */
    bool SeekSessionToTime(const std::string &session_id, double timestamp);

    /**
     * @brief Reset session to beginning
     * @param session_id Session ID
     * @return true if reset successful, false otherwise
     */
    bool ResetSession(const std::string &session_id);

    /**
     * @brief Set session frame rate
     * @param session_id Session ID
     * @param fps Frames per second
     * @return true if set successful, false otherwise
     */
    bool SetSessionFrameRate(const std::string &session_id, uint32_t fps);

private:
    /**
     * @brief Private constructor for singleton
     */
    SessionManager() = default;

    /**
     * @brief Remove session from internal map (caller must hold lock)
     * @param session_id Session ID to remove
     */
    void RemoveSessionLocked(const std::string &session_id);

private:
    mutable std::mutex sessions_mutex_;
    // Unified storage for all worker types
    std::unordered_map<std::string, std::shared_ptr<ISessionWorker>> active_sessions_;

    // Statistics
    std::atomic<size_t> total_sessions_created_{0};
    std::atomic<size_t> total_sessions_finished_{0};
};

#endif // LMSHAO_RTSP_SESSION_MANAGER_H