/*
   Copyright 2023 The Znode Authors

   Licensed under the Apache License, Version 2.0 (the "License");
   you may not use this file except in compliance with the License.
   You may obtain a copy of the License at

       http://www.apache.org/licenses/LICENSE-2.0

   Unless required by applicable law or agreed to in writing, software
   distributed under the License is distributed on an "AS IS" BASIS,
   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
   See the License for the specific language governing permissions and
   limitations under the License.
*/

#include "base64.hpp"

#include <memory>

#include <openssl/bio.h>
#include <openssl/evp.h>

#include <core/common/cast.hpp>

namespace znode::enc::base64 {

// Inspired by https://stackoverflow.com/questions/5288076/base64-encoding-and-decoding-with-openssl

namespace {
    struct BIOFreeAll {
        void operator()(BIO* p) const { BIO_free_all(p); }
    };
}  // namespace

outcome::result<std::string> encode(ByteView bytes) noexcept {
    if (bytes.empty()) return std::string{};
    if (bytes.length() > (std::numeric_limits<std::string::size_type>::max() / 4U) * 3U) {
        return Error::kInputTooLarge;
    }

    const std::unique_ptr<BIO, BIOFreeAll> b64(BIO_new(BIO_f_base64()));
    BIO_set_flags(b64.get(), BIO_FLAGS_BASE64_NO_NL);  // No new line

    BIO* sink = BIO_new(BIO_s_mem());
    BIO_push(b64.get(), sink);

    while (BIO_write(b64.get(), bytes.data(), static_cast<int>(bytes.length())) <= 0) {
        if (BIO_should_retry(b64.get())) continue;
        return Error::kUnexpectedError;
    }

    BIO_flush(b64.get());
    const char* encoded;
    const long encoded_len{BIO_get_mem_data(sink, &encoded)};
    return std::string(encoded, static_cast<std::string::size_type>(encoded_len));
}

outcome::result<std::string> encode(std::string_view data) noexcept {
    return encode(znode::string_view_to_byte_view(data));
}

outcome::result<Bytes> decode(std::string_view input) noexcept {
    if (input.empty()) return Bytes();
    const std::unique_ptr<BIO, BIOFreeAll> b64(BIO_new(BIO_f_base64()));
    BIO_set_flags(b64.get(), BIO_FLAGS_BASE64_NO_NL);
    BIO* source = BIO_new_mem_buf(input.data(), -1);
    BIO_push(b64.get(), source);
    const auto maxlen{input.size() / 4U * 3U + 1U};
    Bytes ret(maxlen, '\0');
    const auto effective_len{BIO_read(b64.get(), ret.data(), static_cast<int>(maxlen))};
    if (effective_len <= 0) return Error::kIllegalBase64Digit;
    ret.resize(static_cast<std::string::size_type>(effective_len));
    return ret;
}
}  // namespace znode::enc::base64
