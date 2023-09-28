/*
   Copyright 2023 Horizen Labs
   Distributed under the MIT software license, see the accompanying
   file COPYING or http://www.opensource.org/licenses/mit-license.php.
*/

#include "message.hpp"

#include <catch2/catch.hpp>
#include <magic_enum.hpp>

#include <core/common/endian.hpp>
#include <core/crypto/hash256.hpp>
#include <core/encoding/hex.hpp>

namespace zenpp::net {

namespace {

    bool parse_hexed_data_into_stream(const std::string& hexed_data, ser::SDataStream& stream) {
        auto data{enc::hex::decode(hexed_data)};
        if (data.has_error()) return false;
        return not stream.write(data.value()).has_error();
    }

    std::string as_message(net::Error error) {
        std::string ret{magic_enum::enum_name<net::Error>(error)};
        ret.erase(0, 1);
        return ret;
    }

    //    std::string as_message(ser::Error error) {
    //        std::string ret{magic_enum::enum_name(error)};
    //        ret.erase(0, 1);
    //        return ret;
    //    }

}  // namespace

TEST_CASE("NetMessage", "[net]") {
    using enum Error;
    Message net_message{};
    auto& header{net_message.header()};
    auto& payload{net_message.data()};

    Bytes network_magic_bytes{0x01, 0x02, 0x03, 0x04};
    std::string hexed_header_data;

    SECTION("Header only validation") {
        CHECK(net_message.validate().error().message() == as_message(kMessageHeaderIncomplete));

        CHECK(header.pristine());
        CHECK(magic_enum::enum_name(header.get_type()) ==
              std::string(magic_enum::enum_name(MessageType::kMissingOrUnknown)));
        CHECK(header.validate().error().message() == as_message(kMessageHeaderEmptyCommand));

        hexed_header_data =
            "00000000"                  // ....               fake network magic
            "76007273696f6e0000000000"  // v.rsion.....       command
            "00000000"                  // ....               payload length
            "00000000";                 // ....               payload checksum

        CHECK(parse_hexed_data_into_stream(hexed_header_data, payload));
        CHECK(header.deserialize(payload).error().message() == as_message(kMessageHeaderMalformedCommand));
        CHECK(header.validate().error().message() == as_message(kMessageHeaderMalformedCommand));

        payload.clear();
        header.reset();

        hexed_header_data =
            "00000000"                  // ....               fake network magic
            "00007273696f6e0000000000"  // ..rsion.....       command
            "00000000"                  // ....               payload length
            "00000000";                 // ....               payload checksum

        CHECK(parse_hexed_data_into_stream(hexed_header_data, payload));
        CHECK(header.deserialize(payload).error().message() == as_message(kMessageHeaderMalformedCommand));
        CHECK(header.validate().error().message() == as_message(kMessageHeaderMalformedCommand));
        payload.clear();
        header.reset();

        hexed_header_data =
            "00000000"                  // ....               fake network magic
            "76657273696f6e0065000000"  // version.e...       command
            "00000000"                  // ....               payload length
            "00000000";                 // ....               payload checksum

        CHECK(parse_hexed_data_into_stream(hexed_header_data, payload));
        CHECK(header.deserialize(payload).error().message() == as_message(kMessageHeaderMalformedCommand));
        CHECK(header.validate().error().message() == as_message(kMessageHeaderMalformedCommand));
        payload.clear();
        header.reset();

        hexed_header_data =
            "00000000"                  // ....               fake network magic
            "76767273696f6e0000000000"  // vvrsion.....       command
            "00000000"                  // ....               payload length
            "00000000";                 // ....               payload checksum

        CHECK(parse_hexed_data_into_stream(hexed_header_data, payload));
        CHECK(header.deserialize(payload).error().message() == as_message(kMessageHeaderIllegalCommand));
        CHECK(header.validate().error().message() == as_message(kMessageHeaderIllegalCommand));
        payload.clear();
        header.reset();

        hexed_header_data =
            "00000000"                  // ....               fake network magic
            "76657273696f6e0000000000"  // version.....       command
            "00000000"                  // ....               payload length
            "00000000";                 // ....               payload checksum

        CHECK(parse_hexed_data_into_stream(hexed_header_data, payload));
        CHECK(header.deserialize(payload).error().message() == as_message(kMessageHeaderUndersizedPayload));
        CHECK(header.validate().error().message() == as_message(kMessageHeaderUndersizedPayload));
        CHECK(magic_enum::enum_name(header.get_type()) == std::string(magic_enum::enum_name(MessageType::kVersion)));
        std::ignore = payload.seekg(0);
        CHECK(payload.to_string() == hexed_header_data);
        payload.clear();
        header.reset();

        hexed_header_data =
            "00000000"                  // ....               fake network magic
            "76657273696f6e0000000000"  // version.....       command
            "80000000"                  // ....               payload length
            "00000000";                 // ....               payload checksum

        CHECK(parse_hexed_data_into_stream(hexed_header_data, payload));
        CHECK_FALSE(header.deserialize(payload).has_error());
        CHECK_FALSE(header.validate().has_error());
        CHECK(magic_enum::enum_name(header.get_type()) == std::string(magic_enum::enum_name(MessageType::kVersion)));
        CHECK(header.payload_length == 128);
        payload.clear();
        header.reset();

        hexed_header_data =
            "01020304"                  // ....               network magic
            "76657261636b000000000000"  // verack......       command
            "00000000"                  // ....               payload length (0)
            "00000000";                 // ....               payload checksum

        CHECK(parse_hexed_data_into_stream(hexed_header_data, payload));
        CHECK(header.deserialize(payload).error().message() == as_message(kMessageHeaderInvalidChecksum));
        CHECK(header.validate().error().message() == as_message(kMessageHeaderInvalidChecksum));
        payload.clear();
        header.reset();

        hexed_header_data =
            "01020304"                  // ....               network magic
            "76657261636b000000000000"  // verack......       command
            "00000000"                  // ....               payload length (0)
            "5df6e0e2";                 // ....               payload checksum (empty)

        CHECK(parse_hexed_data_into_stream(hexed_header_data, payload));
        CHECK_FALSE(header.deserialize(payload).has_error());
        CHECK_FALSE(header.validate().has_error());
        CHECK(magic_enum::enum_name(header.get_type()) == std::string(magic_enum::enum_name(MessageType::kVerAck)));
        CHECK(header.payload_length == 0);
        payload.clear();
        header.reset();

        hexed_header_data =
            "00000000"                  // ....               fake network magic
            "76657273696f6e0000000000"  // version.....       command
            "01040000"                  // ....               payload length (1025)
            "00000000";                 // ....               payload checksum

        CHECK(parse_hexed_data_into_stream(hexed_header_data, payload));
        CHECK(header.deserialize(payload).error().message() == as_message(kMessageHeaderOversizedPayload));
        CHECK(header.validate().error().message() == as_message(kMessageHeaderOversizedPayload));
        CHECK(header.payload_length == 1_KiB + 1);
        CHECK(magic_enum::enum_name(header.get_type()) == std::string(magic_enum::enum_name(MessageType::kVersion)));
        payload.clear();
        header.reset();
    }

    SECTION("Header and body validation") {
        hexed_header_data =
            "00000000"                  // ....               fake network magic
            "696e76000000000000000000"  // inv.........       command
            "00000000"                  // ....               payload length (0)
            "00000000";                 // ....               payload checksum (empty)

        CHECK(parse_hexed_data_into_stream(hexed_header_data, payload));
        CHECK(header.deserialize(payload).error().message() == as_message(kMessageHeaderUndersizedPayload));
        CHECK(header.validate().error().message() == as_message(kMessageHeaderUndersizedPayload));
        CHECK(magic_enum::enum_name(header.get_type()) == std::string(magic_enum::enum_name(MessageType::kInv)));
        payload.clear();
        header.reset();

        hexed_header_data =
            "00000000"                  // ....               fake network magic
            "696e76000000000000000000"  // inv.........       command
            "01000000"                  // ....               payload length (1) not enough
            "00000000";                 // ....               payload checksum (empty)

        CHECK(parse_hexed_data_into_stream(hexed_header_data, payload));
        CHECK(header.deserialize(payload).error().message() == as_message(kMessageHeaderUndersizedPayload));
        CHECK(header.validate().error().message() == as_message(kMessageHeaderUndersizedPayload));
        CHECK(magic_enum::enum_name(header.get_type()) == std::string(magic_enum::enum_name(MessageType::kInv)));
        payload.clear();
        header.reset();

        hexed_header_data =
            "00000000"                  // ....               fake network magic
            "696e76000000000000000000"  // inv.........       command
            "25000000"                  // ....               payload length (37) 1 item
            "00000000";                 // ....               payload checksum (empty)

        CHECK(parse_hexed_data_into_stream(hexed_header_data, payload));
        CHECK_FALSE(header.deserialize(payload).has_error());
        CHECK_FALSE(header.validate().has_error());
        CHECK(magic_enum::enum_name(header.get_type()) == std::string(magic_enum::enum_name(MessageType::kInv)));
        CHECK(header.payload_length == 37);

        // Put in only the size of the vector but nothing else - message validation MUST fail
        uint64_t num_elements{1};
        REQUIRE_FALSE(write_compact(payload, num_elements).has_error());
        CHECK(net_message.validate().error().message() == as_message(kMessageBodyIncomplete));

        // Put in the size of the vector and an incomplete first element - message validation MUST fail
        Bytes data{0x01, 0x02, 0x03, 0x04};
        REQUIRE_FALSE(payload.write(data).has_error());
        REQUIRE(net_message.validate().error().message() == as_message(kMessageBodyIncomplete));

        // Put in the size of the vector and a complete first element 36 bytes - message validation MUST pass
        // sizes checks but will still fail checksum validation
        payload.erase(kMessageHeaderLength + ser::ser_compact_sizeof(num_elements));
        REQUIRE(payload.size() == kMessageHeaderLength + ser::ser_compact_sizeof(num_elements));
        REQUIRE(payload.avail() == ser::ser_compact_sizeof(num_elements));
        data.resize(kInvItemSize, '0');
        REQUIRE_FALSE(payload.write(data).has_error());
        REQUIRE(payload.avail() == ser::ser_compact_sizeof(num_elements) + data.size());

        auto result{net_message.validate_checksum()};
        REQUIRE(result.has_error());
        REQUIRE(result.error().value() == static_cast<int>(kMessageHeaderInvalidChecksum));
        CHECK(result.error().message() == as_message(kMessageHeaderInvalidChecksum));

        REQUIRE(net_message.validate().has_error());
        REQUIRE(net_message.validate().error().message() == as_message(kMessageHeaderInvalidChecksum));

        // Let's move back to begin of body and compute checksum
        payload.seekg(kMessageHeaderLength);
        auto digest_input{payload.read()};
        REQUIRE_FALSE(digest_input.has_error());
        REQUIRE(digest_input.value().size() == ser::ser_compact_sizeof(num_elements) + kInvItemSize);
        crypto::Hash256 hasher(digest_input.value());
        auto final_digest{hasher.finalize()};

        memcpy(header.payload_checksum.data(), final_digest.data(), header.payload_checksum.size());
        REQUIRE_FALSE(net_message.validate().has_error());

        INFO("Test maximum number of vector elements with unique items");
        payload.erase(kMessageHeaderLength);
        num_elements = kMaxInvItems;
        REQUIRE_FALSE(write_compact(payload, num_elements).has_error());
        for (uint64_t i = 0; i < kMaxInvItems; ++i) {
            endian::store_big_u64(data.data(), i);  // This to prevent duplicates
            std::ignore = payload.write(data);
        }

        // Let's move back to begin of body and compute checksum
        payload.seekg(kMessageHeaderLength);
        header.payload_length = static_cast<uint32_t>(payload.avail());
        digest_input = payload.read();
        REQUIRE_FALSE(digest_input.has_error());
        REQUIRE(digest_input.value().size() == ser::ser_compact_sizeof(num_elements) + (kInvItemSize * kMaxInvItems));
        hasher.init(digest_input.value());
        final_digest = hasher.finalize();

        memcpy(header.payload_checksum.data(), final_digest.data(), header.payload_checksum.size());
        REQUIRE_FALSE(net_message.validate().has_error());

        INFO("Test maximum number of vector elements with duplicate items")
        payload.erase(kMessageHeaderLength);
        num_elements = kMaxInvItems;
        REQUIRE_FALSE(write_compact(payload, num_elements).has_error());
        for (uint64_t i = 0; i < kMaxInvItems; ++i) {
            endian::store_big_u64(data.data(), i % 1'000);  // Duplicates every 1K
            std::ignore = payload.write(data);
        }

        // Let's move back to begin of body and compute checksum
        payload.seekg(kMessageHeaderLength);
        header.payload_length = static_cast<uint32_t>(payload.avail());
        digest_input = payload.read();
        REQUIRE_FALSE(digest_input.has_error());
        REQUIRE(digest_input.value().size() == ser::ser_compact_sizeof(num_elements) + (kInvItemSize * kMaxInvItems));
        hasher.init(digest_input.value());
        final_digest = hasher.finalize();

        memcpy(header.payload_checksum.data(), final_digest.data(), header.payload_checksum.size());
        REQUIRE(net_message.validate().error().message() == as_message(kMessagePayloadDuplicateVectorItems));
    }
}

}  // namespace zenpp::net