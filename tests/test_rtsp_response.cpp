/**
 * @author SHAO Liming <lmshao@163.com>
 * @copyright Copyright (c) 2025 SHAO Liming
 * @license MIT
 *
 * SPDX-License-Identifier: MIT
 */

#include <string>

#include "lmrtsp/rtsp_response.h"
#include "test_framework.h"

using namespace test_framework;
using namespace lmshao::lmrtsp;

void test_rtsp_response_basic_construction()
{
    auto response = RtspResponseBuilder().SetStatus(StatusCode::OK).SetCSeq(1).Build();

    std::string response_str = response.ToString();

    ASSERT_STR_CONTAINS(response_str, "RTSP/1.0 200 OK");
    ASSERT_STR_CONTAINS(response_str, "CSeq: 1");
}

void test_rtsp_response_factory_ok()
{
    auto ok_response = RtspResponseFactory::CreateOK(1).SetServer("TestServer/1.0").Build();

    std::string response_str = ok_response.ToString();

    ASSERT_STR_CONTAINS(response_str, "RTSP/1.0 200 OK");
    ASSERT_STR_CONTAINS(response_str, "CSeq: 1");
    ASSERT_STR_CONTAINS(response_str, "Server: TestServer/1.0");
}

void test_rtsp_response_factory_options_ok()
{
    auto options_response = RtspResponseFactory::CreateOptionsOK(1).SetServer("TestServer/1.0").Build();

    std::string response_str = options_response.ToString();

    ASSERT_STR_CONTAINS(response_str, "RTSP/1.0 200 OK");
    ASSERT_STR_CONTAINS(response_str, "CSeq: 1");
    ASSERT_STR_CONTAINS(response_str, "Public: OPTIONS, DESCRIBE, SETUP, TEARDOWN, PLAY, PAUSE, ANNOUNCE, RECORD");
    ASSERT_STR_CONTAINS(response_str, "Server: TestServer/1.0");
}

void test_rtsp_response_factory_describe_ok()
{
    std::string test_sdp = "v=0\r\no=- 123 456 IN IP4 192.168.1.1\r\ns=Test Session\r\n";
    auto describe_response =
        RtspResponseFactory::CreateDescribeOK(2).SetServer("TestServer/1.0").SetSdp(test_sdp).Build();

    std::string response_str = describe_response.ToString();

    ASSERT_STR_CONTAINS(response_str, "RTSP/1.0 200 OK");
    ASSERT_STR_CONTAINS(response_str, "CSeq: 2");
    ASSERT_STR_CONTAINS(response_str, "Content-Type: application/sdp");
    ASSERT_STR_CONTAINS(response_str, "Content-Length: " + std::to_string(test_sdp.length()));
    ASSERT_STR_CONTAINS(response_str, test_sdp);
}

void test_rtsp_response_factory_setup_ok()
{
    auto setup_response = RtspResponseFactory::CreateSetupOK(3)
                              .SetSession("ABCD1234")
                              .SetTransport("RTP/AVP/UDP;unicast;client_port=4588-4589;server_port=6256-6257")
                              .Build();

    std::string response_str = setup_response.ToString();

    ASSERT_STR_CONTAINS(response_str, "RTSP/1.0 200 OK");
    ASSERT_STR_CONTAINS(response_str, "CSeq: 3");
    ASSERT_STR_CONTAINS(response_str, "Session: ABCD1234");
    ASSERT_STR_CONTAINS(response_str, "Transport: RTP/AVP/UDP;unicast;client_port=4588-4589;server_port=6256-6257");
}

void test_rtsp_response_factory_play_ok()
{
    auto play_response = RtspResponseFactory::CreatePlayOK(4)
                             .SetSession("ABCD1234")
                             .SetRange("npt=0-")
                             .SetRTPInfo("url=rtsp://example.com/stream/track1;seq=45102;rtptime=2890844526")
                             .Build();

    std::string response_str = play_response.ToString();

    ASSERT_STR_CONTAINS(response_str, "RTSP/1.0 200 OK");
    ASSERT_STR_CONTAINS(response_str, "CSeq: 4");
    ASSERT_STR_CONTAINS(response_str, "Session: ABCD1234");
    ASSERT_STR_CONTAINS(response_str, "Range: npt=0-");
    ASSERT_STR_CONTAINS(response_str, "RTP-Info: url=rtsp://example.com/stream/track1;seq=45102;rtptime=2890844526");
}

