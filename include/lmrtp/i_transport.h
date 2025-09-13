/**
 * @author SHAO Liming <lmshao@163.com>
 * @copyright Copyright (c) 2025 SHAO Liming
 * @license MIT
 *
 * SPDX-License-Identifier: MIT
 */

#ifndef LMSHAO_LMRTP_I_TRANSPORT_H
#define LMSHAO_LMRTP_I_TRANSPORT_H

#include <cstdint>
#include <string>
#include <vector>

namespace lmshao::lmrtp {

class ITransport {
public:
    virtual ~ITransport() = default;

    virtual bool Init(const std::string &ip, uint16_t port) = 0;
    virtual bool Send(const uint8_t *data, size_t len) = 0;
    virtual void Close() = 0;
};

} // namespace lmshao::lmrtp

#endif // LMSHAO_LMRTP_I_TRANSPORT_H