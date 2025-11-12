/**
 * @author SHAO Liming <lmshao@163.com>
 * @copyright Copyright (c) 2025 SHAO Liming
 * @license MIT
 *
 * SPDX-License-Identifier: MIT
 */

#ifndef LMSHAO_RTSP_ISESSION_WORKER_H
#define LMSHAO_RTSP_ISESSION_WORKER_H

#include <lmrtsp/rtsp_server_session.h>

#include <functional>
#include <memory>
#include <string>

using namespace lmshao::lmrtsp;

/**
 * @brief Base interface for all session worker threads
 *
 * This interface provides a common abstraction for different worker types:
 * - H264 (SessionH264WorkerThread)
 * - H265 (SessionH265WorkerThread)
 * - MP2T (SessionTSWorkerThread)
 * - AAC (SessionAacWorkerThread)
 * - MKV (SessionMkvWorkerThread)
 */
class ISessionWorker {
public:
    virtual ~ISessionWorker() = default;

    /**
     * @brief Start the worker thread
     * @return true if started successfully, false otherwise
     */
    virtual bool Start() = 0;

    /**
     * @brief Stop the worker thread
     */
    virtual void Stop() = 0;

    /**
     * @brief Check if the worker thread is running
     * @return true if running, false otherwise
     */
    virtual bool IsRunning() const = 0;

    /**
     * @brief Get session ID for identification
     * @return Session ID string
     */
    virtual std::string GetSessionId() const = 0;

    /**
     * @brief Reset playback to beginning
     */
    virtual void Reset() = 0;
};

/**
 * @brief Type-erased wrapper for different worker thread types
 *
 * This wrapper allows SessionManager to store different worker types
 * in a unified container without requiring a common base class.
 */
class SessionWorkerWrapper : public ISessionWorker {
public:
    template <typename WorkerType>
    SessionWorkerWrapper(std::shared_ptr<WorkerType> worker)
        : worker_(worker), start_fn_([worker]() { return worker->Start(); }), stop_fn_([worker]() { worker->Stop(); }),
          is_running_fn_([worker]() { return worker->IsRunning(); }),
          get_session_id_fn_([worker]() { return worker->GetSessionId(); }), reset_fn_([worker]() { worker->Reset(); })
    {
    }

    bool Start() override { return start_fn_(); }
    void Stop() override { stop_fn_(); }
    bool IsRunning() const override { return is_running_fn_(); }
    std::string GetSessionId() const override { return get_session_id_fn_(); }
    void Reset() override { reset_fn_(); }

    template <typename WorkerType>
    std::shared_ptr<WorkerType> GetWorker() const
    {
        return std::static_pointer_cast<WorkerType>(worker_);
    }

private:
    std::shared_ptr<void> worker_;
    std::function<bool()> start_fn_;
    std::function<void()> stop_fn_;
    std::function<bool()> is_running_fn_;
    std::function<std::string()> get_session_id_fn_;
    std::function<void()> reset_fn_;
};

#endif // LMSHAO_RTSP_ISESSION_WORKER_H
