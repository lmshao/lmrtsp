/**
 * @author SHAO Liming <lmshao@163.com>
 * @copyright Copyright (c) 2025 SHAO Liming
 * @license MIT
 *
 * SPDX-License-Identifier: MIT
 */

#include <string>

#include "rtsp/rtsp_request.h"
#include "rtsp/rtsp_response.h"
#include "test_framework.h"

using namespace test_framework;
using namespace lmshao::rtsp;

void test_complete_rtsp_session_flow()
{
    // Simulate a complete RTSP session flow
    std::string stream_url = "rtsp://example.com/test.mp4";
    std::string session_id = "ABCD1234";

    // 1. OPTIONS request/response
    auto options_req = RTSPRequestFactory::CreateOptions(1, stream_url).Build();
    auto options_resp = RTSPResponseFactory::CreateOptionsOK(1).SetServer("TestServer/1.0").Build();

    ASSERT_STR_CONTAINS(options_req.ToString(), "OPTIONS");
    ASSERT_STR_CONTAINS(options_resp.ToString(), "Public:");

    // 2. DESCRIBE request/response
    auto describe_req = RTSPRequestFactory::CreateDescribe(2, stream_url).SetAccept("application/sdp").Build();
    auto describe_resp = RTSPResponseFactory::CreateDescribeOK(2)
                             .SetServer("TestServer/1.0")
                             .SetSdp("v=0\r\no=- 123 456 IN IP4 192.168.1.1\r\ns=Test Stream\r\n")
                             .Build();

    ASSERT_STR_CONTAINS(describe_req.ToString(), "DESCRIBE");
    ASSERT_STR_CONTAINS(describe_req.ToString(), "Accept: application/sdp");
    ASSERT_STR_CONTAINS(describe_resp.ToString(), "Content-Type: application/sdp");

    // 3. SETUP request/response
    auto setup_req = RTSPRequestFactory::CreateSetup(3, stream_url + "/track1")
                         .SetTransport("RTP/AVP/UDP;unicast;client_port=4588-4589")
                         .Build();
    auto setup_resp = RTSPResponseFactory::CreateSetupOK(3)
                          .SetSession(session_id)
                          .SetTransport("RTP/AVP/UDP;unicast;client_port=4588-4589;server_port=6256-6257")
                          .Build();

    ASSERT_STR_CONTAINS(setup_req.ToString(), "SETUP");
    ASSERT_STR_CONTAINS(setup_req.ToString(), "Transport:");
    ASSERT_STR_CONTAINS(setup_resp.ToString(), "Session: " + session_id);

    // 4. PLAY request/response
    auto play_req = RTSPRequestFactory::CreatePlay(4, stream_url).SetSession(session_id).SetRange("npt=0-").Build();
    auto play_resp = RTSPResponseFactory::CreatePlayOK(4)
                         .SetSession(session_id)
                         .SetRange("npt=0-")
                         .SetRTPInfo("url=" + stream_url + "/track1;seq=45102;rtptime=2890844526")
                         .Build();

    ASSERT_STR_CONTAINS(play_req.ToString(), "PLAY");
    ASSERT_STR_CONTAINS(play_req.ToString(), "Session: " + session_id);
    ASSERT_STR_CONTAINS(play_resp.ToString(), "RTP-Info:");

    // 5. PAUSE request/response
    auto pause_req = RTSPRequestFactory::CreatePause(5, stream_url).SetSession(session_id).Build();
    auto pause_resp = RTSPResponseFactory::CreateOK(5).SetSession(session_id).Build();

    ASSERT_STR_CONTAINS(pause_req.ToString(), "PAUSE");
    ASSERT_STR_CONTAINS(pause_resp.ToString(), "Session: " + session_id);

    // 6. TEARDOWN request/response
    auto teardown_req = RTSPRequestFactory::CreateTeardown(6, stream_url).SetSession(session_id).Build();
    auto teardown_resp = RTSPResponseFactory::CreateOK(6).SetSession(session_id).Build();

    ASSERT_STR_CONTAINS(teardown_req.ToString(), "TEARDOWN");
    ASSERT_STR_CONTAINS(teardown_resp.ToString(), "Session: " + session_id);
}

