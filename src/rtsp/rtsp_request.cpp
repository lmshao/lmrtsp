/**
 * @author SHAO Liming <lmshao@163.com>
 * @copyright Copyright (c) 2025 SHAO Liming
 * @license MIT
 *
 * SPDX-License-Identifier: MIT
 */

#include "rtsp_request.h"

#include <lmcore/string_utils.h>

#include <sstream>

#include "internal_logger.h"

namespace lmshao::lmrtsp {

using lmcore::StringUtils;

RequestHeader RequestHeader::FromString(const std::string &header_str)
{
    RequestHeader header;

    std::vector<std::string> lines = StringUtils::Split(header_str, CRLF);

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

        std::string header_name = StringUtils::Trim(line.substr(0, colon_pos));
        std::string header_value = StringUtils::Trim(line.substr(colon_pos + 1));

        // Parse standard request headers (case-insensitive)
        if (StringUtils::EqualsIgnoreCase(header_name, ACCEPT)) {
            header.accept_ = header_value;
        } else if (StringUtils::EqualsIgnoreCase(header_name, ACCEPT_ENCODING)) {
            header.acceptEncoding_ = header_value;
        } else if (StringUtils::EqualsIgnoreCase(header_name, ACCEPT_LANGUAGE)) {
            header.acceptLanguage_ = header_value;
        } else if (StringUtils::EqualsIgnoreCase(header_name, AUTHORIZATION)) {
            header.authorization_ = header_value;
        } else if (StringUtils::EqualsIgnoreCase(header_name, FROM)) {
            header.from_ = header_value;
        } else if (StringUtils::EqualsIgnoreCase(header_name, IF_MODIFIED_SINCE)) {
            header.ifModifiedSince_ = header_value;
        } else if (StringUtils::EqualsIgnoreCase(header_name, RANGE)) {
            header.range_ = header_value;
        } else if (StringUtils::EqualsIgnoreCase(header_name, REFERER)) {
            header.referer_ = header_value;
        } else if (StringUtils::EqualsIgnoreCase(header_name, USER_AGENT)) {
            header.userAgent_ = header_value;
        } else {
            // Unknown header, add to custom headers
            header.customHeader_.push_back(header_name + COLON + SP + header_value);
        }
    }

    return header;
}