void test_rtsp_response_error_codes()
{
    struct ErrorTest {
        StatusCode code;
        std::string expected_line;
        std::function<RtspResponseBuilder()> factory_func;
    };

    std::vector<ErrorTest> error_tests = {
        {StatusCode::BadRequest, "RTSP/1.0 400 Bad Request", []() { return RtspResponseFactory::CreateBadRequest(1); }},
        {StatusCode::Unauthorized, "RTSP/1.0 401 Unauthorized",
         []() { return RtspResponseFactory::CreateUnauthorized(2); }},
        {StatusCode::NotFound, "RTSP/1.0 404 Not Found", []() { return RtspResponseFactory::CreateNotFound(3); }},
        {StatusCode::MethodNotAllowed, "RTSP/1.0 405 Method Not Allowed",
         []() { return RtspResponseFactory::CreateMethodNotAllowed(4); }},
        {StatusCode::SessionNotFound, "RTSP/1.0 454 Session Not Found",
         []() { return RtspResponseFactory::CreateSessionNotFound(5); }},
        {StatusCode::InternalServerError, "RTSP/1.0 500 Internal Server Error",
         []() { return RtspResponseFactory::CreateInternalServerError(6); }}};

    for (const auto &test : error_tests) {
        auto response = test.factory_func().Build();
        std::string response_str = response.ToString();
        ASSERT_STR_CONTAINS(response_str, test.expected_line);
    }
}

void test_rtsp_response_custom_headers()
{
    auto response = RtspResponseBuilder()
                        .SetStatus(StatusCode::Created)
                        .SetCSeq(7)
                        .SetServer("TestServer/1.0")
                        .AddCustomHeader("X-Custom-Header: custom-value")
                        .AddCustomHeader("X-Another-Header: another-value")
                        .Build();

    std::string response_str = response.ToString();

    ASSERT_STR_CONTAINS(response_str, "RTSP/1.0 201 Created");
    ASSERT_STR_CONTAINS(response_str, "X-Custom-Header: custom-value");
    ASSERT_STR_CONTAINS(response_str, "X-Another-Header: another-value");
}

void test_rtsp_response_with_body()
{
    std::string body_content = "packets_received: 1000\r\njitter: 0.01\r\npacket_loss: 0";
    auto response = RtspResponseBuilder()
                        .SetStatus(StatusCode::OK)
                        .SetCSeq(8)
                        .SetContentType("text/parameters")
                        .SetMessageBody(body_content)
                        .Build();

    std::string response_str = response.ToString();

    ASSERT_STR_CONTAINS(response_str, "Content-Type: text/parameters");
    ASSERT_STR_CONTAINS(response_str, "Content-Length: " + std::to_string(body_content.length()));
    ASSERT_STR_CONTAINS(response_str, body_content);
}

void test_rtsp_response_unauthorized_with_auth()
{
    auto auth_response = RtspResponseFactory::CreateUnauthorized(9)
                             .SetWWWAuthenticate("Digest realm=\"MyRTSPServer\", nonce=\"b64token\", algorithm=\"MD5\"")
                             .Build();

    std::string response_str = auth_response.ToString();

    ASSERT_STR_CONTAINS(response_str, "RTSP/1.0 401 Unauthorized");
    ASSERT_STR_CONTAINS(response_str,
                        "WWW-Authenticate: Digest realm=\"MyRTSPServer\", nonce=\"b64token\", algorithm=\"MD5\"");
}

void test_rtsp_response_status_code_coverage()
{
    // Test that all important status codes have correct reason phrases
    struct StatusTest {
        StatusCode code;
        std::string expected_line;
    };

    std::vector<StatusTest> status_tests = {
        {StatusCode::OK, "200 OK"},
        {StatusCode::Created, "201 Created"},
        {StatusCode::BadRequest, "400 Bad Request"},
        {StatusCode::Unauthorized, "401 Unauthorized"},
        {StatusCode::NotFound, "404 Not Found"},
        {StatusCode::MethodNotAllowed, "405 Method Not Allowed"},
        {StatusCode::SessionNotFound, "454 Session Not Found"},
        {StatusCode::MethodNotValidInThisState, "455 Method Not Valid in This State"},
        {StatusCode::InvalidRange, "457 Invalid Range"},
        {StatusCode::ParameterIsReadOnly, "458 Parameter Is Read-Only"},
        {StatusCode::UnsupportedTransport, "461 Unsupported transport"},
        {StatusCode::InternalServerError, "500 Internal Server Error"},
        {StatusCode::NotImplemented, "501 Not Implemented"},
        {StatusCode::BadGateway, "502 Bad Gateway"},
        {StatusCode::ServiceUnavailable, "503 Service Unavailable"},
        {StatusCode::RtspVersionNotSupported, "505 RTSP Version not supported"},
        {StatusCode::OptionNotSupported, "551 Option not supported"}};

    for (const auto &test : status_tests) {
        auto response = RtspResponseBuilder().SetStatus(test.code).SetCSeq(1).Build();

        std::string response_str = response.ToString();
        ASSERT_STR_CONTAINS(response_str, test.expected_line);
    }
}

