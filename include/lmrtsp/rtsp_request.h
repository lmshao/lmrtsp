/**
 * @author SHAO Liming <lmshao@163.com>
 * @copyright Copyright (c) 2025 SHAO Liming
 * @license MIT
 *
 * SPDX-License-Identifier: MIT
 */

#ifndef LMSHAO_LMRTSP_RTSP_REQUEST_H
#define LMSHAO_LMRTSP_RTSP_REQUEST_H

#include <map>
#include <optional>
#include <string>
#include <vector>

#include "lmrtsp/rtsp_headers.h"

namespace lmshao::lmrtsp {

// RTSP Request Header class
class RequestHeader {
public:
    RequestHeader() = default;
    std::string ToString() const;
    static RequestHeader FromString(const std::string &header_str);

public:
    std::optional<std::string> accept_;
    std::optional<std::string> acceptEncoding_;
    std::optional<std::string> acceptLanguage_;
    std::optional<std::string> authorization_;
    std::optional<std::string> from_;
    std::optional<std::string> ifModifiedSince_;
    std::optional<std::string> range_;
    std::optional<std::string> referer_;
    std::optional<std::string> userAgent_;
    std::vector<std::string> customHeader_;
};

// RTSP Request class
class RtspRequest {
public:
    RtspRequest() : version_(RTSP_VERSION) {}
    std::string ToString() const;
    static RtspRequest FromString(const std::string &req_str);

    std::map<std::string, std::string> entity_header_;
    std::optional<std::string> messageBody_;

public:
    std::string method_;
    std::string uri_;
    std::string version_;
    std::map<std::string, std::string> general_header_;
    RequestHeader requestHeader_;
};

// Builder class for constructing RTSP requests
class RtspRequestBuilder {
public:
    RtspRequestBuilder();

    // Method setting
    RtspRequestBuilder &SetMethod(const std::string &method);
    RtspRequestBuilder &SetUri(const std::string &uri);
    RtspRequestBuilder &SetCSeq(int cseq);

    // General headers
    RtspRequestBuilder &SetSession(const std::string &session);
    RtspRequestBuilder &SetTransport(const std::string &transport);
    RtspRequestBuilder &SetRange(const std::string &range);
    RtspRequestBuilder &SetLocation(const std::string &location);
    RtspRequestBuilder &SetRequire(const std::string &require);
    RtspRequestBuilder &SetProxyRequire(const std::string &proxy_require);

    // Request headers
    RtspRequestBuilder &SetAccept(const std::string &accept);
    RtspRequestBuilder &SetUserAgent(const std::string &user_agent);
    RtspRequestBuilder &SetAuthorization(const std::string &authorization);
    RtspRequestBuilder &AddCustomHeader(const std::string &header);

    // Entity headers
    RtspRequestBuilder &SetContentType(const std::string &content_type);
    RtspRequestBuilder &SetContentLength(size_t length);

    // Message body
    RtspRequestBuilder &SetMessageBody(const std::string &body);
    RtspRequestBuilder &SetSdp(const std::string &sdp);
    RtspRequestBuilder &SetParameters(const std::vector<std::string> &params);
    RtspRequestBuilder &SetParameters(const std::vector<std::pair<std::string, std::string>> &params);

    // Build the final request
    RtspRequest Build() const;

private:
    RtspRequest request_;
};

// Factory methods for common request types
class RtspRequestFactory {
public:
    static RtspRequestBuilder CreateOptions(int cseq, const std::string &uri = "*");
    static RtspRequestBuilder CreateDescribe(int cseq, const std::string &uri);
    static RtspRequestBuilder CreateAnnounce(int cseq, const std::string &uri);
    static RtspRequestBuilder CreateSetup(int cseq, const std::string &uri);
    static RtspRequestBuilder CreatePlay(int cseq, const std::string &uri);
    static RtspRequestBuilder CreatePause(int cseq, const std::string &uri);
    static RtspRequestBuilder CreateTeardown(int cseq, const std::string &uri);
    static RtspRequestBuilder CreateGetParameter(int cseq, const std::string &uri);
    static RtspRequestBuilder CreateSetParameter(int cseq, const std::string &uri);
    static RtspRequestBuilder CreateRedirect(int cseq, const std::string &uri);
    static RtspRequestBuilder CreateRecord(int cseq, const std::string &uri);
};

} // namespace lmshao::lmrtsp

#endif // LMSHAO_LMRTSP_RTSP_REQUEST_H