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

#include "lmrtsp/rtsp_headers.h"

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
    RtspVersionNotSupported = 505,
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
    std::optional<std::string> proxyAuthenticate_;
    std::vector<std::string> publicMethods_;
    std::optional<std::string> retryAfter_;
    std::optional<std::string> server_;
    std::optional<std::string> vary_;
    std::optional<std::string> wwwAuthenticate_;
    std::optional<std::string> rtpInfo_;
    std::vector<std::string> customHeader_;
};

// RTSP Response class
class RtspResponse {
public:
    RtspResponse() : version_(RTSP_VERSION), status_(StatusCode::OK) {}
    std::string ToString() const;
    static RtspResponse FromString(const std::string &resp_str);

public:
    std::string version_;
    StatusCode status_;
    std::map<std::string, std::string> general_header_;
    ResponseHeader responseHeader_;
    std::map<std::string, std::string> entity_header_;
    std::optional<std::string> messageBody_;
};

// Builder class for constructing RTSP responses
class RtspResponseBuilder {

public:
    RtspResponseBuilder();

    // Status setting
    RtspResponseBuilder &SetStatus(StatusCode status);
    RtspResponseBuilder &SetCSeq(int cseq);

    // General headers
    RtspResponseBuilder &SetSession(const std::string &session);
    RtspResponseBuilder &SetTransport(const std::string &transport);
    RtspResponseBuilder &SetRange(const std::string &range);
    RtspResponseBuilder &SetDate(const std::string &date);

    // Response headers
    RtspResponseBuilder &SetLocation(const std::string &location);
    RtspResponseBuilder &SetServer(const std::string &server);
    RtspResponseBuilder &SetPublic(const std::vector<std::string> &methods);
    RtspResponseBuilder &SetPublic(const std::string &methods_str);
    RtspResponseBuilder &SetWWWAuthenticate(const std::string &auth);
    RtspResponseBuilder &SetRTPInfo(const std::string &rtp_info);
    RtspResponseBuilder &AddCustomHeader(const std::string &header);

    // Entity headers
    RtspResponseBuilder &SetContentType(const std::string &content_type);
    RtspResponseBuilder &SetContentLength(size_t length);

    // Message body
    RtspResponseBuilder &SetMessageBody(const std::string &body);
    RtspResponseBuilder &SetSdp(const std::string &sdp);

    // Build the final response
    RtspResponse Build() const;

private:
    RtspResponse response_;
};

// Factory methods for common response types
class RtspResponseFactory {
public:
    static RtspResponseBuilder CreateOK(int cseq);
    static RtspResponseBuilder CreateOptionsOK(int cseq);
    static RtspResponseBuilder CreateDescribeOK(int cseq);
    static RtspResponseBuilder CreateSetupOK(int cseq);
    static RtspResponseBuilder CreatePlayOK(int cseq);
    static RtspResponseBuilder CreatePauseOK(int cseq);
    static RtspResponseBuilder CreateTeardownOK(int cseq);
    static RtspResponseBuilder CreateError(StatusCode status, int cseq);
    static RtspResponseBuilder CreateBadRequest(int cseq);
    static RtspResponseBuilder CreateUnauthorized(int cseq);
    static RtspResponseBuilder CreateNotFound(int cseq);
    static RtspResponseBuilder CreateMethodNotAllowed(int cseq);
    static RtspResponseBuilder CreateSessionNotFound(int cseq);
    static RtspResponseBuilder CreateInternalServerError(int cseq);
    static RtspResponseBuilder CreateNotImplemented(int cseq);
};

} // namespace lmshao::lmrtsp

#endif // LMSHAO_LMRTSP_RTSP_RESPONSE_H