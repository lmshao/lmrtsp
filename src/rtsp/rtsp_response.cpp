/**
 * @author SHAO Liming <lmshao@163.com>
 * @copyright Copyright (c) 2025 SHAO Liming
 * @license MIT
 *
 * SPDX-License-Identifier: MIT
 */

#include "rtsp_response.h"

#include <sstream>

#include "rtsp_utils.h"

namespace lmshao::lmrtsp {

namespace {

// Helper function to parse status code from string
StatusCode parseStatusCode(const std::string &status_str)
{
    try {
        int code = std::stoi(status_str);
        return static_cast<StatusCode>(code);
    } catch (const std::exception &) {
        return StatusCode::InternalServerError; // Default on parse error
    }
}

// Helper function to split comma-separated values
std::vector<std::string> splitCommaSeparated(const std::string &str)
{
    std::vector<std::string> result;
    std::vector<std::string> parts = RtspUtils::split(str, COMMA);
    for (const std::string &part : parts) {
        std::string trimmed = RtspUtils::trim(part);
        if (!trimmed.empty()) {
            result.push_back(trimmed);
        }
    }
    return result;
}

} // anonymous namespace

ResponseHeader ResponseHeader::FromString(const std::string &header_str)
{
    ResponseHeader header;

    std::vector<std::string> lines = RtspUtils::split(header_str, CRLF);

    for (const std::string &line : lines) {
        if (line.empty()) {
            continue;
        }

        size_t colon_pos = line.find(COLON);
        if (colon_pos == std::string::npos) {
            // Invalid header line, add to custom headers
            header.customHeader_.push_back(line);
            continue;
        }

        std::string header_name = RtspUtils::trim(line.substr(0, colon_pos));
        std::string header_value = RtspUtils::trim(line.substr(colon_pos + 1));

        // Convert header name to lowercase for comparison
        std::string header_name_lower = RtspUtils::toLower(header_name);

        // Parse standard response headers
        if (header_name_lower == RtspUtils::toLower(LOCATION)) {
            header.location_ = header_value;
        } else if (header_name_lower == RtspUtils::toLower(PROXY_AUTHENTICATE)) {
            header.proxyAuthenticate_ = header_value;
        } else if (header_name_lower == RtspUtils::toLower(PUBLIC)) {
            // Parse comma-separated public methods
            header.publicMethods_ = splitCommaSeparated(header_value);
        } else if (header_name_lower == RtspUtils::toLower(RETRY_AFTER)) {
            header.retryAfter_ = header_value;
        } else if (header_name_lower == RtspUtils::toLower(SERVER)) {
            header.server_ = header_value;
        } else if (header_name_lower == RtspUtils::toLower(VARY)) {
            header.vary_ = header_value;
        } else if (header_name_lower == RtspUtils::toLower(WWW_AUTHENTICATE)) {
            header.wwwAuthenticate_ = header_value;
        } else if (header_name_lower == RtspUtils::toLower(RTP_INFO)) {
            header.rtpInfo_ = header_value;
        } else {
            // Unknown header, add to custom headers
            header.customHeader_.push_back(header_name + COLON + SP + header_value);
        }
    }

    return header;
}

RtspResponse RtspResponse::FromString(const std::string &resp_str)
{
    RtspResponse response;

    if (resp_str.empty()) {
        return response;
    }

    // Split the response into lines
    std::vector<std::string> lines = RtspUtils::split(resp_str, CRLF);

    if (lines.empty()) {
        return response;
    }

    // Parse the status line (first line)
    std::string status_line = lines[0];
    std::vector<std::string> status_parts = RtspUtils::split(status_line, SP);

    if (status_parts.size() >= 3) {
        response.version_ = status_parts[0];
        response.status_ = parseStatusCode(status_parts[1]);
        // Note: status_parts[2] and beyond contain the reason phrase, but we don't store it
        // as it's generated automatically by GetReasonPhrase()
    } else {
        // Invalid status line
        response.status_ = StatusCode::InternalServerError;
        return response;
    }

    // Find the empty line that separates headers from body
    size_t body_start = 0;
    bool found_empty_line = false;

    for (size_t i = 1; i < lines.size(); ++i) {
        if (lines[i].empty()) {
            body_start = i + 1;
            found_empty_line = true;
            break;
        }
    }

    // Parse headers
    std::vector<std::string> header_lines;
    size_t header_end = found_empty_line ? body_start - 1 : lines.size();

    for (size_t i = 1; i < header_end; ++i) {
        if (!lines[i].empty()) {
            header_lines.push_back(lines[i]);
        }
    }

    // Process each header line
    for (const std::string &line : header_lines) {
        size_t colon_pos = line.find(COLON);
        if (colon_pos == std::string::npos) {
            continue; // Invalid header line
        }

        std::string header_name = RtspUtils::trim(line.substr(0, colon_pos));
        std::string header_value = RtspUtils::trim(line.substr(colon_pos + 1));

        // Convert header name to lowercase for comparison
        std::string header_name_lower = RtspUtils::toLower(header_name);

        // Classify headers into general, response, and entity headers
        if (header_name_lower == RtspUtils::toLower(CSEQ) || header_name_lower == RtspUtils::toLower(DATE) ||
            header_name_lower == RtspUtils::toLower(SESSION) || header_name_lower == RtspUtils::toLower(TRANSPORT) ||
            header_name_lower == RtspUtils::toLower(RANGE) || header_name_lower == RtspUtils::toLower(REQUIRE) ||
            header_name_lower == RtspUtils::toLower(PROXY_REQUIRE)) {
            // General headers
            response.general_header_[header_name] = header_value;
        } else if (header_name_lower == RtspUtils::toLower(CONTENT_TYPE) ||
                   header_name_lower == RtspUtils::toLower(CONTENT_LENGTH)) {
            // Entity headers
            response.entity_header_[header_name] = header_value;
        } else if (header_name_lower == RtspUtils::toLower(LOCATION) ||
                   header_name_lower == RtspUtils::toLower(PROXY_AUTHENTICATE) ||
                   header_name_lower == RtspUtils::toLower(PUBLIC) ||
                   header_name_lower == RtspUtils::toLower(RETRY_AFTER) ||
                   header_name_lower == RtspUtils::toLower(SERVER) || header_name_lower == RtspUtils::toLower(VARY) ||
                   header_name_lower == RtspUtils::toLower(WWW_AUTHENTICATE) ||
                   header_name_lower == RtspUtils::toLower(RTP_INFO)) {
            // Response headers - parse using ResponseHeader::FromString
            std::string single_header = header_name + COLON + SP + header_value + CRLF;
            ResponseHeader parsed_header = ResponseHeader::FromString(single_header);

            // Merge the parsed header with the response header
            if (parsed_header.location_) {
                response.responseHeader_.location_ = parsed_header.location_;
            }
            if (parsed_header.proxyAuthenticate_) {
                response.responseHeader_.proxyAuthenticate_ = parsed_header.proxyAuthenticate_;
            }
            if (!parsed_header.publicMethods_.empty()) {
                response.responseHeader_.publicMethods_ = parsed_header.publicMethods_;
            }
            if (parsed_header.retryAfter_) {
                response.responseHeader_.retryAfter_ = parsed_header.retryAfter_;
            }
            if (parsed_header.server_) {
                response.responseHeader_.server_ = parsed_header.server_;
            }
            if (parsed_header.vary_) {
                response.responseHeader_.vary_ = parsed_header.vary_;
            }
            if (parsed_header.wwwAuthenticate_) {
                response.responseHeader_.wwwAuthenticate_ = parsed_header.wwwAuthenticate_;
            }
            if (parsed_header.rtpInfo_) {
                response.responseHeader_.rtpInfo_ = parsed_header.rtpInfo_;
            }

            // Add custom headers
            for (const std::string &custom : parsed_header.customHeader_) {
                response.responseHeader_.customHeader_.push_back(custom);
            }
        } else {
            // Unknown header, add to response custom headers
            response.responseHeader_.customHeader_.push_back(header_name + COLON + SP + header_value);
        }
    }

    // Parse message body if present
    if (found_empty_line && body_start < lines.size()) {
        std::ostringstream body_oss;
        for (size_t i = body_start; i < lines.size(); ++i) {
            body_oss << lines[i];
            if (i + 1 < lines.size()) {
                body_oss << CRLF;
            }
        }
        std::string body = body_oss.str();
        if (!body.empty()) {
            response.messageBody_ = body;
        }
    }

    return response;
}

std::string GetReasonPhrase(StatusCode code)
{
    switch (code) {
        case StatusCode::Continue:
            return REASON_CONTINUE;
        case StatusCode::OK:
            return REASON_OK;
        case StatusCode::Created:
            return REASON_CREATED;
        case StatusCode::LowOnStorageSpace:
            return REASON_LOW_ON_STORAGE_SPACE;
        case StatusCode::MultipleChoices:
            return REASON_MULTIPLE_CHOICES;
        case StatusCode::MovedPermanently:
            return REASON_MOVED_PERMANENTLY;
        case StatusCode::MovedTemporarily:
            return REASON_MOVED_TEMPORARILY;
        case StatusCode::SeeOther:
            return REASON_SEE_OTHER;
        case StatusCode::NotModified:
            return REASON_NOT_MODIFIED;
        case StatusCode::UseProxy:
            return REASON_USE_PROXY;
        case StatusCode::BadRequest:
            return REASON_BAD_REQUEST;
        case StatusCode::Unauthorized:
            return REASON_UNAUTHORIZED;
        case StatusCode::PaymentRequired:
            return REASON_PAYMENT_REQUIRED;
        case StatusCode::Forbidden:
            return REASON_FORBIDDEN;
        case StatusCode::NotFound:
            return REASON_NOT_FOUND;
        case StatusCode::MethodNotAllowed:
            return REASON_METHOD_NOT_ALLOWED;
        case StatusCode::NotAcceptable:
            return REASON_NOT_ACCEPTABLE;
        case StatusCode::ProxyAuthenticationRequired:
            return REASON_PROXY_AUTHENTICATION_REQUIRED;
        case StatusCode::RequestTimeout:
            return REASON_REQUEST_TIMEOUT;
        case StatusCode::Gone:
            return REASON_GONE;
        case StatusCode::LengthRequired:
            return REASON_LENGTH_REQUIRED;
        case StatusCode::PreconditionFailed:
            return REASON_PRECONDITION_FAILED;
        case StatusCode::RequestEntityTooLarge:
            return REASON_REQUEST_ENTITY_TOO_LARGE;
        case StatusCode::RequestURITooLarge:
            return REASON_REQUEST_URI_TOO_LARGE;
        case StatusCode::UnsupportedMediaType:
            return REASON_UNSUPPORTED_MEDIA_TYPE;
        case StatusCode::ParameterNotUnderstood:
            return REASON_PARAMETER_NOT_UNDERSTOOD;
        case StatusCode::ConferenceNotFound:
            return REASON_CONFERENCE_NOT_FOUND;
        case StatusCode::NotEnoughBandwidth:
            return REASON_NOT_ENOUGH_BANDWIDTH;
        case StatusCode::SessionNotFound:
            return REASON_SESSION_NOT_FOUND;
        case StatusCode::MethodNotValidInThisState:
            return REASON_METHOD_NOT_VALID_IN_THIS_STATE;
        case StatusCode::HeaderFieldNotValidForResource:
            return REASON_HEADER_FIELD_NOT_VALID_FOR_RESOURCE;
        case StatusCode::InvalidRange:
            return REASON_INVALID_RANGE;
        case StatusCode::ParameterIsReadOnly:
            return REASON_PARAMETER_IS_READ_ONLY;
        case StatusCode::AggregateOperationNotAllowed:
            return REASON_AGGREGATE_OPERATION_NOT_ALLOWED;
        case StatusCode::OnlyAggregateOperationAllowed:
            return REASON_ONLY_AGGREGATE_OPERATION_ALLOWED;
        case StatusCode::UnsupportedTransport:
            return REASON_UNSUPPORTED_TRANSPORT;
        case StatusCode::DestinationUnreachable:
            return REASON_DESTINATION_UNREACHABLE;
        case StatusCode::InternalServerError:
            return REASON_INTERNAL_SERVER_ERROR;
        case StatusCode::NotImplemented:
            return REASON_NOT_IMPLEMENTED;
        case StatusCode::BadGateway:
            return REASON_BAD_GATEWAY;
        case StatusCode::ServiceUnavailable:
            return REASON_SERVICE_UNAVAILABLE;
        case StatusCode::GatewayTimeout:
            return REASON_GATEWAY_TIMEOUT;
        case StatusCode::RtspVersionNotSupported:
            return REASON_RTSP_VERSION_NOT_SUPPORTED;
        case StatusCode::OptionNotSupported:
            return REASON_OPTION_NOT_SUPPORTED;
        default:
            return REASON_UNKNOWN_ERROR;
    }
}

std::string ResponseHeader::ToString() const
{
    std::ostringstream oss;
    if (location_) {
        oss << LOCATION << COLON << SP << *location_ << CRLF;
    }
    if (proxyAuthenticate_) {
        oss << PROXY_AUTHENTICATE << COLON << SP << *proxyAuthenticate_ << CRLF;
    }
    if (!publicMethods_.empty()) {
        oss << PUBLIC << COLON << SP;
        for (size_t i = 0; i < publicMethods_.size(); ++i) {
            oss << publicMethods_[i];
            if (i + 1 < publicMethods_.size()) {
                oss << COMMA << SP;
            }
        }
        oss << CRLF;
    }
    if (retryAfter_) {
        oss << RETRY_AFTER << COLON << SP << *retryAfter_ << CRLF;
    }
    if (server_) {
        oss << SERVER << COLON << SP << *server_ << CRLF;
    }
    if (vary_) {
        oss << VARY << COLON << SP << *vary_ << CRLF;
    }
    if (wwwAuthenticate_) {
        oss << WWW_AUTHENTICATE << COLON << SP << *wwwAuthenticate_ << CRLF;
    }
    if (rtpInfo_) {
        oss << RTP_INFO << COLON << SP << *rtpInfo_ << CRLF;
    }
    for (const auto &h : customHeader_) {
        oss << h << CRLF;
    }
    return oss.str();
}

std::string RtspResponse::ToString() const
{
    std::ostringstream oss;
    oss << version_ << SP << static_cast<uint16_t>(status_) << SP << GetReasonPhrase(status_) << CRLF;
    for (const auto &[k, v] : general_header_) {
        oss << k << COLON << SP << v << CRLF;
    }
    oss << responseHeader_.ToString();
    for (const auto &[k, v] : entity_header_) {
        oss << k << COLON << SP << v << CRLF;
    }
    oss << CRLF;
    if (messageBody_) {
        oss << *messageBody_;
    }
    return oss.str();
}

// RtspResponseBuilder implementations
RtspResponseBuilder::RtspResponseBuilder()
{
    response_.version_ = RTSP_VERSION;
    response_.status_ = StatusCode::OK;
}

RtspResponseBuilder &RtspResponseBuilder::SetStatus(StatusCode status)
{
    response_.status_ = status;
    return *this;
}

RtspResponseBuilder &RtspResponseBuilder::SetCSeq(int cseq)
{
    response_.general_header_[CSEQ] = std::to_string(cseq);
    return *this;
}

RtspResponseBuilder &RtspResponseBuilder::SetSession(const std::string &session)
{
    response_.general_header_[SESSION] = session;
    return *this;
}

RtspResponseBuilder &RtspResponseBuilder::SetTransport(const std::string &transport)
{
    response_.general_header_[TRANSPORT] = transport;
    return *this;
}

RtspResponseBuilder &RtspResponseBuilder::SetRange(const std::string &range)
{
    response_.general_header_[RANGE] = range;
    return *this;
}

RtspResponseBuilder &RtspResponseBuilder::SetDate(const std::string &date)
{
    response_.general_header_[DATE] = date;
    return *this;
}

RtspResponseBuilder &RtspResponseBuilder::SetLocation(const std::string &location)
{
    response_.responseHeader_.location_ = location;
    return *this;
}

RtspResponseBuilder &RtspResponseBuilder::SetServer(const std::string &server)
{
    response_.responseHeader_.server_ = server;
    return *this;
}

RtspResponseBuilder &RtspResponseBuilder::SetPublic(const std::vector<std::string> &methods)
{
    response_.responseHeader_.publicMethods_ = methods;
    return *this;
}

RtspResponseBuilder &RtspResponseBuilder::SetPublic(const std::string &methods_str)
{
    // Split comma-separated string into method list
    std::vector<std::string> methods;
    size_t start = 0;
    size_t end = methods_str.find(COMMA);

    while (end != std::string::npos) {
        methods.push_back(RtspUtils::trim(methods_str.substr(start, end - start)));
        start = end + 1;
        end = methods_str.find(COMMA, start);
    }

    // Add the last method
    if (start < methods_str.length()) {
        methods.push_back(RtspUtils::trim(methods_str.substr(start)));
    }

    response_.responseHeader_.publicMethods_ = methods;
    return *this;
}

RtspResponseBuilder &RtspResponseBuilder::SetWWWAuthenticate(const std::string &auth)
{
    response_.responseHeader_.wwwAuthenticate_ = auth;
    return *this;
}

RtspResponseBuilder &RtspResponseBuilder::SetRTPInfo(const std::string &rtp_info)
{
    response_.responseHeader_.rtpInfo_ = rtp_info;
    return *this;
}

RtspResponseBuilder &RtspResponseBuilder::AddCustomHeader(const std::string &header)
{
    response_.responseHeader_.customHeader_.push_back(header);
    return *this;
}

RtspResponseBuilder &RtspResponseBuilder::SetContentType(const std::string &content_type)
{
    response_.entity_header_[CONTENT_TYPE] = content_type;
    return *this;
}

RtspResponseBuilder &RtspResponseBuilder::SetContentLength(size_t length)
{
    response_.entity_header_[CONTENT_LENGTH] = std::to_string(length);
    return *this;
}

RtspResponseBuilder &RtspResponseBuilder::SetMessageBody(const std::string &body)
{
    response_.messageBody_ = body;
    if (response_.entity_header_.find(CONTENT_LENGTH) == response_.entity_header_.end()) {
        SetContentLength(body.size());
    }
    return *this;
}

RtspResponseBuilder &RtspResponseBuilder::SetSdp(const std::string &sdp)
{
    SetContentType(MIME_SDP);
    SetMessageBody(sdp);
    return *this;
}

RtspResponse RtspResponseBuilder::Build() const
{
    return response_;
}

// RtspResponseFactory implementations
RtspResponseBuilder RtspResponseFactory::CreateOK(int cseq)
{
    return RtspResponseBuilder().SetStatus(StatusCode::OK).SetCSeq(cseq);
}

RtspResponseBuilder RtspResponseFactory::CreateOptionsOK(int cseq)
{
    return RtspResponseBuilder()
        .SetStatus(StatusCode::OK)
        .SetCSeq(cseq)
        .SetPublic({METHOD_OPTIONS, METHOD_DESCRIBE, METHOD_SETUP, METHOD_TEARDOWN, METHOD_PLAY, METHOD_PAUSE,
                    METHOD_ANNOUNCE, METHOD_RECORD, METHOD_GET_PARAMETER, METHOD_SET_PARAMETER});
}

RtspResponseBuilder RtspResponseFactory::CreateDescribeOK(int cseq)
{
    return RtspResponseBuilder().SetStatus(StatusCode::OK).SetCSeq(cseq);
}

RtspResponseBuilder RtspResponseFactory::CreateSetupOK(int cseq)
{
    return RtspResponseBuilder().SetStatus(StatusCode::OK).SetCSeq(cseq);
}

RtspResponseBuilder RtspResponseFactory::CreatePlayOK(int cseq)
{
    return RtspResponseBuilder().SetStatus(StatusCode::OK).SetCSeq(cseq);
}

RtspResponseBuilder RtspResponseFactory::CreatePauseOK(int cseq)
{
    return RtspResponseBuilder().SetStatus(StatusCode::OK).SetCSeq(cseq);
}

RtspResponseBuilder RtspResponseFactory::CreateTeardownOK(int cseq)
{
    return RtspResponseBuilder().SetStatus(StatusCode::OK).SetCSeq(cseq);
}

RtspResponseBuilder RtspResponseFactory::CreateError(StatusCode status, int cseq)
{
    return RtspResponseBuilder().SetStatus(status).SetCSeq(cseq);
}

RtspResponseBuilder RtspResponseFactory::CreateBadRequest(int cseq)
{
    return CreateError(StatusCode::BadRequest, cseq);
}

RtspResponseBuilder RtspResponseFactory::CreateUnauthorized(int cseq)
{
    return CreateError(StatusCode::Unauthorized, cseq);
}

RtspResponseBuilder RtspResponseFactory::CreateNotFound(int cseq)
{
    return CreateError(StatusCode::NotFound, cseq);
}

RtspResponseBuilder RtspResponseFactory::CreateMethodNotAllowed(int cseq)
{
    return CreateError(StatusCode::MethodNotAllowed, cseq);
}

RtspResponseBuilder RtspResponseFactory::CreateSessionNotFound(int cseq)
{
    return CreateError(StatusCode::SessionNotFound, cseq);
}

RtspResponseBuilder RtspResponseFactory::CreateInternalServerError(int cseq)
{
    return CreateError(StatusCode::InternalServerError, cseq);
}

RtspResponseBuilder RtspResponseFactory::CreateNotImplemented(int cseq)
{
    return CreateError(StatusCode::NotImplemented, cseq);
}

} // namespace lmshao::lmrtsp
