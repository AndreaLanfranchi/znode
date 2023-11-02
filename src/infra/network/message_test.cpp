/*
   Copyright 2023 The Znode Authors
   Distributed under the MIT software license, see the accompanying
   file COPYING or http://www.opensource.org/licenses/mit-license.php.
*/

#include "message.hpp"

#include <catch2/catch.hpp>
#include <magic_enum.hpp>

#include <core/common/endian.hpp>
#include <core/crypto/hash256.hpp>
#include <core/encoding/hex.hpp>

#include <infra/network/protocol.hpp>

namespace znode::net {
namespace {
    std::string pad_right(const std::string& str, size_t n) {
        std::string ret{str};
        ret.resize(n, '0');
        return ret;
    }

    std::string pad_hexed_header(const std::string& str) { return pad_right(str, kMessageHeaderLength * 2); }

    std::string as_message(net::Error error) {
        std::string ret{magic_enum::enum_name<net::Error>(error)};
        ret.erase(0, 1);
        return ret;
    }
}  // namespace

TEST_CASE("NetMessage write", "[net]") {
    struct test_case {
        std::string test_label{};
        std::string input_data{};
        std::optional<net::Error> expected_error;
    };

    const std::array<uint8_t, 4> network_magic_bytes{0x01, 0x02, 0x03, 0x04};
    Message test_message(kDefaultProtocolVersion, network_magic_bytes);

    std::vector<test_case> test_cases{
        {"Empty Input", "", net::Error::kMessageHeaderIncomplete},
        {"Not enough header data", std::string(8, '0'), net::Error::kMessageHeaderIncomplete},
        {"All zeroes header data", pad_hexed_header("00"), net::Error::kMessageHeaderInvalidMagic},
        {"Invalid network magic", pad_hexed_header("010a0a0a"), net::Error::kMessageHeaderInvalidMagic},
        {"Invalid payload length (over max protocol)",
         pad_hexed_header(enc::hex::encode(network_magic_bytes) + "76657261636b000000000000" + "04000001"),
         net::Error::kMessageHeaderIllegalPayloadLength},
        {"Invalid command (not provided at all)",
         pad_hexed_header(enc::hex::encode(network_magic_bytes) + "000000000000000000000000"),
         net::Error::kMessageHeaderIllegalCommand},
        {"Invalid command (v.rsion)",
         pad_hexed_header(enc::hex::encode(network_magic_bytes) + "76007273696f6e0000000000"),
         net::Error::kMessageHeaderIllegalCommand},
        {"Valid command version / zero payload / null checksum",
         pad_hexed_header(enc::hex::encode(network_magic_bytes) + "76657273696f6e0000000000"),
         net::Error::kMessageHeaderIllegalPayloadLength},
        {"Valid command verack / zero payload / null checksum",
         pad_hexed_header(enc::hex::encode(network_magic_bytes) + "76657261636b000000000000"),
         net::Error::kMessageHeaderInvalidChecksum},
        {"Valid command verack / zero payload / valid checksum",
         pad_hexed_header(enc::hex::encode(network_magic_bytes) + "76657261636b000000000000" + "000000005df6e0e2"),
         std::nullopt},
        {"Valid command inv / zero payload / valid checksum (for empty)",
         pad_hexed_header(enc::hex::encode(network_magic_bytes) + "696e76000000000000000000" + "000000005df6e0e2"),
         net::Error::kMessageHeaderIllegalPayloadLength},
        {"Valid command inv / insufficient payload length / valid checksum (for empty)",
         pad_hexed_header(enc::hex::encode(network_magic_bytes) + "696e76000000000000000000" + "01"),
         net::Error::kMessageHeaderIllegalPayloadLength},
        {"Valid command (inv) / correct payload length (37) / no body",
         pad_hexed_header(enc::hex::encode(network_magic_bytes) + "696e76000000000000000000" + "25"),
         net::Error::kMessageBodyIncomplete},
        {"Valid command (inv) / correct payload length (37) / insufficient body",
         pad_hexed_header(enc::hex::encode(network_magic_bytes) + "696e76000000000000000000" + "25") + "01",
         net::Error::kMessageBodyIncomplete},
        {"Valid command (inv) / correct payload length (37) / enough body for zero items",
         pad_hexed_header(enc::hex::encode(network_magic_bytes) + "696e76000000000000000000" + "25") + "00",
         net::Error::kMessagePayloadEmptyVector},
        {"Valid command (inv) / short payload length (30) / one item in empty vector",
         pad_hexed_header(enc::hex::encode(network_magic_bytes) + "696e76000000000000000000" + "1e") + "01",
         net::Error::kMessageHeaderIllegalPayloadLength},
        {"Valid command (inv) / correct payload length (37) / body signals 2 items that would exceed payload length",
         pad_hexed_header(enc::hex::encode(network_magic_bytes) + "696e76000000000000000000" + "25") + "02",
         net::Error::kMessagePayloadLengthMismatchesVectorSize},
        {"Valid command (inv) / correct payload length (73) / body signals 2 items / duplicated",
         pad_hexed_header(enc::hex::encode(network_magic_bytes) + "696e76000000000000000000" + "49") + "02" +
             std::string(kInvItemSize * 2 * 2, '0'),
         net::Error::kMessagePayloadDuplicateVectorItems},
        {"Valid command (inv) / correct payload length (73) / body signals 2 items / null checksum",
         pad_hexed_header(enc::hex::encode(network_magic_bytes) + "696e76000000000000000000" + "49") + "02" +
             enc::hex::get_random(kInvItemSize * 2 * 2),
         net::Error::kMessageHeaderInvalidChecksum},
    };

    // We need to create an extra test with known payload to compute the checksum
    // and then replace the checksum in the test case
    std::string hexed_input{pad_hexed_header(enc::hex::encode(network_magic_bytes) + "696e76000000000000000000" +
                                             "49000000" + "00000000" /* checksum */) +
                            "02" + enc::hex::get_random(kInvItemSize * 2 * 2)};
    auto input_bytes{enc::hex::decode(hexed_input).value()};
    crypto::Hash256 payload_digest(
        input_bytes.substr(kMessageHeaderLength, input_bytes.length() - kMessageHeaderLength));
    const auto checksum{payload_digest.finalize()};
    memcpy(&input_bytes[kMessageHeaderLength - 4], checksum.data(), 4);
    test_cases.emplace_back(test_case{"Valid command (inv) / correct payload length (73) / body signals 2 items",
                                      enc::hex::encode(input_bytes), std::nullopt});

    for (const auto& test : test_cases) {
        INFO(test.test_label)
        test_message.reset();
        CHECK(test_message.header().pristine());
        const auto data{enc::hex::decode(test.input_data)};
        REQUIRE_FALSE(data.has_error());
        ByteView data_view{data.value()};
        auto write_result{test_message.write(data_view)};

        if (test.expected_error.has_value()) {
            REQUIRE(write_result.has_error());
            CHECK(write_result.error().value() == static_cast<int>(test.expected_error.value()));
            CHECK(write_result.error().message() == as_message(test.expected_error.value()));
        } else {
            REQUIRE_FALSE(write_result.has_error());
            REQUIRE(test_message.get_type() not_eq MessageType::kMissingOrUnknown);
            REQUIRE(test_message.is_complete());
        }
    }
}

}  // namespace znode::net