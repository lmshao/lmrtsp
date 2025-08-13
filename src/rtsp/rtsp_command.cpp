/**
 * @author SHAO Liming <lmshao@163.com>
 * @copyright Copyright (c) 2025 SHAO Liming
 * @license MIT
 *
 * SPDX-License-Identifier: MIT
 */

#include "rtsp_command.h"

#include "rtsp_log.h"
#include "rtsp_session.h"

namespace lmshao::rtsp {

// RTSPCommand base class implementation is already defined in header file

// OptionsCommand 实现
OptionsCommand::OptionsCommand(const RTSPRequest &request) : RTSPCommand(request) {}

RTSPResponse OptionsCommand::Execute(RTSPSession *session)
{
    RTSP_LOGD("Executing OPTIONS command");
    return session->ProcessRequest(request_);
}

// DescribeCommand 实现
DescribeCommand::DescribeCommand(const RTSPRequest &request) : RTSPCommand(request) {}

RTSPResponse DescribeCommand::Execute(RTSPSession *session)
{
    RTSP_LOGD("Executing DESCRIBE command");
    return session->ProcessRequest(request_);
}

// SetupCommand 实现
SetupCommand::SetupCommand(const RTSPRequest &request) : RTSPCommand(request) {}

RTSPResponse SetupCommand::Execute(RTSPSession *session)
{
    RTSP_LOGD("Executing SETUP command");
    return session->ProcessRequest(request_);
}

// PlayCommand 实现
PlayCommand::PlayCommand(const RTSPRequest &request) : RTSPCommand(request) {}

RTSPResponse PlayCommand::Execute(RTSPSession *session)
{
    RTSP_LOGD("Executing PLAY command");
    return session->ProcessRequest(request_);
}

// PauseCommand 实现
PauseCommand::PauseCommand(const RTSPRequest &request) : RTSPCommand(request) {}

RTSPResponse PauseCommand::Execute(RTSPSession *session)
{
    RTSP_LOGD("Executing PAUSE command");
    return session->ProcessRequest(request_);
}

// TeardownCommand 实现
TeardownCommand::TeardownCommand(const RTSPRequest &request) : RTSPCommand(request) {}

RTSPResponse TeardownCommand::Execute(RTSPSession *session)
{
    RTSP_LOGD("Executing TEARDOWN command");
    return session->ProcessRequest(request_);
}

// GetParameterCommand 实现
GetParameterCommand::GetParameterCommand(const RTSPRequest &request) : RTSPCommand(request) {}

RTSPResponse GetParameterCommand::Execute(RTSPSession *session)
{
    RTSP_LOGD("Executing GET_PARAMETER command");
    return session->ProcessRequest(request_);
}

// SetParameterCommand 实现
SetParameterCommand::SetParameterCommand(const RTSPRequest &request) : RTSPCommand(request) {}

RTSPResponse SetParameterCommand::Execute(RTSPSession *session)
{
    RTSP_LOGD("Executing SET_PARAMETER command");
    return session->ProcessRequest(request_);
}

// RTSPCommandFactory 实现
std::shared_ptr<RTSPCommand> RTSPCommandFactory::CreateCommand(const RTSPRequest &request)
{
    if (request.method_ == "OPTIONS") {
        return std::make_shared<OptionsCommand>(request);
    } else if (request.method_ == "DESCRIBE") {
        return std::make_shared<DescribeCommand>(request);
    } else if (request.method_ == "SETUP") {
        return std::make_shared<SetupCommand>(request);
    } else if (request.method_ == "PLAY") {
        return std::make_shared<PlayCommand>(request);
    } else if (request.method_ == "PAUSE") {
        return std::make_shared<PauseCommand>(request);
    } else if (request.method_ == "TEARDOWN") {
        return std::make_shared<TeardownCommand>(request);
    } else if (request.method_ == "GET_PARAMETER") {
        return std::make_shared<GetParameterCommand>(request);
    } else if (request.method_ == "SET_PARAMETER") {
        return std::make_shared<SetParameterCommand>(request);
    } else {
        RTSP_LOGW("Unknown RTSP method: %s", request.method_.c_str());
        return nullptr;
    }
}

// RTSPResponseFactory 实现在 rtsp_response.cpp 中

} // namespace lmshao::rtsp