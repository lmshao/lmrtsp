/**
 * @author SHAO Liming <lmshao@163.com>
 * @copyright Copyright (c) 2025 SHAO Liming
 * @license MIT
 *
 * SPDX-License-Identifier: MIT
 */

#include "rtsp_request.h"

#include <algorithm>
#include <sstream>

#include "internal_logger.h"
#include "rtsp_utils.h"

namespace lmshao::lmrtsp {

RequestHeader RequestHeader::FromString(const std::string &header_str)
{
    RequestHeader header;

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

        // Parse standard request headers
        if (header_name_lower == RTSPUtils::toLower(ACCEPT)) {
            header.accept_ = header_value;
        } else if (header_name_lower == RTSPUtils::toLower(ACCEPT_ENCODING)) {
            header.accept_encoding_ = header_value;
        } else if (header_name_lower == RTSPUtils::toLower(ACCEPT_LANGUAGE)) {
            header.accept_language_ = header_value;
        } else if (header_name_lower == RTSPUtils::toLower(AUTHORIZATION)) {
            header.authorization_ = header_value;
        } else if (header_name_lower == RTSPUtils::toLower(FROM)) {
            header.from_ = header_value;
        } else if (header_name_lower == RTSPUtils::toLower(IF_MODIFIED_SINCE)) {
            header.if_modified_since_ = header_value;
        } else if (header_name_lower == RTSPUtils::toLower(RANGE)) {
            header.range_ = header_value;
        } else if (header_name_lower == RTSPUtils::toLower(REFERER)) {
            header.referer_ = header_value;
        } else if (header_name_lower == RTSPUtils::toLower(USER_AGENT)) {
            header.user_agent_ = header_value;
        } else {
            // Unknown header, add to custom headers
            header.custom_header_.push_back(header_name + COLON + SP + header_value);
        }
    }

    return header;
}

