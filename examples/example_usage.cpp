/**
 * @author SHAO Liming <lmshao@163.com>
 * @copyright Copyright (c) 2025 SHAO Liming
 * @license MIT
 *
 * SPDX-License-Identifier: MIT
 */

#include <iostream>

#include "rtsp/rtsp_request.h"

using namespace lmshao::rtsp;

int main()
{
    // Old way (before builder pattern):
    // RequestDescribe req_desc(1, "rtsp://example.com/stream");
    // req_desc.SetAccept("application/sdp");

    // New way (with builder pattern):

    // Example 1: Simple DESCRIBE request
    auto describe_request = RTSPRequestFactory::CreateDescribe(1, "rtsp://example.com/stream")
                                .SetAccept("application/sdp")
                                .SetUserAgent("MyRTSPClient/1.0")
                                .Build();

    std::cout << "DESCRIBE Request:\n" << describe_request.ToString() << "\n\n";

    // Example 2: ANNOUNCE request with SDP
    std::string sdp_content = "v=0\r\no=- 1234567890 1234567890 IN IP4 192.168.1.100\r\ns=Test Session\r\n";
    auto announce_request = RTSPRequestFactory::CreateAnnounce(2, "rtsp://example.com/publish")
                                .SetUserAgent("MyRTSPClient/1.0")
                                .SetSdp(sdp_content)
                                .Build();

    std::cout << "ANNOUNCE Request:\n" << announce_request.ToString() << "\n\n";

    // Example 3: SETUP request with transport
    auto setup_request = RTSPRequestFactory::CreateSetup(3, "rtsp://example.com/stream/track1")
                             .SetTransport("RTP/AVP/UDP;unicast;client_port=4588-4589")
                             .SetUserAgent("MyRTSPClient/1.0")
                             .Build();

    std::cout << "SETUP Request:\n" << setup_request.ToString() << "\n\n";

    // Example 4: PLAY request with session and range
    auto play_request = RTSPRequestFactory::CreatePlay(4, "rtsp://example.com/stream")
                            .SetSession("12345678")
                            .SetRange("npt=0-")
                            .SetUserAgent("MyRTSPClient/1.0")
                            .Build();

    std::cout << "PLAY Request:\n" << play_request.ToString() << "\n\n";

    // Example 5: GET_PARAMETER request with parameters
    auto get_param_request = RTSPRequestFactory::CreateGetParameter(5, "rtsp://example.com/stream")
                                 .SetSession("12345678")
                                 .SetParameters({"packets_received", "jitter", "packet_loss"})
                                 .SetUserAgent("MyRTSPClient/1.0")
                                 .Build();

    std::cout << "GET_PARAMETER Request:\n" << get_param_request.ToString() << "\n\n";

    // Example 6: SET_PARAMETER request with key-value parameters
    std::vector<std::pair<std::string, std::string>> params = {{"volume", "80"}, {"brightness", "50"}};
    auto set_param_request = RTSPRequestFactory::CreateSetParameter(6, "rtsp://example.com/stream")
                                 .SetSession("12345678")
                                 .SetParameters(params)
                                 .SetUserAgent("MyRTSPClient/1.0")
                                 .Build();

    std::cout << "SET_PARAMETER Request:\n" << set_param_request.ToString() << "\n\n";

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

    std::cout << "Custom OPTIONS Request:\n" << custom_request.ToString() << "\n\n";

    return 0;
}

/*
Expected output:

DESCRIBE Request:
DESCRIBE rtsp://example.com/stream RTSP/1.0
CSeq: 1
Accept: application/sdp
User-Agent: MyRTSPClient/1.0


ANNOUNCE Request:
ANNOUNCE rtsp://example.com/publish RTSP/1.0
CSeq: 2
User-Agent: MyRTSPClient/1.0
Content-Type: application/sdp
Content-Length: 85

v=0
o=- 1234567890 1234567890 IN IP4 192.168.1.100
s=Test Session


SETUP Request:
SETUP rtsp://example.com/stream/track1 RTSP/1.0
CSeq: 3
Transport: RTP/AVP/UDP;unicast;client_port=4588-4589
User-Agent: MyRTSPClient/1.0


PLAY Request:
PLAY rtsp://example.com/stream RTSP/1.0
CSeq: 4
Session: 12345678
Range: npt=0-
User-Agent: MyRTSPClient/1.0


GET_PARAMETER Request:
GET_PARAMETER rtsp://example.com/stream RTSP/1.0
CSeq: 5
Session: 12345678
User-Agent: MyRTSPClient/1.0
Content-Type: text/parameters
Content-Length: 34

packets_received
jitter
packet_loss


SET_PARAMETER Request:
SET_PARAMETER rtsp://example.com/stream RTSP/1.0
CSeq: 6
Session: 12345678
User-Agent: MyRTSPClient/1.0
Content-Type: text/parameters
Content-Length: 22

volume: 80
brightness: 50


Custom OPTIONS Request:
OPTIONS * RTSP/1.0
CSeq: 7
Require: funky-feature
Proxy-Require: gzipped-messages
User-Agent: MyRTSPClient/1.0
X-Custom-Header: custom-value

*/
