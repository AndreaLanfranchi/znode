/*
   Copyright 2023 Horizen Labs
   Distributed under the MIT software license, see the accompanying
   file COPYING or http://www.opensource.org/licenses/mit-license.php.
*/

#include <core/encoding/base58.hpp>

namespace zenpp::base58 {

/*
 * A note about the implementation
 * base58 encoding/decoding is generally slow due to the div and mod operations
 * Moreover, not constraining the input length to a max size, requires a high
 * number of reverse iterations across the output to repeatedly move the carry over
 * This implementation leverages multiplications first to convert the input into
 * a big integer value on which bitshift division are applied.
 * This is faster however carries a limitation: the input can't be arbitrary long
 * rather is limited by the size of the integer.
 * Adjust the following value to a more appropriate if you experience
 * input too long errors
 */
constexpr size_t kBigintSizeInBits{3072};  // MUST be a multiple of 8

// All alphanumeric characters except for "0", "I", "O", and "l" */
constexpr std::string_view kBase58Digits{"123456789ABCDEFGHJKLMNPQRSTUVWXYZabcdefghijkmnopqrstuvwxyz"};

// How many chars to append from Sha256 digest checksum
constexpr size_t kCheckSumLength{4};

tl::expected<std::string, EncodingError> encode(ByteView input) noexcept {
    static ZEN_THREAD_LOCAL intx::uint<kBigintSizeInBits> value{0};  // Should be enough - at least for tests

    if (input.empty()) return std::string{};
    const size_t encoded_size{(input.size() * 138 / 100) + 1};

    // Convert to BigInt
    bool is_initial{true};
    uint32_t skipped_leading_zeroes_count{0};
    value = 0;
    for (const auto byte : input) {
        if (is_initial) {
            if (byte == 0x00) {
                ++skipped_leading_zeroes_count;
                continue;
            }
            is_initial = false;
        }
        value <<= 8;  // Mul by 256
        value += byte;
    }
    if (intx::count_significant_words(value) == intx::uint<kBigintSizeInBits>::num_words) {
        return tl::unexpected(EncodingError::kInputTooLong);
    }

    Bytes buffer(encoded_size, 0x00);
    Bytes::size_type index{encoded_size - 1};
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
    static ZEN_THREAD_LOCAL intx::uint<kBigintSizeInBits> value{0};  // Should be enough - at least for tests

    if (input.empty()) return {};

    // Convert base58 to BigInt
    bool is_initial{true};
    uint32_t skipped_leading_ones_count{0};
    value = 0;
    for (const auto c : input) {
        if (is_initial) {
            if (c == '1') {
                ++skipped_leading_ones_count;
                continue;
            }
            is_initial = false;
        }
        const auto d{kBase58Digits.find(c)};
        if (d == std::string::npos) {
            return tl::unexpected(DecodingError::kInvalidBase58Input);
        }
        value = value * 58 + d;
    }

    // Convert BigInt into decoded bytes
    // The exact number of bytes to allocate for the decoded buffer
    // corresponds to how many times we can divide the obtained value
    // by 256. As each division by 256 is a right shift by 8 bits (1 byte)
    // we can derive that (bits(value) - clz(value)) / 8 == number of divisions
    // required to reduce value to zero.
    // At this value we must add the number of bytes to be added by the initial '1's
    const auto number_of_divisions{intx::count_significant_bytes(value)};
    Bytes decoded(number_of_divisions + skipped_leading_ones_count, 0);
    Bytes::size_type index(decoded.size() - 1);
    while (value not_eq 0) {
        auto b{static_cast<uint8_t>(value & 0xff)};
        decoded[index--] = b;
        value >>= 8;
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