RtspRequest RtspRequest::FromString(const std::string &req_str)
{
    RtspRequest request;

    if (req_str.empty()) {
        LMRTSP_LOGD("Empty request string received");
        return request;
    }

    // Split the request into lines
    std::vector<std::string> lines = StringUtils::Split(req_str, CRLF);

    if (lines.empty()) {
        LMRTSP_LOGD("No lines found in request string");
        return request;
    }

    // Parse the request line (first line)
    std::string request_line = lines[0];
    LMRTSP_LOGD("Request line: [%s]", request_line.c_str());

    std::vector<std::string> request_parts = StringUtils::Split(request_line, SP);
    LMRTSP_LOGD("Request parts count: %zu", request_parts.size());

    if (request_parts.size() >= 3) {
        LMRTSP_LOGD("Request parts: [%s] [%s] [%s]", request_parts[0].c_str(), request_parts[1].c_str(),
                    request_parts[2].c_str());

        // Validate that the version looks like RTSP/x.x
        if (request_parts[2].find("RTSP/") == 0) {
            request.method_ = request_parts[0];
            request.uri_ = request_parts[1];
            request.version_ = request_parts[2];
            LMRTSP_LOGD("Successfully parsed request line - Method: %s, URI: %s, Version: %s", request.method_.c_str(),
                        request.uri_.c_str(), request.version_.c_str());
        } else {
            // Invalid version format
            LMRTSP_LOGE("Invalid RTSP version format: [%s]. Expected RTSP/x.x", request_parts[2].c_str());
            return request;
        }
    } else {
        // Invalid request line
        LMRTSP_LOGE("Invalid request line format. Expected at least 3 parts, got %zu. Line: [%s]", request_parts.size(),
                    request_line.c_str());
        return request;
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

        std::string header_name = StringUtils::Trim(line.substr(0, colon_pos));
        std::string header_value = StringUtils::Trim(line.substr(colon_pos + 1));

        // Classify headers into general, request, and entity headers (case-insensitive)
        if (StringUtils::EqualsIgnoreCase(header_name, CSEQ) || StringUtils::EqualsIgnoreCase(header_name, DATE) ||
            StringUtils::EqualsIgnoreCase(header_name, SESSION) ||
            StringUtils::EqualsIgnoreCase(header_name, TRANSPORT) ||
            StringUtils::EqualsIgnoreCase(header_name, LOCATION) ||
            StringUtils::EqualsIgnoreCase(header_name, REQUIRE) ||
            StringUtils::EqualsIgnoreCase(header_name, PROXY_REQUIRE)) {
            // General headers
            request.general_header_[header_name] = header_value;
        } else if (StringUtils::EqualsIgnoreCase(header_name, CONTENT_TYPE) ||
                   StringUtils::EqualsIgnoreCase(header_name, CONTENT_LENGTH)) {
            // Entity headers
            request.entity_header_[header_name] = header_value;
        } else if (StringUtils::EqualsIgnoreCase(header_name, ACCEPT) ||
                   StringUtils::EqualsIgnoreCase(header_name, ACCEPT_ENCODING) ||
                   StringUtils::EqualsIgnoreCase(header_name, ACCEPT_LANGUAGE) ||
                   StringUtils::EqualsIgnoreCase(header_name, AUTHORIZATION) ||
                   StringUtils::EqualsIgnoreCase(header_name, FROM) ||
                   StringUtils::EqualsIgnoreCase(header_name, IF_MODIFIED_SINCE) ||
                   StringUtils::EqualsIgnoreCase(header_name, RANGE) ||
                   StringUtils::EqualsIgnoreCase(header_name, REFERER) ||
                   StringUtils::EqualsIgnoreCase(header_name, USER_AGENT)) {
            // Request headers - parse using RequestHeader::FromString
            std::string single_header = header_name + COLON + SP + header_value + CRLF;
            RequestHeader parsed_header = RequestHeader::FromString(single_header);

            // Merge the parsed header with the request header
            if (parsed_header.accept_) {
                request.requestHeader_.accept_ = parsed_header.accept_;
            }
            if (parsed_header.acceptEncoding_) {
                request.requestHeader_.acceptEncoding_ = parsed_header.acceptEncoding_;
            }
            if (parsed_header.acceptLanguage_) {
                request.requestHeader_.acceptLanguage_ = parsed_header.acceptLanguage_;
            }
            if (parsed_header.authorization_) {
                request.requestHeader_.authorization_ = parsed_header.authorization_;
            }
            if (parsed_header.from_) {
                request.requestHeader_.from_ = parsed_header.from_;
            }
            if (parsed_header.ifModifiedSince_) {
                request.requestHeader_.ifModifiedSince_ = parsed_header.ifModifiedSince_;
            }
            if (parsed_header.range_) {
                request.requestHeader_.range_ = parsed_header.range_;
            }
            if (parsed_header.referer_) {
                request.requestHeader_.referer_ = parsed_header.referer_;
            }
            if (parsed_header.userAgent_) {
                request.requestHeader_.userAgent_ = parsed_header.userAgent_;
            }

            // Add custom headers
            for (const std::string &custom : parsed_header.customHeader_) {
                request.requestHeader_.customHeader_.push_back(custom);
            }
        } else {
            // Unknown header, add to request custom headers
            request.requestHeader_.customHeader_.push_back(header_name + COLON + SP + header_value);
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
            request.messageBody_ = body;
        }
    }

    return request;
}

