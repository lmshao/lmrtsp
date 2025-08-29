/**
 * @author SHAO Liming <lmshao@163.com>
 * @copyright Copyright (c) 2025 SHAO Liming
 * @license MIT
 *
 * SPDX-License-Identifier: MIT
 */

#ifndef LMSHAO_RTSP_RTSP_HEADERS_H
#define LMSHAO_RTSP_RTSP_HEADERS_H

namespace lmshao::rtsp {

// RTSP Methods
constexpr const char *METHOD_OPTIONS = "OPTIONS";
constexpr const char *METHOD_DESCRIBE = "DESCRIBE";
constexpr const char *METHOD_ANNOUNCE = "ANNOUNCE";
constexpr const char *METHOD_SETUP = "SETUP";
constexpr const char *METHOD_PLAY = "PLAY";
constexpr const char *METHOD_PAUSE = "PAUSE";
constexpr const char *METHOD_TEARDOWN = "TEARDOWN";
constexpr const char *METHOD_RECORD = "RECORD";
constexpr const char *METHOD_GET_PARAMETER = "GET_PARAMETER";
constexpr const char *METHOD_SET_PARAMETER = "SET_PARAMETER";
constexpr const char *METHOD_REDIRECT = "REDIRECT";

// General Headers
constexpr const char *CSEQ = "CSeq";
constexpr const char *DATE = "Date";
constexpr const char *SESSION = "Session";
constexpr const char *TRANSPORT = "Transport";

// Request Headers
constexpr const char *ACCEPT = "Accept";
constexpr const char *ACCEPT_ENCODING = "Accept-Encoding";
constexpr const char *ACCEPT_LANGUAGE = "Accept-Language";
constexpr const char *AUTHORIZATION = "Authorization";
constexpr const char *FROM = "From";
constexpr const char *IF_MODIFIED_SINCE = "If-Modified-Since";
constexpr const char *RANGE = "Range";
constexpr const char *REFERER = "Referer";
constexpr const char *USER_AGENT = "User-Agent";
constexpr const char *REQUIRE = "Require";
constexpr const char *PROXY_REQUIRE = "Proxy-Require";

// Response Headers
constexpr const char *LOCATION = "Location";
constexpr const char *PROXY_AUTHENTICATE = "Proxy-Authenticate";
constexpr const char *PUBLIC = "Public";
constexpr const char *RETRY_AFTER = "Retry-After";
constexpr const char *SERVER = "Server";
constexpr const char *VARY = "Vary";
constexpr const char *WWW_AUTHENTICATE = "WWW-Authenticate";
constexpr const char *RTP_INFO = "RTP-Info";

// Entity Headers
constexpr const char *CONTENT_TYPE = "Content-Type";
constexpr const char *CONTENT_LENGTH = "Content-Length";

// MIME Types
constexpr const char *MIME_SDP = "application/sdp";
constexpr const char *MIME_PARAMETERS = "text/parameters";

// Protocol Version
constexpr const char *RTSP_VERSION = "RTSP/1.0";

// Protocol Separators
constexpr const char *CRLF = "\r\n";
constexpr const char *CRLFCRLF = "\r\n\r\n";
constexpr const char *COLON = ":";
constexpr const char *SP = " ";
constexpr const char *COMMA = ",";

// Status Code Reason Phrases
constexpr const char *REASON_CONTINUE = "Continue";
constexpr const char *REASON_OK = "OK";
constexpr const char *REASON_CREATED = "Created";
constexpr const char *REASON_LOW_ON_STORAGE_SPACE = "Low on Storage Space";
constexpr const char *REASON_MULTIPLE_CHOICES = "Multiple Choices";
constexpr const char *REASON_MOVED_PERMANENTLY = "Moved Permanently";
constexpr const char *REASON_MOVED_TEMPORARILY = "Moved Temporarily";
constexpr const char *REASON_SEE_OTHER = "See Other";
constexpr const char *REASON_NOT_MODIFIED = "Not Modified";
constexpr const char *REASON_USE_PROXY = "Use Proxy";
constexpr const char *REASON_BAD_REQUEST = "Bad Request";
constexpr const char *REASON_UNAUTHORIZED = "Unauthorized";
constexpr const char *REASON_PAYMENT_REQUIRED = "Payment Required";
constexpr const char *REASON_FORBIDDEN = "Forbidden";
constexpr const char *REASON_NOT_FOUND = "Not Found";
constexpr const char *REASON_METHOD_NOT_ALLOWED = "Method Not Allowed";
constexpr const char *REASON_NOT_ACCEPTABLE = "Not Acceptable";
constexpr const char *REASON_PROXY_AUTHENTICATION_REQUIRED = "Proxy Authentication Required";
constexpr const char *REASON_REQUEST_TIMEOUT = "Request Time-out";
constexpr const char *REASON_GONE = "Gone";
constexpr const char *REASON_LENGTH_REQUIRED = "Length Required";
constexpr const char *REASON_PRECONDITION_FAILED = "Precondition Failed";
constexpr const char *REASON_REQUEST_ENTITY_TOO_LARGE = "Request Entity Too Large";
constexpr const char *REASON_REQUEST_URI_TOO_LARGE = "Request-URI Too Large";
constexpr const char *REASON_UNSUPPORTED_MEDIA_TYPE = "Unsupported Media Type";
constexpr const char *REASON_PARAMETER_NOT_UNDERSTOOD = "Parameter Not Understood";
constexpr const char *REASON_CONFERENCE_NOT_FOUND = "Conference Not Found";
constexpr const char *REASON_NOT_ENOUGH_BANDWIDTH = "Not Enough Bandwidth";
constexpr const char *REASON_SESSION_NOT_FOUND = "Session Not Found";
constexpr const char *REASON_METHOD_NOT_VALID_IN_THIS_STATE = "Method Not Valid in This State";
constexpr const char *REASON_HEADER_FIELD_NOT_VALID_FOR_RESOURCE = "Header Field Not Valid for Resource";
constexpr const char *REASON_INVALID_RANGE = "Invalid Range";
constexpr const char *REASON_PARAMETER_IS_READ_ONLY = "Parameter Is Read-Only";
constexpr const char *REASON_AGGREGATE_OPERATION_NOT_ALLOWED = "Aggregate operation not allowed";
constexpr const char *REASON_ONLY_AGGREGATE_OPERATION_ALLOWED = "Only aggregate operation allowed";
constexpr const char *REASON_UNSUPPORTED_TRANSPORT = "Unsupported transport";
constexpr const char *REASON_DESTINATION_UNREACHABLE = "Destination unreachable";
constexpr const char *REASON_INTERNAL_SERVER_ERROR = "Internal Server Error";
constexpr const char *REASON_NOT_IMPLEMENTED = "Not Implemented";
constexpr const char *REASON_BAD_GATEWAY = "Bad Gateway";
constexpr const char *REASON_SERVICE_UNAVAILABLE = "Service Unavailable";
constexpr const char *REASON_GATEWAY_TIMEOUT = "Gateway Time-out";
constexpr const char *REASON_RTSP_VERSION_NOT_SUPPORTED = "RTSP Version not supported";
constexpr const char *REASON_OPTION_NOT_SUPPORTED = "Option not supported";
// Avoid conflict with Windows REASON_UNKNOWN macro
constexpr const char *REASON_UNKNOWN_ERROR = "Unknown";

} // namespace lmshao::rtsp

#endif // LMSHAO_RTSP_RTSP_HEADERS_H