void test_rtsp_response_header_parsing()
{
    // Test ResponseHeader::FromString
    std::string header_str = "Server: MyRTSPServer/1.0\r\n"
                             "Public: OPTIONS, DESCRIBE, SETUP, TEARDOWN, PLAY, PAUSE\r\n"
                             "Location: rtsp://example.com/newpath\r\n"
                             "RTP-Info: url=rtsp://example.com/video;seq=12345;rtptime=2000\r\n"
                             "Custom-Response-Header: custom-value\r\n";

    ResponseHeader header = ResponseHeader::FromString(header_str);

    ASSERT_TRUE(header.server_.has_value());
    ASSERT_STR_EQ(*header.server_, "MyRTSPServer/1.0");

    ASSERT_TRUE(header.location_.has_value());
    ASSERT_STR_EQ(*header.location_, "rtsp://example.com/newpath");

    ASSERT_TRUE(header.rtpInfo_.has_value());
    ASSERT_STR_EQ(*header.rtpInfo_, "url=rtsp://example.com/video;seq=12345;rtptime=2000");

    ASSERT_EQ(header.publicMethods_.size(), 6);
    ASSERT_STR_EQ(header.publicMethods_[0], "OPTIONS");
    ASSERT_STR_EQ(header.publicMethods_[1], "DESCRIBE");
    ASSERT_STR_EQ(header.publicMethods_[5], "PAUSE");

    ASSERT_EQ(header.customHeader_.size(), 1);
    ASSERT_STR_EQ(header.customHeader_[0], "Custom-Response-Header: custom-value");
}

void test_rtsp_response_full_parsing()
{
    // Test RtspResponse::FromString with complete response
    std::string response_str = "RTSP/1.0 200 OK\r\n"
                               "CSeq: 2\r\n"
                               "Server: MyRTSPServer/1.0\r\n"
                               "Session: 12345678\r\n"
                               "Transport: RTP/AVP;unicast;client_port=8000-8001;server_port=9000-9001\r\n"
                               "Content-Type: application/sdp\r\n"
                               "Content-Length: 25\r\n"
                               "\r\n"
                               "v=0\r\no=- 123456 654321";

    RtspResponse response = RtspResponse::FromString(response_str);

    // Check status line
    ASSERT_STR_EQ(response.version_, "RTSP/1.0");
    ASSERT_EQ(static_cast<int>(response.status_), 200);

    // Check general headers
    ASSERT_STR_EQ(response.general_header_.at("CSeq"), "2");
    ASSERT_STR_EQ(response.general_header_.at("Session"), "12345678");
    ASSERT_STR_EQ(response.general_header_.at("Transport"),
                  "RTP/AVP;unicast;client_port=8000-8001;server_port=9000-9001");

    // Check response headers
    ASSERT_TRUE(response.responseHeader_.server_.has_value());
    ASSERT_STR_EQ(*response.responseHeader_.server_, "MyRTSPServer/1.0");

    // Check entity headers
    ASSERT_STR_EQ(response.entity_header_.at("Content-Type"), "application/sdp");
    ASSERT_STR_EQ(response.entity_header_.at("Content-Length"), "25");

    // Check message body
    ASSERT_TRUE(response.messageBody_.has_value());
    ASSERT_STR_EQ(*response.messageBody_, "v=0\r\no=- 123456 654321");
}