void test_error_handling_scenarios()
{
    // Test various error scenarios

    // 1. Unauthorized access
    auto auth_resp = RTSPResponseFactory::CreateUnauthorized(1)
                         .SetWWWAuthenticate("Digest realm=\"TestServer\", nonce=\"abc123\"")
                         .Build();

    ASSERT_STR_CONTAINS(auth_resp.ToString(), "401 Unauthorized");
    ASSERT_STR_CONTAINS(auth_resp.ToString(), "WWW-Authenticate:");

    // 2. Not Found
    auto not_found_resp = RTSPResponseFactory::CreateNotFound(2).SetServer("TestServer/1.0").Build();

    ASSERT_STR_CONTAINS(not_found_resp.ToString(), "404 Not Found");

    // 3. Session Not Found
    auto session_not_found_resp = RTSPResponseFactory::CreateSessionNotFound(3).SetServer("TestServer/1.0").Build();

    ASSERT_STR_CONTAINS(session_not_found_resp.ToString(), "454 Session Not Found");

    // 4. Internal Server Error
    auto server_error_resp = RTSPResponseFactory::CreateInternalServerError(4).SetServer("TestServer/1.0").Build();

    ASSERT_STR_CONTAINS(server_error_resp.ToString(), "500 Internal Server Error");
}

void test_advanced_features()
{
    // Test advanced RTSP features

    // 1. ANNOUNCE request with SDP body
    std::string sdp_body = "v=0\r\no=- 1234567890 1234567890 IN IP4 192.168.1.100\r\ns=Test Session\r\n";
    auto announce_req = RTSPRequestFactory::CreateAnnounce(1, "rtsp://example.com/publish")
                            .SetContentType("application/sdp")
                            .SetMessageBody(sdp_body)
                            .Build();

    ASSERT_STR_CONTAINS(announce_req.ToString(), "ANNOUNCE");
    ASSERT_STR_CONTAINS(announce_req.ToString(), "Content-Type: application/sdp");
    ASSERT_STR_CONTAINS(announce_req.ToString(), "Content-Length: " + std::to_string(sdp_body.length()));
    ASSERT_STR_CONTAINS(announce_req.ToString(), sdp_body);

    // 2. GET_PARAMETER request
    std::string param_body = "position\r\nvolume\r\n";
    auto get_param_req = RTSPRequestFactory::CreateGetParameter(2, "rtsp://example.com/stream")
                             .SetSession("ABC123")
                             .SetContentType("text/parameters")
                             .SetMessageBody(param_body)
                             .Build();

    ASSERT_STR_CONTAINS(get_param_req.ToString(), "GET_PARAMETER");
    ASSERT_STR_CONTAINS(get_param_req.ToString(), "Session: ABC123");
    ASSERT_STR_CONTAINS(get_param_req.ToString(), param_body);

    // 3. SET_PARAMETER request
    std::string set_param_body = "volume: 80\r\nbrightness: 50\r\n";
    auto set_param_req = RTSPRequestFactory::CreateSetParameter(3, "rtsp://example.com/stream")
                             .SetSession("ABC123")
                             .SetContentType("text/parameters")
                             .SetMessageBody(set_param_body)
                             .Build();

    ASSERT_STR_CONTAINS(set_param_req.ToString(), "SET_PARAMETER");
    ASSERT_STR_CONTAINS(set_param_req.ToString(), set_param_body);

    // 4. Custom headers
    auto custom_req = RTSPRequestBuilder()
                          .SetMethod("OPTIONS")
                          .SetUri("*")
                          .SetCSeq(4)
                          .SetUserAgent("CustomClient/2.0")
                          .AddCustomHeader("X-Session-ID: custom-session")
                          .AddCustomHeader("X-Client-Version: 2.0.1")
                          .Build();

    ASSERT_STR_CONTAINS(custom_req.ToString(), "User-Agent: CustomClient/2.0");
    ASSERT_STR_CONTAINS(custom_req.ToString(), "X-Session-ID: custom-session");
    ASSERT_STR_CONTAINS(custom_req.ToString(), "X-Client-Version: 2.0.1");
}

