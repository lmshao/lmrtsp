/**
 * @author SHAO Liming <lmshao@163.com>
 * @copyright Copyright (c) 2025 SHAO Liming
 * @license MIT
 *
 * SPDX-License-Identifier: MIT
 */

#include <string>

#include "lmrtsp/rtsp_request.h"
#include "test_framework.h"

using namespace test_framework;
using namespace lmshao::lmrtsp;

void test_rtsp_request_basic_construction()
{
    // Test basic request construction using builder
    auto request = RTSPRequestBuilder().SetMethod("OPTIONS").SetUri("rtsp://example.com/stream").SetCSeq(1).Build();

    std::string request_str = request.ToString();

    ASSERT_STR_CONTAINS(request_str, "OPTIONS rtsp://example.com/stream RTSP/1.0");
    ASSERT_STR_CONTAINS(request_str, "CSeq: 1");
}

void test_rtsp_request_factory_options()
{
    auto options_request = RTSPRequestFactory::CreateOptions(1, "rtsp://example.com/stream").Build();
    std::string request_str = options_request.ToString();

    ASSERT_STR_CONTAINS(request_str, "OPTIONS");
    ASSERT_STR_CONTAINS(request_str, "rtsp://example.com/stream");
    ASSERT_STR_CONTAINS(request_str, "CSeq: 1");
}

void test_rtsp_request_factory_describe()
{
    auto describe_request =
        RTSPRequestFactory::CreateDescribe(2, "rtsp://example.com/stream").SetAccept("application/sdp").Build();

    std::string request_str = describe_request.ToString();

    ASSERT_STR_CONTAINS(request_str, "DESCRIBE");
    ASSERT_STR_CONTAINS(request_str, "CSeq: 2");
    ASSERT_STR_CONTAINS(request_str, "Accept: application/sdp");
}

void test_rtsp_request_factory_setup()
{
    auto setup_request = RTSPRequestFactory::CreateSetup(3, "rtsp://example.com/stream/track1")
                             .SetTransport("RTP/AVP/UDP;unicast;client_port=4588-4589")
                             .Build();

    std::string request_str = setup_request.ToString();

    ASSERT_STR_CONTAINS(request_str, "SETUP");
    ASSERT_STR_CONTAINS(request_str, "CSeq: 3");
    ASSERT_STR_CONTAINS(request_str, "Transport: RTP/AVP/UDP;unicast;client_port=4588-4589");
}

void test_rtsp_request_factory_play()
{
    auto play_request =
        RTSPRequestFactory::CreatePlay(4, "rtsp://example.com/stream").SetSession("ABC123").SetRange("npt=0-").Build();

    std::string request_str = play_request.ToString();

    ASSERT_STR_CONTAINS(request_str, "PLAY");
    ASSERT_STR_CONTAINS(request_str, "CSeq: 4");
    ASSERT_STR_CONTAINS(request_str, "Session: ABC123");
    ASSERT_STR_CONTAINS(request_str, "Range: npt=0-");
}

void test_rtsp_request_factory_pause()
{
    auto pause_request = RTSPRequestFactory::CreatePause(5, "rtsp://example.com/stream").SetSession("ABC123").Build();

    std::string request_str = pause_request.ToString();

    ASSERT_STR_CONTAINS(request_str, "PAUSE");
    ASSERT_STR_CONTAINS(request_str, "CSeq: 5");
    ASSERT_STR_CONTAINS(request_str, "Session: ABC123");
}

void test_rtsp_request_factory_teardown()
{
    auto teardown_request =
        RTSPRequestFactory::CreateTeardown(6, "rtsp://example.com/stream").SetSession("ABC123").Build();

    std::string request_str = teardown_request.ToString();

    ASSERT_STR_CONTAINS(request_str, "TEARDOWN");
    ASSERT_STR_CONTAINS(request_str, "CSeq: 6");
    ASSERT_STR_CONTAINS(request_str, "Session: ABC123");
}

void test_rtsp_request_custom_headers()
{
    auto request = RTSPRequestBuilder()
                       .SetMethod("OPTIONS")
                       .SetUri("*")
                       .SetCSeq(7)
                       .SetUserAgent("TestClient/1.0")
                       .AddCustomHeader("X-Custom-Header: custom-value")
                       .AddCustomHeader("X-Another-Header: another-value")
                       .Build();

    std::string request_str = request.ToString();

    ASSERT_STR_CONTAINS(request_str, "User-Agent: TestClient/1.0");
    ASSERT_STR_CONTAINS(request_str, "X-Custom-Header: custom-value");
    ASSERT_STR_CONTAINS(request_str, "X-Another-Header: another-value");
}

