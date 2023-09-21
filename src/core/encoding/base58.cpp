/*
   Copyright 2023 Horizen Labs
   Distributed under the MIT software license, see the accompanying
   file COPYING or http://www.opensource.org/licenses/mit-license.php.
*/

#include "base58.hpp"

namespace zenpp::base58 {

/*
 * A note about the implementation
 * base58 encoding/decoding is generally slow due to the div and mod operations
 * Moreover, not constraining the input length to a max size, requires a high
 * number of reverse iterations across the output to repeatedly move the carry over
 * This implementation leverages multiplications first to convert the input into
 * a big integer value on which bitshift division are applied.
 */

// All alphanumeric characters except for "0", "I", "O", and "l" */
constexpr std::string_view kBase58Digits{"123456789ABCDEFGHJKLMNPQRSTUVWXYZabcdefghijkmnopqrstuvwxyz"};

// How many chars to append from Sha256 digest checksum
constexpr size_t kCheckSumLength{4};

tl::expected<std::string, EncodingError> encode(ByteView input) noexcept {
    if (input.empty()) return std::string{};

    // Convert byte sequence to an integer
    using namespace boost::multiprecision;
    cpp_int value{0};
    for (const auto byte : input) {
        value <<= 8;  // Mul by 256
        value += byte;
    }

    // Encode integer into base58
    std::string encoded{};
    encoded.reserve(input.size() * 138 / 100 + 1);  // 138% is the max ratio between input and output size
    while (value != 0) {
        const cpp_int rem = (value % 58);
        value /= 58;
        encoded.insert(encoded.begin(), kBase58Digits[rem.convert_to<std::string_view::size_type>()]);
    }

    // Add leading zeroes as 1s
    for (const auto byte : input) {
        if (byte not_eq 0x00) break;
        encoded.insert(encoded.begin(), '1');
    }

    return encoded;
}

tl::expected<std::string, EncodingError> encode_check(ByteView input) noexcept {
    Bytes buffer(input);
    crypto::Sha256 digest(buffer);
    const auto hash{digest.finalize()};
    buffer.append(hash.data(), kCheckSumLength);
    const auto ret{encode(buffer)};
    if (not ret) return tl::unexpected(ret.error());
    return *ret;
}

tl::expected<Bytes, DecodingError> decode(std::string_view input) noexcept {
    if (input.empty()) return {};
    using namespace boost::multiprecision;
    cpp_int value{0};

    // Convert base58 to BigInt
    for (auto chr : input) {
        const auto pos{kBase58Digits.find(chr)};
        if (pos == std::string::npos) [[unlikely]] {
            return tl::unexpected(DecodingError::kInvalidBase58Input);
        }
        value = value * 58 + pos;
    }

    // Convert BigInt into decoded bytes
    Bytes decoded{};
    while (value not_eq 0) {
        const cpp_int rem{value % 256};
        value /= 256;
        decoded.insert(decoded.begin(), rem.convert_to<uint8_t>());
    }

    // Add leading 1s as 0s
    for (auto chr : input) {
        if (chr not_eq '1') break;
        decoded.insert(decoded.begin(), 0x00);
    }

    return decoded;
}

tl::expected<Bytes, DecodingError> decode_check(std::string_view input) noexcept {
    const auto decoded{decode(input)};
    if (not decoded) return tl::unexpected(decoded.error());
    if (decoded.value().size() < kCheckSumLength) return tl::unexpected(DecodingError::kInputTooShort);

    // Split decoded into original value and its checksum
    const auto& decoded_value{decoded.value()};
    const ByteView original(decoded_value.data(), decoded_value.size() - kCheckSumLength);
    const ByteView checksum(&decoded_value[decoded_value.size() - kCheckSumLength], kCheckSumLength);

    // Recompute Digest256 from original and check it starts with checksum
    if (crypto::Sha256 digest{original}; not digest.finalize().starts_with(checksum)) {
        return tl::unexpected(DecodingError::kInvalidBase58Checksum);
    }
    return Bytes(original);
}
}  // namespace zenpp::base58
