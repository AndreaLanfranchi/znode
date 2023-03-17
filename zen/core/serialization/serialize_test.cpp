/*
   Copyright 2023 Horizen Labs
   Distributed under the MIT software license, see the accompanying
   file COPYING or http://www.opensource.org/licenses/mit-license.php.
*/

#include <catch2/catch.hpp>

#include <zen/core/crypto/sha_2_256.hpp>
#include <zen/core/encoding/hex.hpp>
#include <zen/core/serialization/serialize.hpp>
#include <zen/core/serialization/stream.hpp>

namespace zen::ser {

TEST_CASE("Serialization Sizes", "[serialization]") {
    CHECK(ser_sizeof('a') == 1);
    CHECK(ser_sizeof(uint8_t{0}) == 1);
    CHECK(ser_sizeof(int8_t{0}) == 1);
    CHECK(ser_sizeof(uint16_t{0}) == 2);
    CHECK(ser_sizeof(int16_t{0}) == 2);
    CHECK(ser_sizeof(uint32_t{0}) == 4);
    CHECK(ser_sizeof(int32_t{0}) == 4);
    CHECK(ser_sizeof(uint64_t{0}) == 8);
    CHECK(ser_sizeof(int64_t{0}) == 8);
    CHECK(ser_sizeof(float{0}) == 4);
    CHECK(ser_sizeof(double{0}) == 8);
    CHECK(ser_sizeof(bool{true}) == 1);

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
    DataStream stream(Scope::kStorage, 0);
    CHECK(stream.eof());

    Bytes data{0x00, 0x01, 0x02, 0xff};
    stream.write(data);
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

    DataStream dst(Scope::kStorage, 0);
    stream.get_clear(dst);
    CHECK(stream.eof());
    CHECK(dst.avail() == data.size());
}

TEST_CASE("Serialization of base types", "[serialization]") {
    SECTION("Write Types", "[serialization]") {
        DataStream stream(Scope::kStorage, 0);

        uint8_t u8{0x10};
        write_data(stream, u8);
        CHECK(stream.size() == ser_sizeof(u8));
        uint16_t u16{0x10};
        write_data(stream, u16);
        CHECK(stream.size() == ser_sizeof(u8) + ser_sizeof(u16));
        uint32_t u32{0x10};
        write_data(stream, u32);
        CHECK(stream.size() == ser_sizeof(u8) + ser_sizeof(u16) + ser_sizeof(u32));
        uint64_t u64{0x10};
        write_data(stream, u64);
        CHECK(stream.size() == ser_sizeof(u8) + ser_sizeof(u16) + ser_sizeof(u32) + ser_sizeof(u64));

        float f{1.05f};
        write_data(stream, f);
        CHECK(stream.size() == ser_sizeof(u8) + ser_sizeof(u16) + ser_sizeof(u32) + ser_sizeof(u64) + ser_sizeof(f));

        double d{f * 2};
        write_data(stream, d);
        CHECK(stream.size() ==
              ser_sizeof(u8) + ser_sizeof(u16) + ser_sizeof(u32) + ser_sizeof(u64) + ser_sizeof(f) + ser_sizeof(d));
    }

    SECTION("Floats serialization", "[serialization]") {
        static const double f64v{19880124.0};
        DataStream stream(Scope::kStorage, 0);
        write_data(stream, f64v);
        CHECK(stream.to_string() == "000000c08bf57241");

        stream.clear();
        for (int i{0}; i < 1000; ++i) {
            write_data(stream, double(i));
        }

        // Get Double SHA256 bitcoin like style
        crypto::Sha256 hasher;
        hasher.update(*stream.read(stream.avail()));
        auto hash{hasher.finalize()};
        hasher.init();
        hasher.update(hash);
        hash.assign(hasher.finalize());
        auto hexed_hash{hex::encode(hash)};
        // Same as bitcoin tests but with the hex reversed as uint256S uses base_blob<BITS>::SetHex which processes
        // the string from the bottom
        CHECK(hexed_hash == hex::reverse_hex("43d0c82591953c4eafe114590d392676a01585d25b25d433557f0d7878b23f96"));

        stream.seekp(0);
        for (int i{0}; i < 1000; ++i) {
            const auto returned_value{read_data<double>(stream)};
            REQUIRE(returned_value);
            CHECK(*returned_value == double(i));
        }

        stream.clear();
        for (int i{0}; i < 1000; ++i) {
            write_data(stream, float(i));
        }

        hasher.init();
        hasher.update(*stream.read(stream.avail()));
        hash.assign(hasher.finalize());
        hasher.init();
        hasher.update(hash);
        hash.assign(hasher.finalize());
        hexed_hash.assign(hex::encode(hash));
        CHECK(hexed_hash == hex::reverse_hex("8e8b4cf3e4df8b332057e3e23af42ebc663b61e0495d5e7e32d85099d7f3fe0c"));

        stream.seekp(0);
        for (int i{0}; i < 1000; ++i) {
            const auto returned_value{read_data<float>(stream)};
            REQUIRE(returned_value);
            CHECK(*returned_value == float(i));
        }
    }

    SECTION("Write compact") {
        DataStream stream(Scope::kStorage, 0);
        uint64_t value{0};
        write_compact(stream, value);
        CHECK(stream.size() == 1);

        auto read_bytes(stream.read(stream.avail()));
        CHECK(read_bytes);
        CHECK(hex::encode(*read_bytes) == "00");
        CHECK(stream.eof());

        stream.clear();
        value = 0xfc;
        write_compact(stream, value);
        CHECK(stream.size() == 1);
        read_bytes = stream.read(1);
        CHECK(read_bytes);
        CHECK(hex::encode(*read_bytes) == "fc");
        CHECK(stream.eof());

        stream.clear();
        value = 0xfffe;
        write_compact(stream, value);
        CHECK(stream.size() == 3);
        read_bytes = stream.read(1);
        CHECK(read_bytes);
        CHECK(hex::encode(*read_bytes) == "fd");
        CHECK(stream.tellp() == 1);
        read_bytes = stream.read(stream.avail());
        CHECK(read_bytes);
        CHECK(hex::encode(*read_bytes) == "feff" /*swapped*/);
        CHECK(stream.eof());

        stream.clear();
        value = 0xfffffffe;
        write_compact(stream, value);
        CHECK(stream.size() == 5);
        read_bytes = stream.read(1);
        CHECK(read_bytes);
        CHECK(hex::encode(*read_bytes) == "fe");
        CHECK(stream.tellp() == 1);
        read_bytes = stream.read(stream.avail());
        CHECK(read_bytes);
        CHECK(hex::encode(*read_bytes) == "feffffff" /*swapped*/);
        CHECK(stream.eof());

        stream.clear();
        value = 0xffffffffa0;
        write_compact(stream, value);
        CHECK(stream.size() == 9);
        read_bytes = stream.read(1);
        CHECK(read_bytes);
        CHECK(hex::encode(*read_bytes) == "ff");
        CHECK(stream.tellp() == 1);
        read_bytes = stream.read(stream.avail());
        CHECK(read_bytes);
        CHECK(hex::encode(*read_bytes) == "a0ffffffff000000" /*swapped*/);

        // Try read more bytes than avail
        stream.clear();
        write_compact(stream, value);
        read_bytes = stream.read(stream.avail() + 1);
        CHECK_FALSE(read_bytes);
        CHECK(read_bytes.error() == DeserializationError::kReadBeyondData);
    }

    SECTION("Read Compact", "[serialization]") {
        DataStream stream(Scope::kStorage, 0);
        DataStream::size_type i;

        for (i = 1; i <= kMaxSerializedCompactSize; i *= 2) {
            write_compact(stream, i - 1);
            write_compact(stream, i);
        }

        for (i = 1; i <= kMaxSerializedCompactSize; i *= 2) {
            auto value{read_compact(stream)};
            CHECK(value);
            CHECK(*value == i - 1);
            value = read_compact(stream);
            CHECK(value);
            CHECK(*value == i);
        }

        // Should be consumed entirely
        CHECK(stream.eof());
    }

    SECTION("Non Canonical Compact", "[serialization]") {
        DataStream stream(Scope::kStorage, 0);
        auto value{read_compact(stream)};
        CHECK_FALSE(value);
        CHECK(value.error() == DeserializationError::kReadBeyondData);

        Bytes data{0xfd, 0x00, 0x00};  // Zero encoded with 3 bytes
        stream.write(data);
        value = read_compact(stream);
        CHECK_FALSE(value);
        CHECK(value.error() == DeserializationError::kNonCanonicalCompactSize);

        data.assign({0xfd, 0xfc, 0x00});  // 252 encoded with 3 bytes
        stream.write(data);
        value = read_compact(stream);
        CHECK_FALSE(value);
        CHECK(value.error() == DeserializationError::kNonCanonicalCompactSize);

        data.assign({0xfd, 0xfd, 0x00});  // 253 encoded with 3 bytes
        stream.write(data);
        value = read_compact(stream);
        CHECK(value);

        data.assign({0xfe, 0x00, 0x00, 0x00, 0x00});  // Zero encoded with 5 bytes
        stream.write(data);
        value = read_compact(stream);
        CHECK_FALSE(value);
        CHECK(value.error() == DeserializationError::kNonCanonicalCompactSize);

        data.assign({0xfe, 0xff, 0xff, 0x00, 0x00});  // 0xffff encoded with 5 bytes
        stream.write(data);
        value = read_compact(stream);
        CHECK_FALSE(value);
        CHECK(value.error() == DeserializationError::kNonCanonicalCompactSize);

        data.assign({0xff, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00});  // Zero encoded with 5 bytes
        stream.write(data);
        value = read_compact(stream);
        CHECK_FALSE(value);
        CHECK(value.error() == DeserializationError::kNonCanonicalCompactSize);

        data.assign({0xff, 0xff, 0xff, 0xff, 0x01, 0x00, 0x00, 0x00, 0x00});  // 0x01ffffff encoded with nine bytes
        stream.write(data);
        value = read_compact(stream);
        CHECK_FALSE(value);
        CHECK(value.error() == DeserializationError::kNonCanonicalCompactSize);

        const uint64_t too_big_value{kMaxSerializedCompactSize + 1};
        Bytes too_big_data(reinterpret_cast<uint8_t*>(&too_big_data), sizeof(too_big_value));
        too_big_data.insert(too_big_data.begin(), 0xff);  // 9 bytes
        stream.write(too_big_data);
        value = read_compact(stream);
        CHECK_FALSE(value);
        CHECK(value.error() == DeserializationError::kCompactSizeTooBig);
    }
}
}  // namespace zen::ser