void test_builder_pattern_validation()
{
    // Test that builder pattern works correctly with various combinations

    // 1. Request with all possible headers
    auto complex_req = RTSPRequestBuilder()
                           .SetMethod("PLAY")
                           .SetUri("rtsp://complex.example.com/stream")
                           .SetCSeq(100)
                           .SetUserAgent("ComplexClient/1.0")
                           .SetAuthorization("Basic dXNlcjpwYXNz")
                           .SetSession("COMPLEX123")
                           .SetRange("npt=30-60")
                           .SetTransport("RTP/AVP/UDP;unicast;client_port=8000-8001")
                           .SetAccept("application/sdp")
                           .AddCustomHeader("X-Debug: enabled")
                           .SetContentType("text/plain")
                           .SetMessageBody("test body")
                           .Build();

    std::string req_str = complex_req.ToString();
    ASSERT_STR_CONTAINS(req_str, "PLAY rtsp://complex.example.com/stream RTSP/1.0");
    ASSERT_STR_CONTAINS(req_str, "CSeq: 100");
    ASSERT_STR_CONTAINS(req_str, "User-Agent: ComplexClient/1.0");
    ASSERT_STR_CONTAINS(req_str, "Authorization: Basic dXNlcjpwYXNz");
    ASSERT_STR_CONTAINS(req_str, "Session: COMPLEX123");
    ASSERT_STR_CONTAINS(req_str, "Range: npt=30-60");
    ASSERT_STR_CONTAINS(req_str, "X-Debug: enabled");
    ASSERT_STR_CONTAINS(req_str, "test body");

    // 2. Response with all possible headers
    auto complex_resp = RTSPResponseBuilder()
                            .SetStatus(StatusCode::OK)
                            .SetCSeq(200)
                            .SetServer("ComplexServer/2.0")
                            .SetSession("RESPONSE456")
                            .SetTransport("RTP/AVP/UDP;unicast;server_port=9000-9001")
                            .SetRange("npt=0-")
                            .SetRTPInfo("url=rtsp://example.com/track1;seq=12345")
                            .AddCustomHeader("X-Stream-Quality: HD")
                            .SetContentType("application/json")
                            .SetMessageBody("{\"status\": \"playing\"}")
                            .Build();

    std::string resp_str = complex_resp.ToString();
    ASSERT_STR_CONTAINS(resp_str, "RTSP/1.0 200 OK");
    ASSERT_STR_CONTAINS(resp_str, "CSeq: 200");
    ASSERT_STR_CONTAINS(resp_str, "Server: ComplexServer/2.0");
    ASSERT_STR_CONTAINS(resp_str, "Session: RESPONSE456");
    ASSERT_STR_CONTAINS(resp_str, "X-Stream-Quality: HD");
    ASSERT_STR_CONTAINS(resp_str, "{\"status\": \"playing\"}");
}

void test_edge_cases()
{
    // Test edge cases and boundary conditions

    // 1. Very large CSeq number
    auto large_cseq_req = RTSPRequestFactory::CreateOptions(999999, "rtsp://test.com").Build();
    ASSERT_STR_CONTAINS(large_cseq_req.ToString(), "CSeq: 999999");

    // 2. Empty message body (should still include Content-Length: 0)
    auto empty_body_req =
        RTSPRequestBuilder().SetMethod("OPTIONS").SetUri("rtsp://test.com").SetCSeq(1).SetMessageBody("").Build();

    std::string empty_str = empty_body_req.ToString();
    ASSERT_STR_CONTAINS(empty_str, "Content-Length: 0");

    // 3. Special characters in URI
    auto special_uri_req =
        RTSPRequestFactory::CreateDescribe(1, "rtsp://test.com/path%20with%20spaces/file.mp4").Build();
    ASSERT_STR_CONTAINS(special_uri_req.ToString(), "rtsp://test.com/path%20with%20spaces/file.mp4");

    // 4. Multiple custom headers
    auto multi_header_req = RTSPRequestBuilder()
                                .SetMethod("SETUP")
                                .SetUri("rtsp://test.com")
                                .SetCSeq(1)
                                .AddCustomHeader("X-Header-1: value1")
                                .AddCustomHeader("X-Header-2: value2")
                                .AddCustomHeader("X-Header-3: value3")
                                .Build();

    std::string multi_str = multi_header_req.ToString();
    ASSERT_STR_CONTAINS(multi_str, "X-Header-1: value1");
    ASSERT_STR_CONTAINS(multi_str, "X-Header-2: value2");
    ASSERT_STR_CONTAINS(multi_str, "X-Header-3: value3");
}

int main()
{
    TestSuite suite("RTSP Integration Tests");

    suite.AddTest("Complete RTSP Session Flow", test_complete_rtsp_session_flow);
    suite.AddTest("Error Handling Scenarios", test_error_handling_scenarios);
    suite.AddTest("Advanced Features", test_advanced_features);
    suite.AddTest("Builder Pattern Validation", test_builder_pattern_validation);
    suite.AddTest("Edge Cases", test_edge_cases);

    bool success = suite.RunAll();
    return success ? 0 : 1;
}
