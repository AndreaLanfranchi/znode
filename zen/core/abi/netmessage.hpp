/*
   Copyright 2009-2010 Satoshi Nakamoto
   Copyright 2009-2013 The Bitcoin Core developers
   Copyright 2023 Horizen Labs
   Distributed under the MIT software license, see the accompanying
   file COPYING or http://www.opensource.org/licenses/mit-license.php.
*/

#pragma once

#include <optional>

#include <zen/core/common/base.hpp>
#include <zen/core/serialization/serializable.hpp>

namespace zen {

static constexpr uint32_t kMaxProtocolMessageLength{static_cast<uint32_t>(4_MiB)};  // Maximum length of a protocol message

class NetMessageHeader : public serialization::Serializable {
  public:
    NetMessageHeader() : Serializable(){};

    uint32_t magic{0};                   // Message magic (origin network)
    std::array<uint8_t, 12> command{0};  // ASCII string identifying the packet content, NULL padded (non-NULL padding
                                         // results in packet rejected)
    uint32_t length{0};                  // Length of payload in bytes
    uint32_t checksum{0};                // First 4 bytes of sha256(sha256(payload)) in internal byte order

    void reset() noexcept;

    [[nodiscard]] bool is_valid(
        std::optional<uint32_t> expected_magic) const noexcept;  // Whether the message is validly formatted

  private:
    friend class serialization::Archive;
    serialization::Error serialization(serialization::Archive& archive, serialization::Action action) override;
};
}  // namespace zen