void test_rtsp_request_with_body()
{
    std::string body_content = "test message body";
    auto request = RTSPRequestBuilder()
                       .SetMethod("SET_PARAMETER")
                       .SetUri("rtsp://example.com/stream")
                       .SetCSeq(8)
                       .SetContentType("text/parameters")
                       .SetMessageBody(body_content)
                       .Build();

    std::string request_str = request.ToString();

    ASSERT_STR_CONTAINS(request_str, "Content-Type: text/parameters");
    ASSERT_STR_CONTAINS(request_str, "Content-Length: " + std::to_string(body_content.length()));
    ASSERT_STR_CONTAINS(request_str, body_content);
}

void test_rtsp_request_large_cseq()
{
    auto request = RTSPRequestFactory::CreateOptions(999999, "rtsp://test.com").Build();
    std::string request_str = request.ToString();

    ASSERT_STR_CONTAINS(request_str, "CSeq: 999999");
}

void test_rtsp_request_all_methods()
{
    // Test all RTSP methods are supported
    std::vector<std::pair<std::string, std::function<RTSPRequestBuilder()>>> method_tests = {
        {"OPTIONS", []() { return RTSPRequestFactory::CreateOptions(1, "rtsp://test.com"); }},
        {"DESCRIBE", []() { return RTSPRequestFactory::CreateDescribe(2, "rtsp://test.com"); }},
        {"ANNOUNCE", []() { return RTSPRequestFactory::CreateAnnounce(3, "rtsp://test.com"); }},
        {"SETUP", []() { return RTSPRequestFactory::CreateSetup(4, "rtsp://test.com"); }},
        {"PLAY", []() { return RTSPRequestFactory::CreatePlay(5, "rtsp://test.com"); }},
        {"PAUSE", []() { return RTSPRequestFactory::CreatePause(6, "rtsp://test.com"); }},
        {"TEARDOWN", []() { return RTSPRequestFactory::CreateTeardown(7, "rtsp://test.com"); }},
        {"GET_PARAMETER", []() { return RTSPRequestFactory::CreateGetParameter(8, "rtsp://test.com"); }},
        {"SET_PARAMETER", []() { return RTSPRequestFactory::CreateSetParameter(9, "rtsp://test.com"); }},
        {"REDIRECT", []() { return RTSPRequestFactory::CreateRedirect(10, "rtsp://test.com"); }},
        {"RECORD", []() { return RTSPRequestFactory::CreateRecord(11, "rtsp://test.com"); }}};

    for (const auto &test : method_tests) {
        auto request = test.second().Build();
        std::string request_str = request.ToString();
        ASSERT_STR_CONTAINS(request_str, test.first);
    }
}

void test_rtsp_request_header_parsing()
{
    // Test RequestHeader::FromString
    std::string header_str = "Accept: application/sdp\r\n"
                             "User-Agent: MyRTSPClient/1.0\r\n"
                             "Authorization: Basic dXNlcjpwYXNz\r\n"
                             "Accept-Encoding: gzip, deflate\r\n"
                             "Custom-Header: custom-value\r\n";

    RequestHeader header = RequestHeader::FromString(header_str);

    ASSERT_TRUE(header.accept_.has_value());
    ASSERT_STR_EQ(*header.accept_, "application/sdp");

    ASSERT_TRUE(header.user_agent_.has_value());
    ASSERT_STR_EQ(*header.user_agent_, "MyRTSPClient/1.0");

    ASSERT_TRUE(header.authorization_.has_value());
    ASSERT_STR_EQ(*header.authorization_, "Basic dXNlcjpwYXNz");

    ASSERT_TRUE(header.accept_encoding_.has_value());
    ASSERT_STR_EQ(*header.accept_encoding_, "gzip, deflate");

    ASSERT_EQ(header.custom_header_.size(), 1);
    ASSERT_STR_EQ(header.custom_header_[0], "Custom-Header: custom-value");
}

