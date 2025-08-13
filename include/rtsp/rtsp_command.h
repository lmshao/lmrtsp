/**
 * @author SHAO Liming <lmshao@163.com>
 * @copyright Copyright (c) 2025 SHAO Liming
 * @license MIT
 *
 * SPDX-License-Identifier: MIT
 */

#ifndef RTSP_COMMAND_H
#define RTSP_COMMAND_H

#include <memory>

#include "rtsp_request.h"
#include "rtsp_response.h"

namespace lmshao::rtsp {

class RTSPSession;

// 命令模式基类
class RTSPCommand {
public:
    explicit RTSPCommand(const RTSPRequest &request) : request_(request) {}
    virtual ~RTSPCommand() = default;
    virtual RTSPResponse Execute(RTSPSession *session) = 0;

protected:
    RTSPRequest request_;
};

// OPTIONS命令
class OptionsCommand : public RTSPCommand {
public:
    explicit OptionsCommand(const RTSPRequest &request);
    RTSPResponse Execute(RTSPSession *session) override;
};

// DESCRIBE命令
class DescribeCommand : public RTSPCommand {
public:
    explicit DescribeCommand(const RTSPRequest &request);
    RTSPResponse Execute(RTSPSession *session) override;
};

// SETUP命令
class SetupCommand : public RTSPCommand {
public:
    explicit SetupCommand(const RTSPRequest &request);
    RTSPResponse Execute(RTSPSession *session) override;
};

// PLAY命令
class PlayCommand : public RTSPCommand {
public:
    explicit PlayCommand(const RTSPRequest &request);
    RTSPResponse Execute(RTSPSession *session) override;
};

// PAUSE命令
class PauseCommand : public RTSPCommand {
public:
    explicit PauseCommand(const RTSPRequest &request);
    RTSPResponse Execute(RTSPSession *session) override;
};

// TEARDOWN命令
class TeardownCommand : public RTSPCommand {
public:
    explicit TeardownCommand(const RTSPRequest &request);
    RTSPResponse Execute(RTSPSession *session) override;
};

// GET_PARAMETER命令
class GetParameterCommand : public RTSPCommand {
public:
    explicit GetParameterCommand(const RTSPRequest &request);
    RTSPResponse Execute(RTSPSession *session) override;
};

// SET_PARAMETER命令
class SetParameterCommand : public RTSPCommand {
public:
    explicit SetParameterCommand(const RTSPRequest &request);
    RTSPResponse Execute(RTSPSession *session) override;
};

// 命令工厂
class RTSPCommandFactory {
public:
    static std::shared_ptr<RTSPCommand> CreateCommand(const RTSPRequest &request);
};

} // namespace lmshao::rtsp

#endif // RTSP_COMMAND_H