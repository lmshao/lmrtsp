/**
 * @author SHAO Liming <lmshao@163.com>
 * @copyright Copyright (c) 2025 SHAO Liming
 * @license MIT
 *
 * SPDX-License-Identifier: MIT
 */

#ifndef RTSP_REQUEST_H
#define RTSP_REQUEST_H

#include <map>
#include <optional>
#include <string>
#include <vector>

#include "rtsp_headers.h"

namespace lmshao::rtsp {

// RTSP Request Header class
class RequestHeader {
public:
    RequestHeader() = default;
    std::string ToString() const;
    static RequestHeader FromString(const std::string &header_str);

public:
    std::optional<std::string> accept_;
    std::optional<std::string> accept_encoding_;
    std::optional<std::string> accept_language_;
    std::optional<std::string> authorization_;
    std::optional<std::string> from_;
    std::optional<std::string> if_modified_since_;
    std::optional<std::string> range_;
    std::optional<std::string> referer_;
    std::optional<std::string> user_agent_;
    std::vector<std::string> custom_header_;
};

// RTSP Request class
class RTSPRequest {
public:
    RTSPRequest() : version_(RTSP_VERSION) {}
    std::string ToString() const;
    static RTSPRequest FromString(const std::string &req_str);

    std::map<std::string, std::string> entity_header_;
    std::optional<std::string> message_body_;

public:
    std::string method_;
    std::string uri_;
    std::string version_;
    std::map<std::string, std::string> general_header_;
    RequestHeader request_header_;
};

// Builder class for constructing RTSP requests
class RTSPRequestBuilder {
public:
    RTSPRequestBuilder();

    // Method setting
    RTSPRequestBuilder &SetMethod(const std::string &method);
    RTSPRequestBuilder &SetUri(const std::string &uri);
    RTSPRequestBuilder &SetCSeq(int cseq);

    // General headers
    RTSPRequestBuilder &SetSession(const std::string &session);
    RTSPRequestBuilder &SetTransport(const std::string &transport);
    RTSPRequestBuilder &SetRange(const std::string &range);
    RTSPRequestBuilder &SetLocation(const std::string &location);
    RTSPRequestBuilder &SetRequire(const std::string &require);
    RTSPRequestBuilder &SetProxyRequire(const std::string &proxy_require);

    // Request headers
    RTSPRequestBuilder &SetAccept(const std::string &accept);
    RTSPRequestBuilder &SetUserAgent(const std::string &user_agent);
    RTSPRequestBuilder &SetAuthorization(const std::string &authorization);
    RTSPRequestBuilder &AddCustomHeader(const std::string &header);

    // Entity headers
    RTSPRequestBuilder &SetContentType(const std::string &content_type);
    RTSPRequestBuilder &SetContentLength(size_t length);

    // Message body
    RTSPRequestBuilder &SetMessageBody(const std::string &body);
    RTSPRequestBuilder &SetSdp(const std::string &sdp);
    RTSPRequestBuilder &SetParameters(const std::vector<std::string> &params);
    RTSPRequestBuilder &SetParameters(const std::vector<std::pair<std::string, std::string>> &params);

    // Build the final request
    RTSPRequest Build() const;

private:
    RTSPRequest request_;
};

// Factory methods for common request types
class RTSPRequestFactory {
public:
    static RTSPRequestBuilder CreateOptions(int cseq, const std::string &uri = "*");
    static RTSPRequestBuilder CreateDescribe(int cseq, const std::string &uri);
    static RTSPRequestBuilder CreateAnnounce(int cseq, const std::string &uri);
    static RTSPRequestBuilder CreateSetup(int cseq, const std::string &uri);
    static RTSPRequestBuilder CreatePlay(int cseq, const std::string &uri);
    static RTSPRequestBuilder CreatePause(int cseq, const std::string &uri);
    static RTSPRequestBuilder CreateTeardown(int cseq, const std::string &uri);
    static RTSPRequestBuilder CreateGetParameter(int cseq, const std::string &uri);
    static RTSPRequestBuilder CreateSetParameter(int cseq, const std::string &uri);
    static RTSPRequestBuilder CreateRedirect(int cseq, const std::string &uri);
    static RTSPRequestBuilder CreateRecord(int cseq, const std::string &uri);
};

} // namespace lmshao::rtsp

#endif // RTSP_REQUEST_H