void test_rtsp_response_roundtrip()
{
    // Test round-trip: build -> toString -> parse -> toString
    auto original_response = RtspResponseFactory::CreateDescribeOK(123)
                                 .SetServer("TestServer/1.0")
                                 .SetSession("abcdef123456")
                                 .SetSdp("v=0\r\no=- 123 456 IN IP4 127.0.0.1")
                                 .Build();

    std::string response_str = original_response.ToString();
    RtspResponse parsed_response = RtspResponse::FromString(response_str);
    std::string reparsed_str = parsed_response.ToString();

    // Check that key components are preserved
    ASSERT_STR_EQ(parsed_response.version_, "RTSP/1.0");
    ASSERT_EQ(static_cast<int>(parsed_response.status_), 200);
    ASSERT_STR_EQ(parsed_response.general_header_.at("CSeq"), "123");

    ASSERT_TRUE(parsed_response.responseHeader_.server_.has_value());
    ASSERT_STR_EQ(*parsed_response.responseHeader_.server_, "TestServer/1.0");

    ASSERT_STR_EQ(parsed_response.general_header_.at("Session"), "abcdef123456");

    ASSERT_TRUE(parsed_response.messageBody_.has_value());
    ASSERT_STR_EQ(*parsed_response.messageBody_, "v=0\r\no=- 123 456 IN IP4 127.0.0.1");
}

void test_rtsp_response_error_parsing()
{
    // Test parsing of error responses

    // Test 404 Not Found
    std::string not_found_str = "RTSP/1.0 404 Not Found\r\n"
                                "CSeq: 5\r\n"
                                "Server: MyRTSPServer/1.0\r\n"
                                "\r\n";

    RtspResponse not_found = RtspResponse::FromString(not_found_str);
    ASSERT_STR_EQ(not_found.version_, "RTSP/1.0");
    ASSERT_EQ(static_cast<int>(not_found.status_), 404);
    ASSERT_STR_EQ(not_found.general_header_.at("CSeq"), "5");
    ASSERT_TRUE(not_found.responseHeader_.server_.has_value());
    ASSERT_STR_EQ(*not_found.responseHeader_.server_, "MyRTSPServer/1.0");

    // Test 500 Internal Server Error
    std::string server_error_str = "RTSP/1.0 500 Internal Server Error\r\n\r\n";
    RtspResponse server_error = RtspResponse::FromString(server_error_str);
    ASSERT_STR_EQ(server_error.version_, "RTSP/1.0");
    ASSERT_EQ(static_cast<int>(server_error.status_), 500);
}

void test_rtsp_response_malformed_parsing()
{
    // Test parsing of malformed responses

    // Empty string
    RtspResponse empty_response = RtspResponse::FromString("");
    ASSERT_STR_EQ(empty_response.version_, "RTSP/1.0");       // Default value
    ASSERT_EQ(static_cast<int>(empty_response.status_), 200); // Default value

    // Invalid status line
    RtspResponse invalid_response = RtspResponse::FromString("INVALID STATUS LINE\r\n");
    ASSERT_EQ(static_cast<int>(invalid_response.status_), 500); // Error default

    // Missing headers but valid status line
    RtspResponse minimal_response = RtspResponse::FromString("RTSP/1.0 200 OK\r\n\r\n");
    ASSERT_STR_EQ(minimal_response.version_, "RTSP/1.0");
    ASSERT_EQ(static_cast<int>(minimal_response.status_), 200);
}

int main()
{
    TestSuite suite("RTSP Response Builder Tests");

    suite.AddTest("Basic Construction", test_rtsp_response_basic_construction);
    suite.AddTest("Factory OK", test_rtsp_response_factory_ok);
    suite.AddTest("Factory OPTIONS OK", test_rtsp_response_factory_options_ok);
    suite.AddTest("Factory DESCRIBE OK", test_rtsp_response_factory_describe_ok);
    suite.AddTest("Factory SETUP OK", test_rtsp_response_factory_setup_ok);
    suite.AddTest("Factory PLAY OK", test_rtsp_response_factory_play_ok);
    suite.AddTest("Error Codes", test_rtsp_response_error_codes);
    suite.AddTest("Custom Headers", test_rtsp_response_custom_headers);
    suite.AddTest("Response with Body", test_rtsp_response_with_body);
    suite.AddTest("Unauthorized with Auth", test_rtsp_response_unauthorized_with_auth);
    suite.AddTest("Status Code Coverage", test_rtsp_response_status_code_coverage);
    suite.AddTest("Header Parsing", test_rtsp_response_header_parsing);
    suite.AddTest("Full Response Parsing", test_rtsp_response_full_parsing);
    suite.AddTest("Round-trip Parsing", test_rtsp_response_roundtrip);
    suite.AddTest("Error Response Parsing", test_rtsp_response_error_parsing);
    suite.AddTest("Malformed Response Parsing", test_rtsp_response_malformed_parsing);

    bool success = suite.RunAll();
    return success ? 0 : 1;
}
