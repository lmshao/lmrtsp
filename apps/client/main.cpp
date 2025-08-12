/**
 * @author SHAO Liming <lmshao@163.com>
 * @copyright Copyright (c) 2025 SHAO Liming
 * @license MIT
 *
 * SPDX-License-Identifier: MIT
 */

#include <iostream>

#include "rtsp/rtsp_request.h"
#include "rtsp/rtsp_response.h"

using namespace lmshao::rtsp;

int main()
{
    std::cout << "RTSP Client (placeholder implementation)" << std::endl;

    auto options_req =
        RTSPRequestFactory::CreateOptions(1, "rtsp://example.com/stream").SetUserAgent("RTSPClient/1.0").Build();

    std::cout << "Example OPTIONS request:" << std::endl;
    std::cout << options_req.ToString() << std::endl;

    auto ok_resp = RTSPResponseFactory::CreateOK(1).SetServer("RTSPServer/1.0").Build();

    std::cout << "Example OK response:" << std::endl;
    std::cout << ok_resp.ToString() << std::endl;

    return 0;
}