std::string RequestHeader::ToString() const
{
    std::ostringstream oss;
    if (accept_) {
        oss << ACCEPT << COLON << SP << *accept_ << CRLF;
    }
    if (acceptEncoding_) {
        oss << ACCEPT_ENCODING << COLON << SP << *acceptEncoding_ << CRLF;
    }
    if (acceptLanguage_) {
        oss << ACCEPT_LANGUAGE << COLON << SP << *acceptLanguage_ << CRLF;
    }
    if (authorization_) {
        oss << AUTHORIZATION << COLON << SP << *authorization_ << CRLF;
    }
    if (from_) {
        oss << FROM << COLON << SP << *from_ << CRLF;
    }
    if (ifModifiedSince_) {
        oss << IF_MODIFIED_SINCE << COLON << SP << *ifModifiedSince_ << CRLF;
    }
    if (range_) {
        oss << RANGE << COLON << SP << *range_ << CRLF;
    }
    if (referer_) {
        oss << REFERER << COLON << SP << *referer_ << CRLF;
    }
    if (userAgent_) {
        oss << USER_AGENT << COLON << SP << *userAgent_ << CRLF;
    }
    for (const auto &h : customHeader_) {
        oss << h << CRLF;
    }
    return oss.str();
}

std::string RtspRequest::ToString() const
{
    std::ostringstream oss;
    oss << method_ << SP << uri_ << SP << version_ << CRLF;
    for (const auto &[k, v] : general_header_) {
        oss << k << COLON << SP << v << CRLF;
    }
    oss << requestHeader_.ToString();
    for (const auto &[k, v] : entity_header_) {
        oss << k << COLON << SP << v << CRLF;
    }
    oss << CRLF;
    if (messageBody_) {
        oss << *messageBody_;
    }
    return oss.str();
}

// RtspRequestBuilder implementations
RtspRequestBuilder::RtspRequestBuilder()
{
    request_.version_ = RTSP_VERSION;
}

RtspRequestBuilder &RtspRequestBuilder::SetMethod(const std::string &method)
{
    request_.method_ = method;
    return *this;
}

RtspRequestBuilder &RtspRequestBuilder::SetUri(const std::string &uri)
{
    request_.uri_ = uri;
    return *this;
}

RtspRequestBuilder &RtspRequestBuilder::SetCSeq(int cseq)
{
    request_.general_header_[CSEQ] = std::to_string(cseq);
    return *this;
}

RtspRequestBuilder &RtspRequestBuilder::SetSession(const std::string &session)
{
    request_.general_header_[SESSION] = session;
    return *this;
}

RtspRequestBuilder &RtspRequestBuilder::SetTransport(const std::string &transport)
{
    request_.general_header_[TRANSPORT] = transport;
    return *this;
}

RtspRequestBuilder &RtspRequestBuilder::SetRange(const std::string &range)
{
    request_.general_header_[RANGE] = range;
    return *this;
}

RtspRequestBuilder &RtspRequestBuilder::SetLocation(const std::string &location)
{
    request_.general_header_[LOCATION] = location;
    return *this;
}

RtspRequestBuilder &RtspRequestBuilder::SetRequire(const std::string &require)
{
    request_.general_header_[REQUIRE] = require;
    return *this;
}

RtspRequestBuilder &RtspRequestBuilder::SetProxyRequire(const std::string &proxy_require)
{
    request_.general_header_[PROXY_REQUIRE] = proxy_require;
    return *this;
}

RtspRequestBuilder &RtspRequestBuilder::SetAccept(const std::string &accept)
{
    request_.requestHeader_.accept_ = accept;
    return *this;
}

RtspRequestBuilder &RtspRequestBuilder::SetUserAgent(const std::string &user_agent)
{
    request_.requestHeader_.userAgent_ = user_agent;
    return *this;
}

RtspRequestBuilder &RtspRequestBuilder::SetAuthorization(const std::string &authorization)
{
    request_.requestHeader_.authorization_ = authorization;
    return *this;
}

RtspRequestBuilder &RtspRequestBuilder::AddCustomHeader(const std::string &header)
{
    request_.requestHeader_.customHeader_.push_back(header);
    return *this;
}

RtspRequestBuilder &RtspRequestBuilder::SetContentType(const std::string &content_type)
{
    request_.entity_header_[CONTENT_TYPE] = content_type;
    return *this;
}

RtspRequestBuilder &RtspRequestBuilder::SetContentLength(size_t length)
{
    request_.entity_header_[CONTENT_LENGTH] = std::to_string(length);
    return *this;
}

