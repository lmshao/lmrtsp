/**
 * @author SHAO Liming <lmshao@163.com>
 * @copyright Copyright (c) 2025 SHAO Liming
 * @license MIT
 *
 * SPDX-License-Identifier: MIT
 */

#include <cassert>
#include <iostream>

#include "rtsp/rtsp_request.h"

using namespace lmshao::rtsp;

void test_request_builder()
{
    std::cout << "=== RTSP Request Builder Examples ===\n\n";

    // Example 1: Simple DESCRIBE request
    auto describe_request = RTSPRequestFactory::CreateDescribe(1, "rtsp://example.com/stream")
                                .SetAccept("application/sdp")
                                .SetUserAgent("MyRTSPClient/1.0")
                                .Build();

    std::cout << "1. DESCRIBE Request:\n" << describe_request.ToString() << "\n";

    // Example 2: ANNOUNCE request with SDP
    std::string sdp_content = "v=0\r\n"
                              "o=- 1234567890 1234567890 IN IP4 192.168.1.100\r\n"
                              "s=Test Session\r\n";

    auto announce_request = RTSPRequestFactory::CreateAnnounce(2, "rtsp://example.com/publish")
                                .SetUserAgent("MyRTSPClient/1.0")
                                .SetSdp(sdp_content)
                                .Build();

    std::cout << "2. ANNOUNCE Request:\n" << announce_request.ToString() << "\n";

    // Example 3: SETUP request with transport
    auto setup_request = RTSPRequestFactory::CreateSetup(3, "rtsp://example.com/stream/track1")
                             .SetTransport("RTP/AVP/UDP;unicast;client_port=4588-4589")
                             .SetUserAgent("MyRTSPClient/1.0")
                             .Build();

    std::cout << "3. SETUP Request:\n" << setup_request.ToString() << "\n";

    // Example 4: PLAY request with session and range
    auto play_request = RTSPRequestFactory::CreatePlay(4, "rtsp://example.com/stream")
                            .SetSession("12345678")
                            .SetRange("npt=0-")
                            .SetUserAgent("MyRTSPClient/1.0")
                            .Build();

    std::cout << "4. PLAY Request:\n" << play_request.ToString() << "\n";

    // Example 5: GET_PARAMETER request with parameters
    auto get_param_request = RTSPRequestFactory::CreateGetParameter(5, "rtsp://example.com/stream")
                                 .SetSession("12345678")
                                 .SetParameters({"packets_received", "jitter", "packet_loss"})
                                 .SetUserAgent("MyRTSPClient/1.0")
                                 .Build();

    std::cout << "5. GET_PARAMETER Request:\n" << get_param_request.ToString() << "\n";

    // Example 6: SET_PARAMETER request with key-value parameters
    std::vector<std::pair<std::string, std::string>> params = {{"volume", "80"}, {"brightness", "50"}};
    auto set_param_request = RTSPRequestFactory::CreateSetParameter(6, "rtsp://example.com/stream")
                                 .SetSession("12345678")
                                 .SetParameters(params)
                                 .SetUserAgent("MyRTSPClient/1.0")
                                 .Build();

    std::cout << "6. SET_PARAMETER Request:\n" << set_param_request.ToString() << "\n";

    // Example 7: Custom request using RTSPRequestBuilder directly
    auto custom_request = RTSPRequestBuilder()
                              .SetMethod("OPTIONS")
                              .SetUri("*")
                              .SetCSeq(7)
                              .SetRequire("funky-feature")
                              .SetProxyRequire("gzipped-messages")
                              .SetUserAgent("MyRTSPClient/1.0")
                              .AddCustomHeader("X-Custom-Header: custom-value")
                              .Build();

    std::cout << "7. Custom OPTIONS Request:\n" << custom_request.ToString() << "\n";

    std::cout << "=== All Request Examples Completed ===\n\n";
}

int main()
{
    test_request_builder();
    return 0;
}
