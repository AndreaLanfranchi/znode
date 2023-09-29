/*
   Copyright 2023 Horizen Labs
   Distributed under the MIT software license, see the accompanying
   file COPYING or http://www.opensource.org/licenses/mit-license.php.
*/

#include <catch2/catch.hpp>

#include <core/crypto/md.hpp>
#include <core/encoding/hex.hpp>
#include <core/serialization/serialize.hpp>
#include <core/serialization/stream.hpp>

namespace zenpp::ser {

TEST_CASE("Serialization Sizes", "[serialization]") {
    CHECK(ssizeof<char> == 1);
    CHECK(ssizeof<uint8_t> == 1);
    CHECK(ssizeof<int8_t> == 1);
    CHECK(ssizeof<uint16_t> == 2);
    CHECK(ssizeof<int16_t> == 2);
    CHECK(ssizeof<uint32_t> == 4);
    CHECK(ssizeof<int32_t> == 4);
    CHECK(ssizeof<uint64_t> == 8);
    CHECK(ssizeof<int64_t> == 8);
    CHECK(ssizeof<float> == 4);
    CHECK(ssizeof<double> == 8);
    CHECK(ssizeof<bool> == 1);
    CHECK(ssizeof<uint128_t> == 16);
    CHECK(ssizeof<uint256_t> == 32);

    CHECK(sizeof(uint32_t) == sizeof(float));
    CHECK(sizeof(uint64_t) == sizeof(double));

    // Compact integral
    CHECK(ser_compact_sizeof(0x00) == 1);
    CHECK(ser_compact_sizeof(0xfffa) == 3);
    CHECK(ser_compact_sizeof(256) == 3);
    CHECK(ser_compact_sizeof(0x10003) == 5);
    CHECK(ser_compact_sizeof(0xffffffff) == 5);
    CHECK(ser_compact_sizeof(UINT64_MAX) == 9);
}

TEST_CASE("Float conversions", "[serialization]") {
    SECTION("Floats") {
        std::vector<std::pair<uint32_t, float>> tests{{0x00, 0.0F},       {0x3f000000, 0.5F},
                                                      {0x3f800000, 1.0F}, {0x40000000, 2.0F},
                                                      {0x40800000, 4.0F}, {0x44444444, 785.066650390625F}};

        for (const auto& [u32, f32] : tests) {
            CHECK(std::bit_cast<float>(u32) == f32);
            CHECK(std::bit_cast<uint32_t>(f32) == u32);
        }
    }

    SECTION("Doubles") {
        std::vector<std::pair<uint64_t, double>> tests{
            {0x0000000000000000ULL, 0.0}, {0x3fe0000000000000ULL, 0.5},  {0x3ff0000000000000ULL, 1.0},
            {0x4000000000000000ULL, 2.0}, {0x4010000000000000ULL, 4.0F}, {0x4088888880000000ULL, 785.066650390625}};

        for (const auto& [u64, f64] : tests) {
            CHECK(std::bit_cast<double>(u64) == f64);
            CHECK(std::bit_cast<uint64_t>(f64) == u64);
        }
    }
}

TEST_CASE("Serialization stream", "[serialization]") {
    SDataStream stream(Scope::kStorage, 0);
    CHECK(stream.eof());

    Bytes data{0x00, 0x01, 0x02, 0xff};
    REQUIRE_FALSE(stream.write(data).has_error());
    CHECK(stream.size() == data.size());
    CHECK(stream.avail() == data.size());

    // Inserting at beginning/end/middle:

    uint8_t c{0x11};
    stream.insert(stream.begin(), c);
    CHECK(stream.size() == data.size() + 1);
    CHECK(stream[0] == c);
    CHECK(stream[1] == 0);

    stream.insert(stream.end(), c);
    CHECK(stream.size() == data.size() + 2);
    CHECK(stream[4] == 0xff);
    CHECK(stream[5] == c);

    stream.insert(stream.begin() + 2, c);
    CHECK(stream.size() == data.size() + 3);
    CHECK(stream[2] == c);

    // Delete at beginning/end/middle

    stream.erase(stream.begin());
    CHECK(stream.size() == data.size() + 2);
    CHECK(stream[0] == 0);

    stream.erase(stream.begin() + static_cast<DataStream::difference_type>(stream.size()) - 1);
    CHECK(stream.size() == data.size() + 1);
    CHECK(stream[4] == 0xff);

    stream.erase(stream.begin() + 1);
    CHECK(stream.size() == data.size());
    CHECK(stream[0] == 0);
    CHECK(stream[1] == 1);
    CHECK(stream[2] == 2);
    CHECK(stream[3] == 0xff);

    SDataStream dst(Scope::kStorage, 0);
    REQUIRE_FALSE(stream.get_clear(dst).has_error());
    CHECK(stream.eof());
    CHECK(dst.avail() == data.size());
}

TEST_CASE("Serialization of base types", "[serialization]") {
    SECTION("Write Types", "[serialization]") {
        SDataStream stream(Scope::kStorage, 0);

        uint8_t u8{0x10};
        REQUIRE_FALSE(write_data(stream, u8).has_error());
        CHECK(stream.size() == ssizeof<uint8_t>);
        uint16_t u16{0x10};
        REQUIRE_FALSE(write_data(stream, u16).has_error());
        CHECK(stream.size() == ssizeof<uint8_t> + ssizeof<uint16_t>);
        uint32_t u32{0x10};
        REQUIRE_FALSE(write_data(stream, u32).has_error());
        CHECK(stream.size() == ssizeof<uint8_t> + ssizeof<uint16_t> + ssizeof<uint32_t>);
        uint64_t u64{0x10};
        REQUIRE_FALSE(write_data(stream, u64).has_error());
        CHECK(stream.size() == ssizeof<uint8_t> + ssizeof<uint16_t> + ssizeof<uint32_t> + ssizeof<uint64_t>);

        float f{1.05f};
        REQUIRE_FALSE(write_data(stream, f).has_error());
        CHECK(stream.size() ==
              ssizeof<uint8_t> + ssizeof<uint16_t> + ssizeof<uint32_t> + ssizeof<uint64_t> + ssizeof<float>);

        double d{f * 2};
        REQUIRE_FALSE(write_data(stream, d).has_error());
        CHECK(stream.size() == ssizeof<uint8_t> + ssizeof<uint16_t> + ssizeof<uint32_t> + ssizeof<uint64_t> +
                                   ssizeof<float> + ssizeof<double>);
    }

    SECTION("Floats serialization", "[serialization]") {
        static const double f64v{19880124.0};
        SDataStream stream(Scope::kStorage, 0);
        REQUIRE_FALSE(write_data(stream, f64v).has_error());
        CHECK(stream.to_string() == "000000c08bf57241");

        stream.clear();
        for (int i{0}; i < 1000; ++i) {
            std::ignore = write_data(stream, double(i));
        }

        // Get Double SHA256 bitcoin like style
        crypto::Sha256 hasher;
        hasher.update(stream.read(stream.avail()).value());
        auto hash{hasher.finalize()};
        hasher.init();
        hasher.update(hash);
        hash.assign(hasher.finalize());
        auto hexed_hash{enc::hex::encode(hash)};
        // Same as bitcoin tests but with the hex reversed as uint256S uses base_blob<BITS>::SetHex which processes
        // the string from the bottom
        CHECK(hexed_hash == enc::hex::reverse_hex("43d0c82591953c4eafe114590d392676a01585d25b25d433557f0d7878b23f96"));

        stream.seekg(0);
        for (int i{0}; i < 1000; ++i) {
            const auto returned{read_data<double>(stream)};
            REQUIRE_FALSE(returned.has_error());
            CHECK(returned.value() == double(i));
        }

        stream.clear();
        for (int i{0}; i < 1000; ++i) {
            std::ignore = write_data(stream, float(i));
        }

        hasher.init();
        hasher.update(stream.read(stream.avail()).value());
        hash.assign(hasher.finalize());
        hasher.init();
        hasher.update(hash);
        hash.assign(hasher.finalize());
        hexed_hash.assign(enc::hex::encode(hash));
        CHECK(hexed_hash == enc::hex::reverse_hex("8e8b4cf3e4df8b332057e3e23af42ebc663b61e0495d5e7e32d85099d7f3fe0c"));

        stream.seekg(0);
        for (int i{0}; i < 1000; ++i) {
            const auto returned{read_data<float>(stream)};
            REQUIRE_FALSE(returned.has_error());
            CHECK(returned.value() == float(i));
        }
    }

    SECTION("Write compact") {
        SDataStream stream(Scope::kStorage, 0);
        uint64_t value{0};
        REQUIRE_FALSE(write_compact(stream, value).has_error());
        CHECK(stream.size() == 1);

        auto read_bytes(stream.read(stream.avail()));
        REQUIRE_FALSE(read_bytes.has_error());
        CHECK(enc::hex::encode(read_bytes.value()) == "00");
        CHECK(stream.eof());

        stream.clear();
        value = 0xfc;
        REQUIRE_FALSE(write_compact(stream, value).has_error());
        CHECK(stream.size() == 1);
        read_bytes = stream.read(1);
        REQUIRE_FALSE(read_bytes.has_error());
        CHECK(enc::hex::encode(read_bytes.value()) == "fc");
        CHECK(stream.eof());

        stream.clear();
        value = 0xfffe;
        REQUIRE_FALSE(write_compact(stream, value).has_error());
        CHECK(stream.size() == 3);
        read_bytes = stream.read(1);
        REQUIRE(read_bytes);
        CHECK(enc::hex::encode(read_bytes.value()) == "fd");
        CHECK(stream.tellg() == 1);
        read_bytes = stream.read(stream.avail());
        REQUIRE(read_bytes);
        CHECK(enc::hex::encode(read_bytes.value()) == "feff" /*swapped*/);
        CHECK(stream.eof());

        stream.clear();
        value = 0xfffffffe;
        REQUIRE_FALSE(write_compact(stream, value).has_error());
        CHECK(stream.size() == 5);
        read_bytes = stream.read(1);
        REQUIRE(read_bytes);
        CHECK(enc::hex::encode(read_bytes.value()) == "fe");
        CHECK(stream.tellg() == 1);
        read_bytes = stream.read(stream.avail());
        REQUIRE(read_bytes);
        CHECK(enc::hex::encode(read_bytes.value()) == "feffffff" /*swapped*/);
        CHECK(stream.eof());

        stream.clear();
        value = 0xffffffffa0;
        REQUIRE_FALSE(write_compact(stream, value).has_error());
        CHECK(stream.size() == 9);
        read_bytes = stream.read(1);
        REQUIRE(read_bytes);
        CHECK(enc::hex::encode(read_bytes.value()) == "ff");
        CHECK(stream.tellg() == 1);
        read_bytes = stream.read(stream.avail());
        REQUIRE(read_bytes);
        CHECK(enc::hex::encode(read_bytes.value()) == "a0ffffffff000000" /*swapped*/);

        // Try read more bytes than avail
        stream.clear();
        REQUIRE_FALSE(write_compact(stream, value).has_error());
        read_bytes = stream.read(stream.avail() + 1);
        REQUIRE(read_bytes.has_error());
        REQUIRE(read_bytes.error().value() == static_cast<int>(Error::kReadOverflow));
    }

    SECTION("Read Compact", "[serialization]") {
        SDataStream stream(Scope::kStorage, 0);
        DataStream::size_type i;

        for (i = 1; i <= kMaxSerializedCompactSize; i *= 2) {
            std::ignore = write_compact(stream, i - 1);
            std::ignore = write_compact(stream, i);
        }

        for (i = 1; i <= kMaxSerializedCompactSize; i *= 2) {
            auto result{read_compact(stream)};
            REQUIRE_FALSE(result.has_error());
            CHECK(result.value() == i - 1);
            result = read_compact(stream);
            REQUIRE_FALSE(result.has_error());
            CHECK(result.value() == i);
        }

        // Should be consumed entirely
        CHECK(stream.eof());
    }

    SECTION("Non Canonical Compact", "[serialization]") {
        SDataStream stream(Scope::kStorage, 0);
        auto value{read_compact(stream)};
        REQUIRE(value.has_error());
        CHECK(value.error().value() == static_cast<int>(Error::kReadOverflow));

        Bytes data{0xfd, 0x00, 0x00};  // Zero encoded with 3 bytes
        REQUIRE_FALSE(stream.write(data).has_error());
        value = read_compact(stream);
        REQUIRE_FALSE(value);
        CHECK(value.error().value() == static_cast<int>(Error::kNonCanonicalCompactSize));

        data.assign({0xfd, 0xfc, 0x00});  // 252 encoded with 3 bytes
        REQUIRE_FALSE(stream.write(data).has_error());
        value = read_compact(stream);
        REQUIRE_FALSE(value);
        CHECK(value.error().value() == static_cast<int>(Error::kNonCanonicalCompactSize));

        data.assign({0xfd, 0xfd, 0x00});  // 253 encoded with 3 bytes
        REQUIRE_FALSE(stream.write(data).has_error());
        value = read_compact(stream);
        REQUIRE(value);

        data.assign({0xfe, 0x00, 0x00, 0x00, 0x00});  // Zero encoded with 5 bytes
        REQUIRE_FALSE(stream.write(data).has_error());
        value = read_compact(stream);
        CHECK_FALSE(value);
        CHECK(value.error().value() == static_cast<int>(Error::kNonCanonicalCompactSize));

        data.assign({0xfe, 0xff, 0xff, 0x00, 0x00});  // 0xffff encoded with 5 bytes
        REQUIRE_FALSE(stream.write(data).has_error());
        value = read_compact(stream);
        CHECK_FALSE(value);
        CHECK(value.error().value() == static_cast<int>(Error::kNonCanonicalCompactSize));

        data.assign({0xff, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00});  // Zero encoded with 9 bytes
        REQUIRE_FALSE(stream.write(data).has_error());
        value = read_compact(stream);
        CHECK_FALSE(value);
        CHECK(value.error().value() == static_cast<int>(Error::kNonCanonicalCompactSize));

        data.assign({0xff, 0xff, 0xff, 0xff, 0x01, 0x00, 0x00, 0x00, 0x00});  // 0x01ffffff encoded with nine bytes
        REQUIRE_FALSE(stream.write(data).has_error());
        value = read_compact(stream);
        CHECK_FALSE(value);
        CHECK(value.error().value() == static_cast<int>(Error::kNonCanonicalCompactSize));

        const uint32_t too_big_value{kMaxSerializedCompactSize + 1};
        Bytes too_big_data(reinterpret_cast<const uint8_t*>(&too_big_value), sizeof(too_big_value));
        too_big_data.insert(too_big_data.begin(), 0xfe);
        REQUIRE_FALSE(stream.write(too_big_data).has_error());
        value = read_compact(stream);
        CHECK_FALSE(value);
        CHECK(value.error().value() == static_cast<int>(Error::kCompactSizeTooBig));
    }
}
}  // namespace zenpp::ser
