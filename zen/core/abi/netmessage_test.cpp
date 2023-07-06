/*
   Copyright 2023 Horizen Labs
   Distributed under the MIT software license, see the accompanying
   file COPYING or http://www.opensource.org/licenses/mit-license.php.
*/

#include <catch2/catch.hpp>
#include <magic_enum.hpp>

#include <zen/core/abi/netmessage.hpp>
#include <zen/core/encoding/hex.hpp>

namespace zen {

namespace serialization::test {

    bool parse_hexed_data_into_stream(const std::string& hexed_data, SDataStream& stream) {
        auto data{hex::decode(hexed_data)};
        if (!data) return false;
        return stream.write(*data) == Error::kSuccess;
    }

}  // namespace serialization::test

TEST_CASE("NetMessage", "[abi]") {
    using namespace serialization;
    using enum serialization::Error;
    NetMessage net_message{};
    SDataStream stream{};

    Bytes network_magic_bytes{0x01, 0x02, 0x03, 0x04};
    std::string hexed_data;

    SECTION("Header only validation") {
        REQUIRE(net_message.validate() == kMessageHeaderUnknownCommand);

        auto& header{net_message.header()};
        REQUIRE(header.pristine());
        REQUIRE(header.get_type() == NetMessageType::kMissingOrUnknown);
        REQUIRE(header.validate() == kMessageHeaderEmptyCommand);

        hexed_data =
            "00000000"                  // ....               fake network magic
            "76007273696f6e0000000000"  // v.rsion.....       command
            "00000000"                  // ....               payload length
            "00000000";                 // ....               payload checksum

        REQUIRE(test::parse_hexed_data_into_stream(hexed_data, stream));
        REQUIRE(magic_enum::enum_name(header.deserialize(stream)) == "kSuccess");
        REQUIRE(magic_enum::enum_name(header.validate()) == "kMessageHeaderMalformedCommand");
        stream.clear();
        header.reset();

        hexed_data =
            "00000000"                  // ....               fake network magic
            "00007273696f6e0000000000"  // ..rsion.....       command
            "00000000"                  // ....               payload length
            "00000000";                 // ....               payload checksum

        REQUIRE(test::parse_hexed_data_into_stream(hexed_data, stream));
        REQUIRE(magic_enum::enum_name(header.deserialize(stream)) == "kSuccess");
        REQUIRE(magic_enum::enum_name(header.validate()) == "kMessageHeaderEmptyCommand");
        stream.clear();
        header.reset();

        hexed_data =
            "00000000"                  // ....               fake network magic
            "76657273696f6e0065000000"  // version.e...       command
            "00000000"                  // ....               payload length
            "00000000";                 // ....               payload checksum

        REQUIRE(test::parse_hexed_data_into_stream(hexed_data, stream));
        REQUIRE(magic_enum::enum_name(header.deserialize(stream)) == "kSuccess");
        REQUIRE(magic_enum::enum_name(header.validate()) == "kMessageHeaderMalformedCommand");
        stream.clear();
        header.reset();

        hexed_data =
            "00000000"                  // ....               fake network magic
            "76767273696f6e0000000000"  // vvrsion.....       command
            "00000000"                  // ....               payload length
            "00000000";                 // ....               payload checksum

        REQUIRE(test::parse_hexed_data_into_stream(hexed_data, stream));
        REQUIRE(magic_enum::enum_name(header.deserialize(stream)) == "kSuccess");
        REQUIRE(magic_enum::enum_name(header.validate()) == "kMessageHeaderUnknownCommand");
        stream.clear();
        header.reset();

        hexed_data =
            "00000000"                  // ....               fake network magic
            "76657273696f6e0000000000"  // version.....       command
            "00000000"                  // ....               payload length
            "00000000";                 // ....               payload checksum

        REQUIRE(test::parse_hexed_data_into_stream(hexed_data, stream));
        REQUIRE(magic_enum::enum_name(header.deserialize(stream)) == "kSuccess");
        REQUIRE(magic_enum::enum_name(header.validate()) == "kMessageHeaderUndersizedPayload");
        REQUIRE(magic_enum::enum_name(header.get_type()) == "kVersion");
        stream.clear();
        header.reset();

        hexed_data =
            "00000000"                  // ....               fake network magic
            "76657273696f6e0000000000"  // version.....       command
            "80000000"                  // ....               payload length
            "00000000";                 // ....               payload checksum

        REQUIRE(test::parse_hexed_data_into_stream(hexed_data, stream));
        REQUIRE(magic_enum::enum_name(header.deserialize(stream)) == "kSuccess");
        REQUIRE(magic_enum::enum_name(header.validate()) == "kSuccess");
        REQUIRE(magic_enum::enum_name(header.get_type()) == "kVersion");
        REQUIRE(header.length == 128);
        stream.clear();
        header.reset();

        hexed_data =
            "00000000"                  // ....               fake network magic
            "76657273696f6e0000000000"  // version.....       command
            "01040000"                  // ....               payload length (1025)
            "00000000";                 // ....               payload checksum

        REQUIRE(test::parse_hexed_data_into_stream(hexed_data, stream));
        REQUIRE(magic_enum::enum_name(header.deserialize(stream)) == "kSuccess");
        REQUIRE(magic_enum::enum_name(header.validate()) == "kMessageHeaderOversizedPayload");
        REQUIRE(header.length == 1_KiB + 1);
        REQUIRE(magic_enum::enum_name(header.get_type()) == "kVersion");
        stream.clear();
        header.reset();
    }
}

}  // namespace zen