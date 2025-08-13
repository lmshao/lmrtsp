/**
 * @author SHAO Liming <lmshao@163.com>
 * @copyright Copyright (c) 2025 SHAO Liming
 * @license MIT
 *
 * SPDX-License-Identifier: MIT
 */

#include <iostream>

#include "rtsp_log.h"

int main()
{
    RTSP_LOGD("RTSP server started");
    RTSP_LOGE("RTSP server encountered an error: %s", "demo error");
    return 0;
}