RTSPRequest RTSPRequest::FromString(const std::string &req_str)
{
    RTSPRequest request;

    if (req_str.empty()) {
        RTSP_LOGD("Empty request string received");
        return request;
    }

    // Split the request into lines
    std::vector<std::string> lines = RTSPUtils::split(req_str, CRLF);

    if (lines.empty()) {
        RTSP_LOGD("No lines found in request string");
        return request;
    }

    // Parse the request line (first line)
    std::string request_line = lines[0];
    RTSP_LOGD("Request line: [%s]", request_line.c_str());

    std::vector<std::string> request_parts = RTSPUtils::split(request_line, SP);
    RTSP_LOGD("Request parts count: %zu", request_parts.size());

    if (request_parts.size() >= 3) {
        RTSP_LOGD("Request parts: [%s] [%s] [%s]", request_parts[0].c_str(), request_parts[1].c_str(),
                  request_parts[2].c_str());

        // Validate that the version looks like RTSP/x.x
        if (request_parts[2].find("RTSP/") == 0) {
            request.method_ = request_parts[0];
            request.uri_ = request_parts[1];
            request.version_ = request_parts[2];
            RTSP_LOGD("Successfully parsed request line - Method: %s, URI: %s, Version: %s", request.method_.c_str(),
                      request.uri_.c_str(), request.version_.c_str());
        } else {
            // Invalid version format
            RTSP_LOGE("Invalid RTSP version format: [%s]. Expected RTSP/x.x", request_parts[2].c_str());
            return request;
        }
    } else {
        // Invalid request line
        RTSP_LOGE("Invalid request line format. Expected at least 3 parts, got %zu. Line: [%s]", request_parts.size(),
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

        std::string header_name = RTSPUtils::trim(line.substr(0, colon_pos));
        std::string header_value = RTSPUtils::trim(line.substr(colon_pos + 1));

        // Convert header name to lowercase for comparison
        std::string header_name_lower = RTSPUtils::toLower(header_name);

        // Classify headers into general, request, and entity headers
        if (header_name_lower == RTSPUtils::toLower(CSEQ) || header_name_lower == RTSPUtils::toLower(DATE) ||
            header_name_lower == RTSPUtils::toLower(SESSION) || header_name_lower == RTSPUtils::toLower(TRANSPORT) ||
            header_name_lower == RTSPUtils::toLower(LOCATION) || header_name_lower == RTSPUtils::toLower(REQUIRE) ||
            header_name_lower == RTSPUtils::toLower(PROXY_REQUIRE)) {
            // General headers
            request.general_header_[header_name] = header_value;
        } else if (header_name_lower == RTSPUtils::toLower(CONTENT_TYPE) ||
                   header_name_lower == RTSPUtils::toLower(CONTENT_LENGTH)) {
            // Entity headers
            request.entity_header_[header_name] = header_value;
        } else if (header_name_lower == RTSPUtils::toLower(ACCEPT) ||
                   header_name_lower == RTSPUtils::toLower(ACCEPT_ENCODING) ||
                   header_name_lower == RTSPUtils::toLower(ACCEPT_LANGUAGE) ||
                   header_name_lower == RTSPUtils::toLower(AUTHORIZATION) ||
                   header_name_lower == RTSPUtils::toLower(FROM) ||
                   header_name_lower == RTSPUtils::toLower(IF_MODIFIED_SINCE) ||
                   header_name_lower == RTSPUtils::toLower(RANGE) || header_name_lower == RTSPUtils::toLower(REFERER) ||
                   header_name_lower == RTSPUtils::toLower(USER_AGENT)) {
            // Request headers - parse using RequestHeader::FromString
            std::string single_header = header_name + COLON + SP + header_value + CRLF;
            RequestHeader parsed_header = RequestHeader::FromString(single_header);

            // Merge the parsed header with the request header
            if (parsed_header.accept_) {
                request.request_header_.accept_ = parsed_header.accept_;
            }
            if (parsed_header.accept_encoding_) {
                request.request_header_.accept_encoding_ = parsed_header.accept_encoding_;
            }
            if (parsed_header.accept_language_) {
                request.request_header_.accept_language_ = parsed_header.accept_language_;
            }
            if (parsed_header.authorization_) {
                request.request_header_.authorization_ = parsed_header.authorization_;
            }
            if (parsed_header.from_) {
                request.request_header_.from_ = parsed_header.from_;
            }
            if (parsed_header.if_modified_since_) {
                request.request_header_.if_modified_since_ = parsed_header.if_modified_since_;
            }
            if (parsed_header.range_) {
                request.request_header_.range_ = parsed_header.range_;
            }
            if (parsed_header.referer_) {
                request.request_header_.referer_ = parsed_header.referer_;
            }
            if (parsed_header.user_agent_) {
                request.request_header_.user_agent_ = parsed_header.user_agent_;
            }

            // Add custom headers
            for (const std::string &custom : parsed_header.custom_header_) {
                request.request_header_.custom_header_.push_back(custom);
            }
        } else {
            // Unknown header, add to request custom headers
            request.request_header_.custom_header_.push_back(header_name + COLON + SP + header_value);
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
            request.message_body_ = body;
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
    if (accept_encoding_) {
        oss << ACCEPT_ENCODING << COLON << SP << *accept_encoding_ << CRLF;
    }
    if (accept_language_) {
        oss << ACCEPT_LANGUAGE << COLON << SP << *accept_language_ << CRLF;
    }
    if (authorization_) {
        oss << AUTHORIZATION << COLON << SP << *authorization_ << CRLF;
    }
    if (from_) {
        oss << FROM << COLON << SP << *from_ << CRLF;
    }
    if (if_modified_since_) {
        oss << IF_MODIFIED_SINCE << COLON << SP << *if_modified_since_ << CRLF;
    }
    if (range_) {
        oss << RANGE << COLON << SP << *range_ << CRLF;
    }
    if (referer_) {
        oss << REFERER << COLON << SP << *referer_ << CRLF;
    }
    if (user_agent_) {
        oss << USER_AGENT << COLON << SP << *user_agent_ << CRLF;
    }
    for (const auto &h : custom_header_) {
        oss << h << CRLF;
    }
    return oss.str();
}

std::string RTSPRequest::ToString() const
{
    std::ostringstream oss;
    oss << method_ << SP << uri_ << SP << version_ << CRLF;
    for (const auto &[k, v] : general_header_) {
        oss << k << COLON << SP << v << CRLF;
    }
    oss << request_header_.ToString();
    for (const auto &[k, v] : entity_header_) {
        oss << k << COLON << SP << v << CRLF;
    }
    oss << CRLF;
    if (message_body_) {
        oss << *message_body_;
    }
    return oss.str();
}

// RTSPRequestBuilder implementations
RTSPRequestBuilder::RTSPRequestBuilder()
{
    request_.version_ = RTSP_VERSION;
}

RTSPRequestBuilder &RTSPRequestBuilder::SetMethod(const std::string &method)
{
    request_.method_ = method;
    return *this;
}

RTSPRequestBuilder &RTSPRequestBuilder::SetUri(const std::string &uri)
{
    request_.uri_ = uri;
    return *this;
}

RTSPRequestBuilder &RTSPRequestBuilder::SetCSeq(int cseq)
{
    request_.general_header_[CSEQ] = std::to_string(cseq);
    return *this;
}

RTSPRequestBuilder &RTSPRequestBuilder::SetSession(const std::string &session)
{
    request_.general_header_[SESSION] = session;
    return *this;
}

RTSPRequestBuilder &RTSPRequestBuilder::SetTransport(const std::string &transport)
{
    request_.general_header_[TRANSPORT] = transport;
    return *this;
}

RTSPRequestBuilder &RTSPRequestBuilder::SetRange(const std::string &range)
{
    request_.general_header_[RANGE] = range;
    return *this;
}

RTSPRequestBuilder &RTSPRequestBuilder::SetLocation(const std::string &location)
{
    request_.general_header_[LOCATION] = location;
    return *this;
}

RTSPRequestBuilder &RTSPRequestBuilder::SetRequire(const std::string &require)
{
    request_.general_header_[REQUIRE] = require;
    return *this;
}

RTSPRequestBuilder &RTSPRequestBuilder::SetProxyRequire(const std::string &proxy_require)
{
    request_.general_header_[PROXY_REQUIRE] = proxy_require;
    return *this;
}

RTSPRequestBuilder &RTSPRequestBuilder::SetAccept(const std::string &accept)
{
    request_.request_header_.accept_ = accept;
    return *this;
}

RTSPRequestBuilder &RTSPRequestBuilder::SetUserAgent(const std::string &user_agent)
{
    request_.request_header_.user_agent_ = user_agent;
    return *this;
}

RTSPRequestBuilder &RTSPRequestBuilder::SetAuthorization(const std::string &authorization)
{
    request_.request_header_.authorization_ = authorization;
    return *this;
}

RTSPRequestBuilder &RTSPRequestBuilder::AddCustomHeader(const std::string &header)
{
    request_.request_header_.custom_header_.push_back(header);
    return *this;
}

RTSPRequestBuilder &RTSPRequestBuilder::SetContentType(const std::string &content_type)
{
    request_.entity_header_[CONTENT_TYPE] = content_type;
    return *this;
}

RTSPRequestBuilder &RTSPRequestBuilder::SetContentLength(size_t length)
{
    request_.entity_header_[CONTENT_LENGTH] = std::to_string(length);
    return *this;
}

RTSPRequestBuilder &RTSPRequestBuilder::SetMessageBody(const std::string &body)
{
    request_.message_body_ = body;
    if (request_.entity_header_.find(CONTENT_LENGTH) == request_.entity_header_.end()) {
        SetContentLength(body.size());
    }
    return *this;
}

RTSPRequestBuilder &RTSPRequestBuilder::SetSdp(const std::string &sdp)
{
    SetContentType(MIME_SDP);
    SetMessageBody(sdp);
    return *this;
}

RTSPRequestBuilder &RTSPRequestBuilder::SetParameters(const std::vector<std::string> &params)
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

RTSPRequestBuilder &RTSPRequestBuilder::SetParameters(const std::vector<std::pair<std::string, std::string>> &params)
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

RTSPRequest RTSPRequestBuilder::Build() const
{
    return request_;
}

// RTSPRequestFactory implementations
RTSPRequestBuilder RTSPRequestFactory::CreateOptions(int cseq, const std::string &uri)
{
    return RTSPRequestBuilder().SetMethod(METHOD_OPTIONS).SetUri(uri).SetCSeq(cseq);
}

RTSPRequestBuilder RTSPRequestFactory::CreateDescribe(int cseq, const std::string &uri)
{
    return RTSPRequestBuilder().SetMethod(METHOD_DESCRIBE).SetUri(uri).SetCSeq(cseq);
}

RTSPRequestBuilder RTSPRequestFactory::CreateAnnounce(int cseq, const std::string &uri)
{
    return RTSPRequestBuilder().SetMethod(METHOD_ANNOUNCE).SetUri(uri).SetCSeq(cseq);
}

RTSPRequestBuilder RTSPRequestFactory::CreateSetup(int cseq, const std::string &uri)
{
    return RTSPRequestBuilder().SetMethod(METHOD_SETUP).SetUri(uri).SetCSeq(cseq);
}

RTSPRequestBuilder RTSPRequestFactory::CreatePlay(int cseq, const std::string &uri)
{
    return RTSPRequestBuilder().SetMethod(METHOD_PLAY).SetUri(uri).SetCSeq(cseq);
}

RTSPRequestBuilder RTSPRequestFactory::CreatePause(int cseq, const std::string &uri)
{
    return RTSPRequestBuilder().SetMethod(METHOD_PAUSE).SetUri(uri).SetCSeq(cseq);
}

RTSPRequestBuilder RTSPRequestFactory::CreateTeardown(int cseq, const std::string &uri)
{
    return RTSPRequestBuilder().SetMethod(METHOD_TEARDOWN).SetUri(uri).SetCSeq(cseq);
}

RTSPRequestBuilder RTSPRequestFactory::CreateGetParameter(int cseq, const std::string &uri)
{
    return RTSPRequestBuilder().SetMethod(METHOD_GET_PARAMETER).SetUri(uri).SetCSeq(cseq);
}

RTSPRequestBuilder RTSPRequestFactory::CreateSetParameter(int cseq, const std::string &uri)
{
    return RTSPRequestBuilder().SetMethod(METHOD_SET_PARAMETER).SetUri(uri).SetCSeq(cseq);
}

RTSPRequestBuilder RTSPRequestFactory::CreateRedirect(int cseq, const std::string &uri)
{
    return RTSPRequestBuilder().SetMethod(METHOD_REDIRECT).SetUri(uri).SetCSeq(cseq);
}

RTSPRequestBuilder RTSPRequestFactory::CreateRecord(int cseq, const std::string &uri)
{
    return RTSPRequestBuilder().SetMethod(METHOD_RECORD).SetUri(uri).SetCSeq(cseq);
}

} // namespace lmshao::lmrtsp
