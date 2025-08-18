/**
 * @author SHAO Liming <lmshao@163.com>
 * @copyright Copyright (c) 2025 SHAO Liming
 * @license MIT
 *
 * SPDX-License-Identifier: MIT
 */

#include "rtsp_response.h"

#include <algorithm>
#include <sstream>

#include "rtsp_utils.h"

namespace lmshao::rtsp {

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
    std::vector<std::string> parts = RTSPUtils::split(str, COMMA);
    for (const std::string &part : parts) {
        std::string trimmed = RTSPUtils::trim(part);
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

    std::vector<std::string> lines = RTSPUtils::split(header_str, CRLF);

    for (const std::string &line : lines) {
        if (line.empty()) {
            continue;
        }

        size_t colon_pos = line.find(COLON);
        if (colon_pos == std::string::npos) {
            // Invalid header line, add to custom headers
            header.custom_header_.push_back(line);
            continue;
        }

        std::string header_name = RTSPUtils::trim(line.substr(0, colon_pos));
        std::string header_value = RTSPUtils::trim(line.substr(colon_pos + 1));

        // Convert header name to lowercase for comparison
        std::string header_name_lower = RTSPUtils::toLower(header_name);

        // Parse standard response headers
        if (header_name_lower == RTSPUtils::toLower(LOCATION)) {
            header.location_ = header_value;
        } else if (header_name_lower == RTSPUtils::toLower(PROXY_AUTHENTICATE)) {
            header.proxy_authenticate_ = header_value;
        } else if (header_name_lower == RTSPUtils::toLower(PUBLIC)) {
            // Parse comma-separated public methods
            header.public_methods_ = splitCommaSeparated(header_value);
        } else if (header_name_lower == RTSPUtils::toLower(RETRY_AFTER)) {
            header.retry_after_ = header_value;
        } else if (header_name_lower == RTSPUtils::toLower(SERVER)) {
            header.server_ = header_value;
        } else if (header_name_lower == RTSPUtils::toLower(VARY)) {
            header.vary_ = header_value;
        } else if (header_name_lower == RTSPUtils::toLower(WWW_AUTHENTICATE)) {
            header.www_authenticate_ = header_value;
        } else if (header_name_lower == RTSPUtils::toLower(RTP_INFO)) {
            header.rtp_info_ = header_value;
        } else {
            // Unknown header, add to custom headers
            header.custom_header_.push_back(header_name + COLON + SP + header_value);
        }
    }

    return header;
}

RTSPResponse RTSPResponse::FromString(const std::string &resp_str)
{
    RTSPResponse response;

    if (resp_str.empty()) {
        return response;
    }

    // Split the response into lines
    std::vector<std::string> lines = RTSPUtils::split(resp_str, CRLF);

    if (lines.empty()) {
        return response;
    }

    // Parse the status line (first line)
    std::string status_line = lines[0];
    std::vector<std::string> status_parts = RTSPUtils::split(status_line, SP);

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

        std::string header_name = RTSPUtils::trim(line.substr(0, colon_pos));
        std::string header_value = RTSPUtils::trim(line.substr(colon_pos + 1));

        // Convert header name to lowercase for comparison
        std::string header_name_lower = RTSPUtils::toLower(header_name);

        // Classify headers into general, response, and entity headers
        if (header_name_lower == RTSPUtils::toLower(CSEQ) || header_name_lower == RTSPUtils::toLower(DATE) ||
            header_name_lower == RTSPUtils::toLower(SESSION) || header_name_lower == RTSPUtils::toLower(TRANSPORT) ||
            header_name_lower == RTSPUtils::toLower(RANGE) || header_name_lower == RTSPUtils::toLower(REQUIRE) ||
            header_name_lower == RTSPUtils::toLower(PROXY_REQUIRE)) {
            // General headers
            response.general_header_[header_name] = header_value;
        } else if (header_name_lower == RTSPUtils::toLower(CONTENT_TYPE) ||
                   header_name_lower == RTSPUtils::toLower(CONTENT_LENGTH)) {
            // Entity headers
            response.entity_header_[header_name] = header_value;
        } else if (header_name_lower == RTSPUtils::toLower(LOCATION) ||
                   header_name_lower == RTSPUtils::toLower(PROXY_AUTHENTICATE) ||
                   header_name_lower == RTSPUtils::toLower(PUBLIC) ||
                   header_name_lower == RTSPUtils::toLower(RETRY_AFTER) ||
                   header_name_lower == RTSPUtils::toLower(SERVER) || header_name_lower == RTSPUtils::toLower(VARY) ||
                   header_name_lower == RTSPUtils::toLower(WWW_AUTHENTICATE) ||
                   header_name_lower == RTSPUtils::toLower(RTP_INFO)) {
            // Response headers - parse using ResponseHeader::FromString
            std::string single_header = header_name + COLON + SP + header_value + CRLF;
            ResponseHeader parsed_header = ResponseHeader::FromString(single_header);

            // Merge the parsed header with the response header
            if (parsed_header.location_) {
                response.response_header_.location_ = parsed_header.location_;
            }
            if (parsed_header.proxy_authenticate_) {
                response.response_header_.proxy_authenticate_ = parsed_header.proxy_authenticate_;
            }
            if (!parsed_header.public_methods_.empty()) {
                response.response_header_.public_methods_ = parsed_header.public_methods_;
            }
            if (parsed_header.retry_after_) {
                response.response_header_.retry_after_ = parsed_header.retry_after_;
            }
            if (parsed_header.server_) {
                response.response_header_.server_ = parsed_header.server_;
            }
            if (parsed_header.vary_) {
                response.response_header_.vary_ = parsed_header.vary_;
            }
            if (parsed_header.www_authenticate_) {
                response.response_header_.www_authenticate_ = parsed_header.www_authenticate_;
            }
            if (parsed_header.rtp_info_) {
                response.response_header_.rtp_info_ = parsed_header.rtp_info_;
            }

            // Add custom headers
            for (const std::string &custom : parsed_header.custom_header_) {
                response.response_header_.custom_header_.push_back(custom);
            }
        } else {
            // Unknown header, add to response custom headers
            response.response_header_.custom_header_.push_back(header_name + COLON + SP + header_value);
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
            response.message_body_ = body;
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
        case StatusCode::RTSPVersionNotSupported:
            return REASON_RTSP_VERSION_NOT_SUPPORTED;
        case StatusCode::OptionNotSupported:
            return REASON_OPTION_NOT_SUPPORTED;
        default:
            return REASON_UNKNOWN;
    }
}

std::string ResponseHeader::ToString() const
{
    std::ostringstream oss;
    if (location_) {
        oss << LOCATION << COLON << SP << *location_ << CRLF;
    }
    if (proxy_authenticate_) {
        oss << PROXY_AUTHENTICATE << COLON << SP << *proxy_authenticate_ << CRLF;
    }
    if (!public_methods_.empty()) {
        oss << PUBLIC << COLON << SP;
        for (size_t i = 0; i < public_methods_.size(); ++i) {
            oss << public_methods_[i];
            if (i + 1 < public_methods_.size()) {
                oss << COMMA << SP;
            }
        }
        oss << CRLF;
    }
    if (retry_after_) {
        oss << RETRY_AFTER << COLON << SP << *retry_after_ << CRLF;
    }
    if (server_) {
        oss << SERVER << COLON << SP << *server_ << CRLF;
    }
    if (vary_) {
        oss << VARY << COLON << SP << *vary_ << CRLF;
    }
    if (www_authenticate_) {
        oss << WWW_AUTHENTICATE << COLON << SP << *www_authenticate_ << CRLF;
    }
    if (rtp_info_) {
        oss << RTP_INFO << COLON << SP << *rtp_info_ << CRLF;
    }
    for (const auto &h : custom_header_) {
        oss << h << CRLF;
    }
    return oss.str();
}

std::string RTSPResponse::ToString() const
{
    std::ostringstream oss;
    oss << version_ << SP << static_cast<uint16_t>(status_) << SP << GetReasonPhrase(status_) << CRLF;
    for (const auto &[k, v] : general_header_) {
        oss << k << COLON << SP << v << CRLF;
    }
    oss << response_header_.ToString();
    for (const auto &[k, v] : entity_header_) {
        oss << k << COLON << SP << v << CRLF;
    }
    oss << CRLF;
    if (message_body_) {
        oss << *message_body_;
    }
    return oss.str();
}

// RTSPResponseBuilder implementations
RTSPResponseBuilder::RTSPResponseBuilder()
{
    response_.version_ = RTSP_VERSION;
    response_.status_ = StatusCode::OK;
}

RTSPResponseBuilder &RTSPResponseBuilder::SetStatus(StatusCode status)
{
    response_.status_ = status;
    return *this;
}

RTSPResponseBuilder &RTSPResponseBuilder::SetCSeq(int cseq)
{
    response_.general_header_[CSEQ] = std::to_string(cseq);
    return *this;
}

RTSPResponseBuilder &RTSPResponseBuilder::SetSession(const std::string &session)
{
    response_.general_header_[SESSION] = session;
    return *this;
}

RTSPResponseBuilder &RTSPResponseBuilder::SetTransport(const std::string &transport)
{
    response_.general_header_[TRANSPORT] = transport;
    return *this;
}

RTSPResponseBuilder &RTSPResponseBuilder::SetRange(const std::string &range)
{
    response_.general_header_[RANGE] = range;
    return *this;
}

RTSPResponseBuilder &RTSPResponseBuilder::SetDate(const std::string &date)
{
    response_.general_header_[DATE] = date;
    return *this;
}

RTSPResponseBuilder &RTSPResponseBuilder::SetLocation(const std::string &location)
{
    response_.response_header_.location_ = location;
    return *this;
}

RTSPResponseBuilder &RTSPResponseBuilder::SetServer(const std::string &server)
{
    response_.response_header_.server_ = server;
    return *this;
}

RTSPResponseBuilder &RTSPResponseBuilder::SetPublic(const std::vector<std::string> &methods)
{
    response_.response_header_.public_methods_ = methods;
    return *this;
}

RTSPResponseBuilder &RTSPResponseBuilder::SetPublic(const std::string &methods_str)
{
    // Split comma-separated string into method list
    std::vector<std::string> methods;
    size_t start = 0;
    size_t end = methods_str.find(COMMA);

    while (end != std::string::npos) {
        methods.push_back(RTSPUtils::trim(methods_str.substr(start, end - start)));
        start = end + 1;
        end = methods_str.find(COMMA, start);
    }

    // Add the last method
    if (start < methods_str.length()) {
        methods.push_back(RTSPUtils::trim(methods_str.substr(start)));
    }

    response_.response_header_.public_methods_ = methods;
    return *this;
}

RTSPResponseBuilder &RTSPResponseBuilder::SetWWWAuthenticate(const std::string &auth)
{
    response_.response_header_.www_authenticate_ = auth;
    return *this;
}

RTSPResponseBuilder &RTSPResponseBuilder::SetRTPInfo(const std::string &rtp_info)
{
    response_.response_header_.rtp_info_ = rtp_info;
    return *this;
}

RTSPResponseBuilder &RTSPResponseBuilder::AddCustomHeader(const std::string &header)
{
    response_.response_header_.custom_header_.push_back(header);
    return *this;
}

RTSPResponseBuilder &RTSPResponseBuilder::SetContentType(const std::string &content_type)
{
    response_.entity_header_[CONTENT_TYPE] = content_type;
    return *this;
}

RTSPResponseBuilder &RTSPResponseBuilder::SetContentLength(size_t length)
{
    response_.entity_header_[CONTENT_LENGTH] = std::to_string(length);
    return *this;
}

RTSPResponseBuilder &RTSPResponseBuilder::SetMessageBody(const std::string &body)
{
    response_.message_body_ = body;
    if (response_.entity_header_.find(CONTENT_LENGTH) == response_.entity_header_.end()) {
        SetContentLength(body.size());
    }
    return *this;
}

RTSPResponseBuilder &RTSPResponseBuilder::SetSdp(const std::string &sdp)
{
    SetContentType(MIME_SDP);
    SetMessageBody(sdp);
    return *this;
}

RTSPResponse RTSPResponseBuilder::Build() const
{
    return response_;
}

// RTSPResponseFactory implementations
RTSPResponseBuilder RTSPResponseFactory::CreateOK(int cseq)
{
    return RTSPResponseBuilder().SetStatus(StatusCode::OK).SetCSeq(cseq);
}

RTSPResponseBuilder RTSPResponseFactory::CreateOptionsOK(int cseq)
{
    return RTSPResponseBuilder()
        .SetStatus(StatusCode::OK)
        .SetCSeq(cseq)
        .SetPublic({METHOD_OPTIONS, METHOD_DESCRIBE, METHOD_SETUP, METHOD_TEARDOWN, METHOD_PLAY, METHOD_PAUSE,
                    METHOD_ANNOUNCE, METHOD_RECORD});
}

RTSPResponseBuilder RTSPResponseFactory::CreateDescribeOK(int cseq)
{
    return RTSPResponseBuilder().SetStatus(StatusCode::OK).SetCSeq(cseq);
}

RTSPResponseBuilder RTSPResponseFactory::CreateSetupOK(int cseq)
{
    return RTSPResponseBuilder().SetStatus(StatusCode::OK).SetCSeq(cseq);
}

RTSPResponseBuilder RTSPResponseFactory::CreatePlayOK(int cseq)
{
    return RTSPResponseBuilder().SetStatus(StatusCode::OK).SetCSeq(cseq);
}

RTSPResponseBuilder RTSPResponseFactory::CreatePauseOK(int cseq)
{
    return RTSPResponseBuilder().SetStatus(StatusCode::OK).SetCSeq(cseq);
}

RTSPResponseBuilder RTSPResponseFactory::CreateTeardownOK(int cseq)
{
    return RTSPResponseBuilder().SetStatus(StatusCode::OK).SetCSeq(cseq);
}

RTSPResponseBuilder RTSPResponseFactory::CreateError(StatusCode status, int cseq)
{
    return RTSPResponseBuilder().SetStatus(status).SetCSeq(cseq);
}

RTSPResponseBuilder RTSPResponseFactory::CreateBadRequest(int cseq)
{
    return CreateError(StatusCode::BadRequest, cseq);
}

RTSPResponseBuilder RTSPResponseFactory::CreateUnauthorized(int cseq)
{
    return CreateError(StatusCode::Unauthorized, cseq);
}

RTSPResponseBuilder RTSPResponseFactory::CreateNotFound(int cseq)
{
    return CreateError(StatusCode::NotFound, cseq);
}

RTSPResponseBuilder RTSPResponseFactory::CreateMethodNotAllowed(int cseq)
{
    return CreateError(StatusCode::MethodNotAllowed, cseq);
}

RTSPResponseBuilder RTSPResponseFactory::CreateSessionNotFound(int cseq)
{
    return CreateError(StatusCode::SessionNotFound, cseq);
}

RTSPResponseBuilder RTSPResponseFactory::CreateInternalServerError(int cseq)
{
    return CreateError(StatusCode::InternalServerError, cseq);
}

RTSPResponseBuilder RTSPResponseFactory::CreateNotImplemented(int cseq)
{
    return CreateError(StatusCode::NotImplemented, cseq);
}

} // namespace lmshao::rtsp