RtspRequestBuilder &RtspRequestBuilder::SetMessageBody(const std::string &body)
{
    request_.messageBody_ = body;
    if (request_.entity_header_.find(CONTENT_LENGTH) == request_.entity_header_.end()) {
        SetContentLength(body.size());
    }
    return *this;
}

RtspRequestBuilder &RtspRequestBuilder::SetSdp(const std::string &sdp)
{
    SetContentType(MIME_SDP);
    SetMessageBody(sdp);
    return *this;
}

RtspRequestBuilder &RtspRequestBuilder::SetParameters(const std::vector<std::string> &params)
{
    std::ostringstream oss;
    for (size_t i = 0; i < params.size(); ++i) {
        oss << params[i];
        if (i + 1 < params.size()) {
            oss << CRLF;
        }
    }
    SetContentType(MIME_PARAMETERS);
    SetMessageBody(oss.str());
    return *this;
}

RtspRequestBuilder &RtspRequestBuilder::SetParameters(const std::vector<std::pair<std::string, std::string>> &params)
{
    std::ostringstream oss;
    for (size_t i = 0; i < params.size(); ++i) {
        oss << params[i].first << COLON << SP << params[i].second;
        if (i + 1 < params.size()) {
            oss << CRLF;
        }
    }
    SetContentType(MIME_PARAMETERS);
    SetMessageBody(oss.str());
    return *this;
}

RtspRequest RtspRequestBuilder::Build() const
{
    return request_;
}

// RtspRequestFactory implementations
RtspRequestBuilder RtspRequestFactory::CreateOptions(int cseq, const std::string &uri)
{
    return RtspRequestBuilder().SetMethod(METHOD_OPTIONS).SetUri(uri).SetCSeq(cseq);
}

RtspRequestBuilder RtspRequestFactory::CreateDescribe(int cseq, const std::string &uri)
{
    return RtspRequestBuilder().SetMethod(METHOD_DESCRIBE).SetUri(uri).SetCSeq(cseq);
}

RtspRequestBuilder RtspRequestFactory::CreateAnnounce(int cseq, const std::string &uri)
{
    return RtspRequestBuilder().SetMethod(METHOD_ANNOUNCE).SetUri(uri).SetCSeq(cseq);
}

RtspRequestBuilder RtspRequestFactory::CreateSetup(int cseq, const std::string &uri)
{
    return RtspRequestBuilder().SetMethod(METHOD_SETUP).SetUri(uri).SetCSeq(cseq);
}

RtspRequestBuilder RtspRequestFactory::CreatePlay(int cseq, const std::string &uri)
{
    return RtspRequestBuilder().SetMethod(METHOD_PLAY).SetUri(uri).SetCSeq(cseq);
}

RtspRequestBuilder RtspRequestFactory::CreatePause(int cseq, const std::string &uri)
{
    return RtspRequestBuilder().SetMethod(METHOD_PAUSE).SetUri(uri).SetCSeq(cseq);
}

RtspRequestBuilder RtspRequestFactory::CreateTeardown(int cseq, const std::string &uri)
{
    return RtspRequestBuilder().SetMethod(METHOD_TEARDOWN).SetUri(uri).SetCSeq(cseq);
}

RtspRequestBuilder RtspRequestFactory::CreateGetParameter(int cseq, const std::string &uri)
{
    return RtspRequestBuilder().SetMethod(METHOD_GET_PARAMETER).SetUri(uri).SetCSeq(cseq);
}

RtspRequestBuilder RtspRequestFactory::CreateSetParameter(int cseq, const std::string &uri)
{
    return RtspRequestBuilder().SetMethod(METHOD_SET_PARAMETER).SetUri(uri).SetCSeq(cseq);
}

RtspRequestBuilder RtspRequestFactory::CreateRedirect(int cseq, const std::string &uri)
{
    return RtspRequestBuilder().SetMethod(METHOD_REDIRECT).SetUri(uri).SetCSeq(cseq);
}

RtspRequestBuilder RtspRequestFactory::CreateRecord(int cseq, const std::string &uri)
{
    return RtspRequestBuilder().SetMethod(METHOD_RECORD).SetUri(uri).SetCSeq(cseq);
}

} // namespace lmshao::lmrtsp
