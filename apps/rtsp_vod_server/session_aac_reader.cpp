/**
 * @author SHAO Liming <lmshao@163.com>
 * @copyright Copyright (c) 2025 SHAO Liming
 * @license MIT
 *
 * SPDX-License-Identifier: MIT
 */

#include "session_aac_reader.h"

SessionAacReader::SessionAacReader(std::shared_ptr<lmshao::lmcore::MappedFile> mapped_file) : reader_(mapped_file) {}

bool SessionAacReader::ReadNextFrame(std::vector<uint8_t> &frame_data)
{
    return reader_.ReadNextFrame(frame_data);
}

void SessionAacReader::Reset()
{
    reader_.Reset();
}
