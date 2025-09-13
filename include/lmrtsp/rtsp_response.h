/**
 * @author SHAO Liming <lmshao@163.com>
 * @copyright Copyright (c) 2025 SHAO Liming
 * @license MIT
 *
 * SPDX-License-Identifier: MIT
 */

#ifndef LMSHAO_LMRTSP_RTSP_RESPONSE_H
#define LMSHAO_LMRTSP_RTSP_RESPONSE_H

#include <cstdint>
#include <map>
#include <optional>
#include <string>
#include <vector>

#include "rtsp_headers.h"

namespace lmshao::lmrtsp {

// RTSP Status Code enumeration
enum class StatusCode : uint16_t {
    // 1xx Informational
    Continue = 100,

    // 2xx Success
    OK = 200,
    Created = 201,
    LowOnStorageSpace = 250,

    // 3xx Redirection
    MultipleChoices = 300,
    MovedPermanently = 301,
    MovedTemporarily = 302,
    SeeOther = 303,
    NotModified = 304,
    UseProxy = 305,

    // 4xx Client Error
    BadRequest = 400,
    Unauthorized = 401,
    PaymentRequired = 402,
    Forbidden = 403,
    NotFound = 404,
    MethodNotAllowed = 405,
    NotAcceptable = 406,
    ProxyAuthenticationRequired = 407,
    RequestTimeout = 408,
    Gone = 410,
    LengthRequired = 411,
    PreconditionFailed = 412,
    RequestEntityTooLarge = 413,
    RequestURITooLarge = 414,
    UnsupportedMediaType = 415,
    ParameterNotUnderstood = 451,
    ConferenceNotFound = 452,
    NotEnoughBandwidth = 453,
    SessionNotFound = 454,
    MethodNotValidInThisState = 455,
    HeaderFieldNotValidForResource = 456,
    InvalidRange = 457,
    ParameterIsReadOnly = 458,
    AggregateOperationNotAllowed = 459,
    OnlyAggregateOperationAllowed = 460,
    UnsupportedTransport = 461,
    DestinationUnreachable = 462,

    // 5xx Server Error
    InternalServerError = 500,
    NotImplemented = 501,
    BadGateway = 502,
    ServiceUnavailable = 503,
    GatewayTimeout = 504,
    RTSPVersionNotSupported = 505,
    OptionNotSupported = 551
};

// Helper function to get reason phrase for status code
std::string GetReasonPhrase(StatusCode code);

// RTSP Response Header class
class ResponseHeader {
public:
    ResponseHeader() = default;
    std::string ToString() const;
    static ResponseHeader FromString(const std::string &header_str);

public:
    std::optional<std::string> location_;
    std::optional<std::string> proxy_authenticate_;
    std::vector<std::string> public_methods_;
    std::optional<std::string> retry_after_;
    std::optional<std::string> server_;
    std::optional<std::string> vary_;
    std::optional<std::string> www_authenticate_;
    std::optional<std::string> rtp_info_;
    std::vector<std::string> custom_header_;
};

// RTSP Response class
class RTSPResponse {
public:
    RTSPResponse() : version_(RTSP_VERSION), status_(StatusCode::OK) {}
    std::string ToString() const;
    static RTSPResponse FromString(const std::string &resp_str);

public:
    std::string version_;
    StatusCode status_;
    std::map<std::string, std::string> general_header_;
    ResponseHeader response_header_;
    std::map<std::string, std::string> entity_header_;
    std::optional<std::string> message_body_;
};

// Builder class for constructing RTSP responses
class RTSPResponseBuilder {

public:
    RTSPResponseBuilder();

    // Status setting
    RTSPResponseBuilder &SetStatus(StatusCode status);
    RTSPResponseBuilder &SetCSeq(int cseq);

    // General headers
    RTSPResponseBuilder &SetSession(const std::string &session);
    RTSPResponseBuilder &SetTransport(const std::string &transport);
    RTSPResponseBuilder &SetRange(const std::string &range);
    RTSPResponseBuilder &SetDate(const std::string &date);

    // Response headers
    RTSPResponseBuilder &SetLocation(const std::string &location);
    RTSPResponseBuilder &SetServer(const std::string &server);
    RTSPResponseBuilder &SetPublic(const std::vector<std::string> &methods);
    RTSPResponseBuilder &SetPublic(const std::string &methods_str);
    RTSPResponseBuilder &SetWWWAuthenticate(const std::string &auth);
    RTSPResponseBuilder &SetRTPInfo(const std::string &rtp_info);
    RTSPResponseBuilder &AddCustomHeader(const std::string &header);

    // Entity headers
    RTSPResponseBuilder &SetContentType(const std::string &content_type);
    RTSPResponseBuilder &SetContentLength(size_t length);

    // Message body
    RTSPResponseBuilder &SetMessageBody(const std::string &body);
    RTSPResponseBuilder &SetSdp(const std::string &sdp);

    // Build the final response
    RTSPResponse Build() const;

private:
    RTSPResponse response_;
};

// Factory methods for common response types
class RTSPResponseFactory {
public:
    static RTSPResponseBuilder CreateOK(int cseq);
    static RTSPResponseBuilder CreateOptionsOK(int cseq);
    static RTSPResponseBuilder CreateDescribeOK(int cseq);
    static RTSPResponseBuilder CreateSetupOK(int cseq);
    static RTSPResponseBuilder CreatePlayOK(int cseq);
    static RTSPResponseBuilder CreatePauseOK(int cseq);
    static RTSPResponseBuilder CreateTeardownOK(int cseq);
    static RTSPResponseBuilder CreateError(StatusCode status, int cseq);
    static RTSPResponseBuilder CreateBadRequest(int cseq);
    static RTSPResponseBuilder CreateUnauthorized(int cseq);
    static RTSPResponseBuilder CreateNotFound(int cseq);
    static RTSPResponseBuilder CreateMethodNotAllowed(int cseq);
    static RTSPResponseBuilder CreateSessionNotFound(int cseq);
    static RTSPResponseBuilder CreateInternalServerError(int cseq);
    static RTSPResponseBuilder CreateNotImplemented(int cseq);
};

} // namespace lmshao::lmrtsp

#endif // LMSHAO_LMRTSP_RTSP_RESPONSE_H