//
// Created by Andrea on 19/05/2023.
//

#include "base58.hpp"

#include <zen/core/common/secure_bytes.hpp>

namespace zen::base58 {

tl::expected<std::string, EncodingError> encode(zen::ByteView bytes) noexcept {
    // All alphanumeric characters except for "0", "I", "O", and "l" */
    static const char kBase58Digits[]{"123456789ABCDEFGHJKLMNPQRSTUVWXYZabcdefghijkmnopqrstuvwxyz"};

    if (bytes.empty()) return std::string{};
    const size_t encoded_size{(bytes.size() * 138 / 100) + 1};

    bool is_initial{true};
    uint32_t skipped_leading_zeroes_count{0};
    intx::uint<2048> value{0};  // Should be enough - at least for tests
    for (const auto byte : bytes) {
        value = value * 256 + byte;
        if (is_initial) {
            if (byte == 0x00) {
                ++skipped_leading_zeroes_count;
            } else {
                is_initial = false;
            }
        }
    }

    SecureBytes buffer(encoded_size, 0x00);
    SecureBytes::size_type index{encoded_size - 1};
    while (value != 0) {
        auto rem = static_cast<uint8_t>(value % 58);
        value /= 58;
        buffer[index--] = rem;
    }

    std::string encoded;
    encoded.reserve(encoded_size);
    encoded.assign(skipped_leading_zeroes_count, '1');

    is_initial = true;  // To skip initial zeroes
    for (const auto byte : buffer) {
        if (is_initial) {
            if (byte == 0x00) continue;
            is_initial = false;
        }
        encoded.push_back(kBase58Digits[byte]);
    }

    return encoded;
}

}  // namespace zen::base58
