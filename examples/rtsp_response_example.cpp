/**
 * RTSP Response Builder Example
 *
 * This example demonstrates how to use the RTSP Response Builder pattern
 * to create various types of RTSP responses.
 */

#include <cassert>
#include <iostream>

#include "rtsp/rtsp_response.h"

using namespace lmshao::rtsp;

void test_response_builder()
{
    std::cout << "=== RTSP Response Builder Examples ===\n\n";

    // Example 1: Simple OPTIONS response
    auto options_response = RTSPResponseFactory::CreateOptionsOK(1).SetServer("MyRTSPServer/1.0").Build();

    std::cout << "1. OPTIONS Response:\n" << options_response.ToString() << "\n";

    // Example 2: DESCRIBE response with SDP
    std::string sdp_content = "v=0\r\n"
                              "o=- 1234567890 1234567890 IN IP4 192.168.1.100\r\n"
                              "s=Test Stream\r\n"
                              "c=IN IP4 192.168.1.100\r\n"
                              "t=0 0\r\n"
                              "m=video 0 RTP/AVP 96\r\n"
                              "a=rtpmap:96 H264/90000\r\n"
                              "a=control:track1\r\n";

    auto describe_response =
        RTSPResponseFactory::CreateDescribeOK(2).SetServer("MyRTSPServer/1.0").SetSdp(sdp_content).Build();

    std::cout << "2. DESCRIBE Response:\n" << describe_response.ToString() << "\n";

    // Example 3: SETUP response with session and transport
    auto setup_response = RTSPResponseFactory::CreateSetupOK(3)
                              .SetServer("MyRTSPServer/1.0")
                              .SetSession("A5B4C3D2")
                              .SetTransport("RTP/AVP/UDP;unicast;client_port=4588-4589;server_port=6256-6257")
                              .Build();

    std::cout << "3. SETUP Response:\n" << setup_response.ToString() << "\n";

    // Example 4: PLAY response with RTP-Info
    auto play_response = RTSPResponseFactory::CreatePlayOK(4)
                             .SetServer("MyRTSPServer/1.0")
                             .SetSession("A5B4C3D2")
                             .SetRange("npt=0-")
                             .SetRTPInfo("url=rtsp://example.com/stream/track1;seq=45102;rtptime=2890844526")
                             .Build();

    std::cout << "4. PLAY Response:\n" << play_response.ToString() << "\n";

    // Example 5: Error response - Unauthorized
    auto unauthorized_response =
        RTSPResponseFactory::CreateUnauthorized(5)
            .SetServer("MyRTSPServer/1.0")
            .SetWWWAuthenticate("Digest realm=\"MyRTSPServer\", nonce=\"b64token\", algorithm=\"MD5\"")
            .Build();

    std::cout << "5. Unauthorized Response:\n" << unauthorized_response.ToString() << "\n";

    // Example 6: Error response - Session Not Found
    auto session_not_found = RTSPResponseFactory::CreateSessionNotFound(6).SetServer("MyRTSPServer/1.0").Build();

    std::cout << "6. Session Not Found Response:\n" << session_not_found.ToString() << "\n";

    // Example 7: Custom response using RTSPResponseBuilder directly
    auto custom_response = RTSPResponseBuilder()
                               .SetStatus(StatusCode::Created)
                               .SetCSeq(7)
                               .SetServer("MyRTSPServer/1.0")
                               .SetLocation("rtsp://example.com/stream/record.sdp")
                               .AddCustomHeader("X-Custom-Header: custom-value")
                               .Build();

    std::cout << "7. Custom Response:\n" << custom_response.ToString() << "\n";

    // Example 8: Response with message body (non-SDP)
    auto parameter_response = RTSPResponseFactory::CreateOK(8)
                                  .SetServer("MyRTSPServer/1.0")
                                  .SetContentType("text/parameters")
                                  .SetMessageBody("packets_received: 10000\r\njitter: 0.01\r\npacket_loss: 0")
                                  .Build();

    std::cout << "8. Parameter Response:\n" << parameter_response.ToString() << "\n";

    std::cout << "=== All Response Examples Completed ===\n\n";
}

int main()
{
    test_response_builder();
    return 0;
}
