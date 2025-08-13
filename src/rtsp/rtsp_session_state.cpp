/**
 * @author SHAO Liming <lmshao@163.com>
 * @copyright Copyright (c) 2025 SHAO Liming
 * @license MIT
 *
 * SPDX-License-Identifier: MIT
 */

#include "rtsp_session_state.h"

#include "rtsp_log.h"
#include "rtsp_session.h"

namespace lmshao::rtsp {

// 通用RTSP方法处理函数，可以被各个状态类复用
namespace {

RTSPResponse HandleOptions(RTSPSession *session, const RTSPRequest &request)
{
    RTSP_LOGD("Processing OPTIONS request");

    // OPTIONS请求在所有状态下都可以处理
    int cseq = std::stoi(request.general_header_.at("CSeq"));

    // 构建响应
    auto response = RTSPResponseBuilder()
                        .SetStatus(StatusCode::OK)
                        .SetCSeq(cseq)
                        .SetPublic("OPTIONS, DESCRIBE, SETUP, PLAY, PAUSE, TEARDOWN, GET_PARAMETER, SET_PARAMETER")
                        .Build();

    return response;
}

RTSPResponse HandleDescribe(RTSPSession *session, const RTSPRequest &request)
{
    RTSP_LOGD("Processing DESCRIBE request");

    // DESCRIBE请求在所有状态下都可以处理
    int cseq = std::stoi(request.general_header_.at("CSeq"));

    // 获取请求的URI
    std::string uri = request.uri_;

    // 这里应该根据URI获取媒体信息并生成SDP描述
    // 简化实现，使用一个示例SDP
    std::string sdp = "v=0\r\n"
                      "o=- 12345 12345 IN IP4 127.0.0.1\r\n"
                      "s=RTSP Session\r\n"
                      "t=0 0\r\n"
                      "m=video 0 RTP/AVP 96\r\n"
                      "a=rtpmap:96 H264/90000\r\n"
                      "a=control:track1\r\n"
                      "m=audio 0 RTP/AVP 97\r\n"
                      "a=rtpmap:97 MPEG4-GENERIC/44100/2\r\n"
                      "a=control:track2\r\n";

    // 保存SDP描述到会话
    session->SetSdpDescription(sdp);

    // 构建响应
    auto response = RTSPResponseBuilder()
                        .SetStatus(StatusCode::OK)
                        .SetCSeq(cseq)
                        .SetContentType("application/sdp")
                        .SetContentLength(sdp.size())
                        .SetMessageBody(sdp)
                        .Build();

    return response;
}

RTSPResponse HandleGetParameter(RTSPSession *session, const RTSPRequest &request)
{
    RTSP_LOGD("Processing GET_PARAMETER request");

    // GET_PARAMETER请求在所有状态下都可以处理
    int cseq = std::stoi(request.general_header_.at("CSeq"));

    // 构建响应
    auto response =
        RTSPResponseBuilder().SetStatus(StatusCode::OK).SetCSeq(cseq).SetSession(session->GetSessionId()).Build();

    return response;
}

RTSPResponse HandleSetParameter(RTSPSession *session, const RTSPRequest &request)
{
    RTSP_LOGD("Processing SET_PARAMETER request");

    int cseq = std::stoi(request.general_header_.at("CSeq"));

    // 简化实现，返回成功响应
    auto response = RTSPResponseBuilder().SetStatus(StatusCode::OK).SetCSeq(cseq).Build();

    return response;
}

} // namespace

// InitialState 实现
std::shared_ptr<RTSPSessionState> InitialState::GetInstance()
{
    static std::shared_ptr<InitialState> instance(new InitialState());
    return instance;
}

// GetName方法已在头文件中定义

RTSPResponse InitialState::OnOptions(RTSPSession *session, const RTSPRequest &request)
{
    return HandleOptions(session, request);
}

RTSPResponse InitialState::OnDescribe(RTSPSession *session, const RTSPRequest &request)
{
    return HandleDescribe(session, request);
}

RTSPResponse InitialState::OnGetParameter(RTSPSession *session, const RTSPRequest &request)
{
    return HandleGetParameter(session, request);
}

RTSPResponse InitialState::OnSetParameter(RTSPSession *session, const RTSPRequest &request)
{
    return HandleSetParameter(session, request);
}

RTSPResponse InitialState::OnSetup(RTSPSession *session, const RTSPRequest &request)
{
    RTSP_LOGD("Processing SETUP request in InitialState");

    int cseq = std::stoi(request.general_header_.at("CSeq"));

    // 获取Transport头
    std::string transport = "";
    for (const auto &header : request.request_header_.custom_header_) {
        if (header.find("Transport:") == 0) {
            transport = header.substr(10); // 去掉"Transport: "
            break;
        }
    }

    // 设置媒体
    if (!session->SetupMedia(request.uri_, transport)) {
        RTSP_LOGE("Failed to setup media");

        // 构建错误响应
        auto response = RTSPResponseBuilder().SetStatus(StatusCode::NotFound).SetCSeq(cseq).Build();

        return response;
    }

    // 切换到Ready状态
    session->ChangeState(ReadyState::GetInstance());

    // 构建成功响应
    auto response = RTSPResponseBuilder()
                        .SetStatus(StatusCode::OK)
                        .SetCSeq(cseq)
                        .SetSession(session->GetSessionId())
                        .SetTransport(session->GetTransportInfo())
                        .Build();

    return response;
}

RTSPResponse InitialState::OnPlay(RTSPSession *session, const RTSPRequest &request)
{
    RTSP_LOGD("Processing PLAY request in InitialState");

    int cseq = std::stoi(request.general_header_.at("CSeq"));

    // 在Initial状态下不能执行PLAY
    auto response = RTSPResponseBuilder().SetStatus(StatusCode::MethodNotValidInThisState).SetCSeq(cseq).Build();

    return response;
}

RTSPResponse InitialState::OnPause(RTSPSession *session, const RTSPRequest &request)
{
    RTSP_LOGD("Processing PAUSE request in InitialState");

    int cseq = std::stoi(request.general_header_.at("CSeq"));

    // 在Initial状态下不能执行PAUSE
    auto response = RTSPResponseBuilder().SetStatus(StatusCode::MethodNotValidInThisState).SetCSeq(cseq).Build();

    return response;
}

RTSPResponse InitialState::OnTeardown(RTSPSession *session, const RTSPRequest &request)
{
    RTSP_LOGD("Processing TEARDOWN request in InitialState");

    int cseq = std::stoi(request.general_header_.at("CSeq"));

    // 在Initial状态下不能执行TEARDOWN
    auto response = RTSPResponseBuilder().SetStatus(StatusCode::MethodNotValidInThisState).SetCSeq(cseq).Build();

    return response;
}

// ReadyState 实现
std::shared_ptr<RTSPSessionState> ReadyState::GetInstance()
{
    static std::shared_ptr<ReadyState> instance(new ReadyState());
    return instance;
}

// GetName方法已在头文件中定义

RTSPResponse ReadyState::OnOptions(RTSPSession *session, const RTSPRequest &request)
{
    return HandleOptions(session, request);
}

RTSPResponse ReadyState::OnDescribe(RTSPSession *session, const RTSPRequest &request)
{
    return HandleDescribe(session, request);
}

RTSPResponse ReadyState::OnGetParameter(RTSPSession *session, const RTSPRequest &request)
{
    return HandleGetParameter(session, request);
}

RTSPResponse ReadyState::OnSetParameter(RTSPSession *session, const RTSPRequest &request)
{
    return HandleSetParameter(session, request);
}

RTSPResponse ReadyState::OnSetup(RTSPSession *session, const RTSPRequest &request)
{
    // 在Ready状态下，可以继续设置其他媒体轨道
    return InitialState::GetInstance()->OnSetup(session, request);
}

RTSPResponse ReadyState::OnPlay(RTSPSession *session, const RTSPRequest &request)
{
    RTSP_LOGD("Processing PLAY request in ReadyState");

    int cseq = std::stoi(request.general_header_.at("CSeq"));

    // 获取Range头（如果有）
    std::string range = "";
    if (request.request_header_.range_) {
        range = *request.request_header_.range_;
    }

    // 开始播放
    if (!session->PlayMedia()) {
        RTSP_LOGE("Failed to start playing");

        // 构建错误响应
        auto response = RTSPResponseBuilder().SetStatus(StatusCode::InternalServerError).SetCSeq(cseq).Build();

        return response;
    }

    // 切换到Playing状态
    session->ChangeState(PlayingState::GetInstance());

    // 构建成功响应
    auto response = RTSPResponseBuilder()
                        .SetStatus(StatusCode::OK)
                        .SetCSeq(cseq)
                        .SetSession(session->GetSessionId())
                        .SetRange(range.empty() ? "npt=0-" : range)
                        // 这里应该添加RTP-Info头，包含RTP时间戳信息
                        .SetRTPInfo("url=rtsp://example.com/track1;seq=1000;rtptime=123456")
                        .Build();

    return response;
}

RTSPResponse ReadyState::OnPause(RTSPSession *session, const RTSPRequest &request)
{
    RTSP_LOGD("Processing PAUSE request in ReadyState");

    int cseq = std::stoi(request.general_header_.at("CSeq"));

    // 在Ready状态下不能执行PAUSE
    auto response = RTSPResponseBuilder().SetStatus(StatusCode::MethodNotValidInThisState).SetCSeq(cseq).Build();

    return response;
}

RTSPResponse ReadyState::OnTeardown(RTSPSession *session, const RTSPRequest &request)
{
    RTSP_LOGD("Processing TEARDOWN request in ReadyState");

    int cseq = std::stoi(request.general_header_.at("CSeq"));

    // 释放媒体资源
    session->TeardownMedia();

    // 切换到Initial状态
    session->ChangeState(InitialState::GetInstance());

    // 构建响应
    auto response = RTSPResponseBuilder().SetStatus(StatusCode::OK).SetCSeq(cseq).Build();

    return response;
}

// PlayingState 实现
std::shared_ptr<RTSPSessionState> PlayingState::GetInstance()
{
    static std::shared_ptr<PlayingState> instance(new PlayingState());
    return instance;
}

// GetName方法已在头文件中定义

RTSPResponse PlayingState::OnOptions(RTSPSession *session, const RTSPRequest &request)
{
    return HandleOptions(session, request);
}

RTSPResponse PlayingState::OnDescribe(RTSPSession *session, const RTSPRequest &request)
{
    return HandleDescribe(session, request);
}

RTSPResponse PlayingState::OnGetParameter(RTSPSession *session, const RTSPRequest &request)
{
    return HandleGetParameter(session, request);
}

RTSPResponse PlayingState::OnSetParameter(RTSPSession *session, const RTSPRequest &request)
{
    return HandleSetParameter(session, request);
}

RTSPResponse PlayingState::OnSetup(RTSPSession *session, const RTSPRequest &request)
{
    RTSP_LOGD("Processing SETUP request in PlayingState");

    int cseq = std::stoi(request.general_header_.at("CSeq"));

    // 在Playing状态下不能执行SETUP
    auto response = RTSPResponseBuilder().SetStatus(StatusCode::MethodNotValidInThisState).SetCSeq(cseq).Build();

    return response;
}

RTSPResponse PlayingState::OnPlay(RTSPSession *session, const RTSPRequest &request)
{
    RTSP_LOGD("Processing PLAY request in PlayingState");

    int cseq = std::stoi(request.general_header_.at("CSeq"));

    // 已经在播放中，返回OK
    auto response =
        RTSPResponseBuilder().SetStatus(StatusCode::OK).SetCSeq(cseq).SetSession(session->GetSessionId()).Build();

    return response;
}

RTSPResponse PlayingState::OnPause(RTSPSession *session, const RTSPRequest &request)
{
    RTSP_LOGD("Processing PAUSE request in PlayingState");

    int cseq = std::stoi(request.general_header_.at("CSeq"));

    // 暂停播放
    if (!session->PauseMedia()) {
        RTSP_LOGE("Failed to pause playing");

        // 构建错误响应
        auto response = RTSPResponseBuilder().SetStatus(StatusCode::InternalServerError).SetCSeq(cseq).Build();

        return response;
    }

    // 切换到Paused状态
    session->ChangeState(PausedState::GetInstance());

    // 构建成功响应
    auto response =
        RTSPResponseBuilder().SetStatus(StatusCode::OK).SetCSeq(cseq).SetSession(session->GetSessionId()).Build();

    return response;
}

RTSPResponse PlayingState::OnTeardown(RTSPSession *session, const RTSPRequest &request)
{
    RTSP_LOGD("Processing TEARDOWN request in PlayingState");

    int cseq = std::stoi(request.general_header_.at("CSeq"));

    // 释放媒体资源
    session->TeardownMedia();

    // 切换到Initial状态
    session->ChangeState(InitialState::GetInstance());

    // 构建响应
    auto response = RTSPResponseBuilder().SetStatus(StatusCode::OK).SetCSeq(cseq).Build();

    return response;
}

// PausedState 实现
std::shared_ptr<RTSPSessionState> PausedState::GetInstance()
{
    static std::shared_ptr<PausedState> instance(new PausedState());
    return instance;
}

// GetName方法已在头文件中定义

RTSPResponse PausedState::OnOptions(RTSPSession *session, const RTSPRequest &request)
{
    return HandleOptions(session, request);
}

RTSPResponse PausedState::OnDescribe(RTSPSession *session, const RTSPRequest &request)
{
    return HandleDescribe(session, request);
}

RTSPResponse PausedState::OnGetParameter(RTSPSession *session, const RTSPRequest &request)
{
    return HandleGetParameter(session, request);
}

RTSPResponse PausedState::OnSetParameter(RTSPSession *session, const RTSPRequest &request)
{
    return HandleSetParameter(session, request);
}

RTSPResponse PausedState::OnSetup(RTSPSession *session, const RTSPRequest &request)
{
    RTSP_LOGD("Processing SETUP request in PausedState");

    int cseq = std::stoi(request.general_header_.at("CSeq"));

    // 在Paused状态下不能执行SETUP
    auto response = RTSPResponseBuilder().SetStatus(StatusCode::MethodNotValidInThisState).SetCSeq(cseq).Build();

    return response;
}

RTSPResponse PausedState::OnPlay(RTSPSession *session, const RTSPRequest &request)
{
    RTSP_LOGD("Processing PLAY request in PausedState");

    int cseq = std::stoi(request.general_header_.at("CSeq"));

    // 获取Range头（如果有）
    std::string range = "";
    if (request.request_header_.range_) {
        range = *request.request_header_.range_;
    }

    // 恢复播放
    if (!session->PlayMedia(range)) {
        RTSP_LOGE("Failed to resume playing");

        // 构建错误响应
        auto response = RTSPResponseBuilder().SetStatus(StatusCode::InternalServerError).SetCSeq(cseq).Build();

        return response;
    }

    // 切换到Playing状态
    session->ChangeState(PlayingState::GetInstance());

    // 构建成功响应
    auto response = RTSPResponseBuilder()
                        .SetStatus(StatusCode::OK)
                        .SetCSeq(cseq)
                        .SetSession(session->GetSessionId())
                        .SetRange(range.empty() ? "npt=0-" : range)
                        // 这里应该添加RTP-Info头，包含RTP时间戳信息
                        .SetRTPInfo("url=rtsp://example.com/track1;seq=1000;rtptime=123456")
                        .Build();

    return response;
}

RTSPResponse PausedState::OnPause(RTSPSession *session, const RTSPRequest &request)
{
    RTSP_LOGD("Processing PAUSE request in PausedState");

    int cseq = std::stoi(request.general_header_.at("CSeq"));

    // 已经在暂停状态，返回OK
    auto response =
        RTSPResponseBuilder().SetStatus(StatusCode::OK).SetCSeq(cseq).SetSession(session->GetSessionId()).Build();

    return response;
}

RTSPResponse PausedState::OnTeardown(RTSPSession *session, const RTSPRequest &request)
{
    RTSP_LOGD("Processing TEARDOWN request in PausedState");

    int cseq = std::stoi(request.general_header_.at("CSeq"));

    // 释放媒体资源
    session->TeardownMedia();

    // 切换到Initial状态
    session->ChangeState(InitialState::GetInstance());

    // 构建响应
    auto response = RTSPResponseBuilder().SetStatus(StatusCode::OK).SetCSeq(cseq).Build();

    return response;
}

RTSPResponse RTSPSessionState::OnSetup(RTSPSession *session, const RTSPRequest &request)
{
    RTSP_LOGW("SETUP not allowed in %s state", GetName().c_str());

    int cseq = std::stoi(request.general_header_.at("CSeq"));

    // 构建错误响应
    auto response = RTSPResponseBuilder().SetStatus(StatusCode::MethodNotValidInThisState).SetCSeq(cseq).Build();

    return response;
}

RTSPResponse RTSPSessionState::OnPlay(RTSPSession *session, const RTSPRequest &request)
{
    RTSP_LOGW("PLAY not allowed in %s state", GetName().c_str());

    int cseq = std::stoi(request.general_header_.at("CSeq"));

    // 构建错误响应
    auto response = RTSPResponseBuilder().SetStatus(StatusCode::MethodNotValidInThisState).SetCSeq(cseq).Build();

    return response;
}

RTSPResponse RTSPSessionState::OnPause(RTSPSession *session, const RTSPRequest &request)
{
    RTSP_LOGW("PAUSE not allowed in %s state", GetName().c_str());

    int cseq = std::stoi(request.general_header_.at("CSeq"));

    // 构建错误响应
    auto response = RTSPResponseBuilder().SetStatus(StatusCode::MethodNotValidInThisState).SetCSeq(cseq).Build();

    return response;
}

RTSPResponse RTSPSessionState::OnTeardown(RTSPSession *session, const RTSPRequest &request)
{
    RTSP_LOGW("TEARDOWN not allowed in %s state", GetName().c_str());

    int cseq = std::stoi(request.general_header_.at("CSeq"));

    // 构建错误响应
    auto response = RTSPResponseBuilder().SetStatus(StatusCode::MethodNotValidInThisState).SetCSeq(cseq).Build();

    return response;
}

// InitialState的方法已在前面定义

// ReadyState的方法已在前面定义

// PlayingState的方法已在前面定义

// PausedState::GetInstance() 已在前面定义

// 这些方法已在前面定义
// RTSPResponse PausedState::OnPlay(RTSPSession* session, const RTSPRequest& request)
// RTSPResponse PausedState::OnTeardown(RTSPSession* session, const RTSPRequest& request)

} // namespace lmshao::rtsp