void test_rtsp_request_full_parsing()
{
    // Test RTSPRequest::FromString with complete request
    std::string request_str = "DESCRIBE rtsp://example.com/stream RTSP/1.0\r\n"
                              "CSeq: 2\r\n"
                              "User-Agent: MyRTSPClient/1.0\r\n"
                              "Accept: application/sdp\r\n"
                              "Session: 12345678\r\n"
                              "Content-Type: text/parameters\r\n"
                              "Content-Length: 11\r\n"
                              "\r\n"
                              "param1: val";

    RTSPRequest request = RTSPRequest::FromString(request_str);

    // Check request line
    ASSERT_STR_EQ(request.method_, "DESCRIBE");
    ASSERT_STR_EQ(request.uri_, "rtsp://example.com/stream");
    ASSERT_STR_EQ(request.version_, "RTSP/1.0");

    // Check general headers
    ASSERT_STR_EQ(request.general_header_.at("CSeq"), "2");
    ASSERT_STR_EQ(request.general_header_.at("Session"), "12345678");

    // Check request headers
    ASSERT_TRUE(request.request_header_.user_agent_.has_value());
    ASSERT_STR_EQ(*request.request_header_.user_agent_, "MyRTSPClient/1.0");

    ASSERT_TRUE(request.request_header_.accept_.has_value());
    ASSERT_STR_EQ(*request.request_header_.accept_, "application/sdp");

    // Check entity headers
    ASSERT_STR_EQ(request.entity_header_.at("Content-Type"), "text/parameters");
    ASSERT_STR_EQ(request.entity_header_.at("Content-Length"), "11");

    // Check message body
    ASSERT_TRUE(request.message_body_.has_value());
    ASSERT_STR_EQ(*request.message_body_, "param1: val");
}

void test_rtsp_request_roundtrip()
{
    // Test round-trip: build -> toString -> parse -> toString
    auto original_request = RTSPRequestFactory::CreateDescribe(123, "rtsp://test.com/movie")
                                .SetAccept("application/sdp")
                                .SetUserAgent("TestAgent/1.0")
                                .SetAuthorization("Basic dGVzdA==")
                                .Build();

    std::string request_str = original_request.ToString();
    RTSPRequest parsed_request = RTSPRequest::FromString(request_str);
    std::string reparsed_str = parsed_request.ToString();

    // Check that key components are preserved
    ASSERT_STR_EQ(parsed_request.method_, "DESCRIBE");
    ASSERT_STR_EQ(parsed_request.uri_, "rtsp://test.com/movie");
    ASSERT_STR_EQ(parsed_request.version_, "RTSP/1.0");
    ASSERT_STR_EQ(parsed_request.general_header_.at("CSeq"), "123");

    ASSERT_TRUE(parsed_request.request_header_.accept_.has_value());
    ASSERT_STR_EQ(*parsed_request.request_header_.accept_, "application/sdp");

    ASSERT_TRUE(parsed_request.request_header_.user_agent_.has_value());
    ASSERT_STR_EQ(*parsed_request.request_header_.user_agent_, "TestAgent/1.0");

    ASSERT_TRUE(parsed_request.request_header_.authorization_.has_value());
    ASSERT_STR_EQ(*parsed_request.request_header_.authorization_, "Basic dGVzdA==");
}

void test_rtsp_request_malformed_parsing()
{
    // Test parsing of malformed requests

    // Test empty request
    RTSPRequest empty_request = RTSPRequest::FromString("");
    ASSERT_TRUE(empty_request.method_.empty());

    // Test invalid request
    RTSPRequest invalid_request = RTSPRequest::FromString("INVALID REQUEST");
    ASSERT_TRUE(invalid_request.method_.empty());

    // Test minimal valid request
    RTSPRequest minimal_request = RTSPRequest::FromString("OPTIONS * RTSP/1.0\r\n\r\n");
    ASSERT_STR_EQ(minimal_request.method_, "OPTIONS");
    ASSERT_STR_EQ(minimal_request.uri_, "*");
    ASSERT_STR_EQ(minimal_request.version_, "RTSP/1.0");
}

int main()
{
    TestSuite suite("RTSP Request Builder Tests");

    suite.AddTest("Basic Construction", test_rtsp_request_basic_construction);
    suite.AddTest("Factory OPTIONS", test_rtsp_request_factory_options);
    suite.AddTest("Factory DESCRIBE", test_rtsp_request_factory_describe);
    suite.AddTest("Factory SETUP", test_rtsp_request_factory_setup);
    suite.AddTest("Factory PLAY", test_rtsp_request_factory_play);
    suite.AddTest("Factory PAUSE", test_rtsp_request_factory_pause);
    suite.AddTest("Factory TEARDOWN", test_rtsp_request_factory_teardown);
    suite.AddTest("Custom Headers", test_rtsp_request_custom_headers);
    suite.AddTest("Request with Body", test_rtsp_request_with_body);
    suite.AddTest("Large CSeq", test_rtsp_request_large_cseq);
    suite.AddTest("All Methods", test_rtsp_request_all_methods);
    suite.AddTest("Header Parsing", test_rtsp_request_header_parsing);
    suite.AddTest("Full Request Parsing", test_rtsp_request_full_parsing);
    suite.AddTest("Round-trip Parsing", test_rtsp_request_roundtrip);
    suite.AddTest("Malformed Request Parsing", test_rtsp_request_malformed_parsing);

    bool success = suite.RunAll();
    return success ? 0 : 1;
}
