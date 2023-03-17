/*
   Copyright 2023 Horizen Labs
   Distributed under the MIT software license, see the accompanying
   file COPYING or http://www.opensource.org/licenses/mit-license.php.
*/

#include <memory>

#include <openssl/bio.h>
#include <openssl/evp.h>

#include <zen/core/common/cast.hpp>
#include <zen/core/encoding/base64.hpp>

namespace zen::base64 {

// Inspired by https://stackoverflow.com/questions/5288076/base64-encoding-and-decoding-with-openssl

namespace {
    struct BIOFreeAll {
        void operator()(BIO* p) const { BIO_free_all(p); }
    };
}  // namespace

tl::expected<std::string, EncodingError> encode(ByteView bytes) noexcept {
    if (bytes.empty()) return std::string{};
    if (bytes.length() > (std::numeric_limits<std::string::size_type>::max() / 4U) * 3U) {
        return tl::unexpected(EncodingError::kInputTooLong);
    }

    std::unique_ptr<BIO, BIOFreeAll> b64(BIO_new(BIO_f_base64()));
    BIO_set_flags(b64.get(), BIO_FLAGS_BASE64_NO_NL);  // No new line

    BIO* sink = BIO_new(BIO_s_mem());
    BIO_push(b64.get(), sink);

    while (BIO_write(b64.get(), bytes.data(), static_cast<int>(bytes.length())) <= 0) {
        if (BIO_should_retry(b64.get())) continue;
        return tl::unexpected(EncodingError::kUnexpectedError);
    }

    BIO_flush(b64.get());
    const char* encoded;
    const long encoded_len{BIO_get_mem_data(sink, &encoded)};
    return std::string(encoded, static_cast<std::string::size_type>(encoded_len));
}

tl::expected<std::string, EncodingError> encode(std::string_view data) noexcept {
    return encode(zen::string_view_to_byte_view(data));
}

tl::expected<Bytes, DecodingError> decode(std::string_view input) noexcept {
    if (input.empty()) return Bytes();
    std::unique_ptr<BIO, BIOFreeAll> b64(BIO_new(BIO_f_base64()));
    BIO_set_flags(b64.get(), BIO_FLAGS_BASE64_NO_NL);
    BIO* source = BIO_new_mem_buf(input.data(), -1);
    BIO_push(b64.get(), source);
    const auto maxlen{input.size() / 4 * 3 + 1};
    Bytes ret(maxlen, '\0');
    const auto effective_len{BIO_read(b64.get(), ret.data(), static_cast<int>(maxlen))};
    if (effective_len <= 0) return tl::unexpected(DecodingError::kInvalidBase64Input);
    ret.resize(static_cast<std::string::size_type>(effective_len));
    return ret;
}

}  // namespace zen::base64
