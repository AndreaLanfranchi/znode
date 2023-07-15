/*
   Copyright 2022 The Silkworm Authors
   Copyright 2023 Horizen Labs
   Distributed under the MIT software license, see the accompanying
   file COPYING or http://www.opensource.org/licenses/mit-license.php.
*/

#include <atomic>
#include <map>
#include <thread>
#include <vector>

#include <catch2/catch.hpp>

#include <zen/core/common/cast.hpp>

#include <zen/node/common/directories.hpp>
#include <zen/node/database/mdbx.hpp>

namespace zen::db {

static const std::map<std::string, std::string, std::less<>> kGeneticCodes{
    {"AAA", "Lysine"},        {"AAC", "Asparagine"},    {"AAG", "Lysine"},        {"AAU", "Asparagine"},
    {"ACA", "Threonine"},     {"ACC", "Threonine"},     {"ACG", "Threonine"},     {"ACU", "Threonine"},
    {"AGA", "Arginine"},      {"AGC", "Serine"},        {"AGG", "Arginine"},      {"AGU", "Serine"},
    {"AUA", "Isoleucine"},    {"AUC", "Isoleucine"},    {"AUG", "Methionine"},    {"AUU", "Isoleucine"},
    {"CAA", "Glutamine"},     {"CAC", "Histidine"},     {"CAG", "Glutamine"},     {"CAU", "Histidine"},
    {"CCA", "Proline"},       {"CCC", "Proline"},       {"CCG", "Proline"},       {"CCU", "Proline"},
    {"CGA", "Arginine"},      {"CGC", "Arginine"},      {"CGG", "Arginine"},      {"CGU", "Arginine"},
    {"CUA", "Leucine"},       {"CUC", "Leucine"},       {"CUG", "Leucine"},       {"CUU", "Leucine"},
    {"GAA", "Glutamic acid"}, {"GAC", "Aspartic acid"}, {"GAG", "Glutamic acid"}, {"GAU", "Aspartic acid"},
    {"GCA", "Alanine"},       {"GCC", "Alanine"},       {"GCG", "Alanine"},       {"GCU", "Alanine"},
    {"GGA", "Glycine"},       {"GGC", "Glycine"},       {"GGG", "Glycine"},       {"GGU", "Glycine"},
    {"GUA", "Valine"},        {"GUC", "Valine"},        {"GUG", "Valine"},        {"GUU", "Valine"},
    {"UAA", "Stop"},          {"UAC", "Tyrosine"},      {"UAG", "Stop"},          {"UAU", "Tyrosine"},
    {"UCA", "Serine"},        {"UCC", "Serine"},        {"UCG", "Serine"},        {"UCU", "Serine"},
    {"UGA", "Stop"},          {"UGC", "Cysteine"},      {"UGG", "Tryptophan"},    {"UGU", "Cysteine"},
    {"UUA", "Leucine"},       {"UUC", "Phenylalanine"}, {"UUG", "Leucine"},       {"UUU", "Phenylalanine"},
};
TEST_CASE("Database Environment", "[database]") {
    const TempDirectory tmp_dir{};

    SECTION("Non default page size") {
        EnvConfig db_config{tmp_dir.path().string(), /*create=*/true};
        db_config.inmemory = true;
        auto env{db::open_env(db_config)};
        REQUIRE(env.get_pagesize() == db_config.page_size);
    }

    SECTION("Incompatible page size") {
        {
            db::EnvConfig db_config{tmp_dir.path().string(), /*create*/ true};
            db_config.inmemory = true;
            db_config.page_size = 4_KiB;
            REQUIRE_NOTHROW((void)db::open_env(db_config));
        }

        // Try re-open same db with different page size
        db::EnvConfig db_config{tmp_dir.path().string(), /*create*/ false};
        db_config.inmemory = true;
        db_config.page_size = 16_KiB;
        REQUIRE_THROWS((void)db::open_env(db_config));
    }
}

TEST_CASE("Database Cursor", "[database]") {
    const TempDirectory tmp_dir;
    db::EnvConfig db_config{tmp_dir.path().string(), /*create*/ true};
    db_config.inmemory = true;
    auto env{db::open_env(db_config)};
    const db::MapConfig map_config{"GeneticCode"};

    auto txn{env.start_write()};
    REQUIRE_FALSE(has_map(txn, map_config.name));
    (void)open_map(txn, map_config);
    txn.commit();  // Is no longer valid
    txn = env.start_read();
    REQUIRE(has_map(txn, map_config.name));

    // A bit of explanation here:
    // Cursors cache may get polluted by previous tests or is empty
    // in case this is the only test being executed. So we can't rely
    // on empty() property rather we must evaluate deltas.
    const size_t original_cache_size{db::Cursor::handles_cache().size()};

    {
        Cursor cursor1(txn, map_config);
        const size_t expected_cache_size{std::min(original_cache_size, original_cache_size - 1)};
        REQUIRE(Cursor::handles_cache().size() == expected_cache_size);
        REQUIRE(cursor1.get_map_stat().ms_entries == 0);
    }

    // After destruction of previous cursor cache has increased by one if it was originally empty, otherwise it is
    // restored to its original size
    if (!original_cache_size) {
        REQUIRE(db::Cursor::handles_cache().size() == original_cache_size + 1);
    } else {
        REQUIRE(db::Cursor::handles_cache().size() == original_cache_size);
    }

    txn.abort();
    txn = env.start_write();

    // Force exceed of cache size
    std::vector<db::Cursor> cursors;
    for (size_t i = 0; i < original_cache_size + 5; ++i) {
        cursors.emplace_back(txn, map_config);
    }

    REQUIRE(Cursor::handles_cache().empty() == true);
    cursors.clear();
    REQUIRE(Cursor::handles_cache().empty() == false);
    REQUIRE(Cursor::handles_cache().size() == original_cache_size + 5);

    Cursor cursor2(txn, {"test"});
    REQUIRE(cursor2.operator bool() == true);
    Cursor cursor3 = std::move(cursor2);
    REQUIRE(cursor2.operator bool() == false);
    REQUIRE(cursor3.operator bool() == true);
    txn.commit();

    // In another thread cursor cache must be empty
    std::atomic<size_t> other_thread_size1{0};
    std::atomic<size_t> other_thread_size2{0};
    std::thread t([&other_thread_size1, &other_thread_size2, &env]() {
        auto thread_txn{env.start_write()};
        { Cursor cursor(thread_txn, {"Test"}); }
        other_thread_size1 = Cursor::handles_cache().size();

        // Pull a handle from the pool and close the cursor directly
        // so is not returned to the pool
        Cursor cursor(thread_txn, {"Test"});
        cursor.close();
        other_thread_size2 = Cursor::handles_cache().size();
    });
    t.join();
    REQUIRE(other_thread_size1 == 1);
    REQUIRE(other_thread_size2 == 0);
}

TEST_CASE("Read/Write Transaction", "[database]") {
    const TempDirectory tmp_dir;
    db::EnvConfig db_config{tmp_dir.path().string(), /*create*/ true};
    db_config.inmemory = true;
    auto env{db::open_env(db_config)};
    static const char* table_name{"GeneticCode"};

    SECTION("Managed") {
        auto tx{db::RWTxn(env)};
        db::Cursor table_cursor(*tx, {table_name});

        // populate table
        for (const auto& [key, value] : kGeneticCodes) {
            table_cursor.upsert(mdbx::slice{key}, mdbx::slice{value});
        }
        tx.commit(/*renew=*/true);
        table_cursor.bind(tx, {table_name});
        REQUIRE_FALSE(table_cursor.empty());
    }

    SECTION("External") {
        SECTION("External") {
            auto ext_tx{env.start_write()};
            {
                auto tx{db::RWTxn(ext_tx)};
                (void)tx->create_map(table_name, mdbx::key_mode::usual, mdbx::value_mode::single);
                tx.commit();  // Does not have any effect
            }
            ext_tx.abort();
            ext_tx = env.start_write();
            REQUIRE(db::has_map(ext_tx, table_name) == false);
        }
    }

    SECTION("Cursor from RWTxn") {
        auto tx{db::RWTxn(env)};
        db::Cursor table_cursor(tx, {table_name});
        REQUIRE(table_cursor.empty());
        REQUIRE_NOTHROW(table_cursor.bind(tx, {table_name}));
        table_cursor.close();
        REQUIRE_THROWS(table_cursor.bind(tx, {table_name}));
    }
}

TEST_CASE("Database Cursor Walk", "[database]") {
    const TempDirectory tmp_dir;
    EnvConfig db_config{tmp_dir.path().string(), /*create*/ true};
    db_config.inmemory = true;
    auto env{db::open_env(db_config)};
    auto txn{env.start_write()};

    static const char* table_name{"GeneticCode"};
    Cursor table_cursor(txn, {table_name});

    // A map to collect data
    std::map<std::string, std::string, std::less<>> data_map;
    auto save_all_data_map{[&data_map](ByteView key, ByteView value) {
        data_map.emplace(byte_view_to_string_view(key), byte_view_to_string_view(value));
    }};

    // A vector to collect data
    std::vector<std::pair<std::string, std::string>> data_vec;
    auto save_all_data_vec{[&data_vec](ByteView key, ByteView value) {
        data_vec.emplace_back(byte_view_to_string_view(key), byte_view_to_string_view(value));
    }};

    SECTION("For each") {
        // Empty table
        cursor_for_each(table_cursor, save_all_data_map);
        REQUIRE(data_map.empty());
        REQUIRE(table_cursor.empty());

        // Populate table
        for (const auto& [key, value] : kGeneticCodes) {
            table_cursor.upsert(to_slice(key), to_slice(value));
        }
        REQUIRE(table_cursor.size() == kGeneticCodes.size());
        REQUIRE_FALSE(table_cursor.empty());

        // Rebind cursor so its position is undefined
        table_cursor.bind(txn, {table_name});
        REQUIRE(table_cursor.eof());

        // read entire table forward
        cursor_for_each(table_cursor, save_all_data_map);
        CHECK(data_map == kGeneticCodes);
        data_map.clear();

        // read entire table backwards
        table_cursor.bind(txn, {table_name});
        cursor_for_each(table_cursor, save_all_data_map, CursorMoveDirection::Reverse);
        CHECK(data_map == kGeneticCodes);
        data_map.clear();

        // Ensure the order is reversed
        table_cursor.bind(txn, {table_name});
        cursor_for_each(table_cursor, save_all_data_vec, CursorMoveDirection::Reverse);
        CHECK(data_vec.back().second == kGeneticCodes.at("AAA"));

        // Late start
        table_cursor.find("UUG");
        cursor_for_each(table_cursor, save_all_data_map);
        REQUIRE(data_map.size() == 2);
        CHECK(data_map.at("UUG") == "Leucine");
        CHECK(data_map.at("UUU") == "Phenylalanine");
        data_map.clear();
    }

    SECTION("Erase by prefix") {
        // populate table
        for (const auto& [key, value] : kGeneticCodes) {
            table_cursor.upsert(to_slice(key), to_slice(value));
        }
        REQUIRE(table_cursor.size() == kGeneticCodes.size());
        REQUIRE_FALSE(table_cursor.empty());

        // Delete all records starting with "AC"
        Bytes prefix{'A', 'C'};
        const auto erased{cursor_erase_prefix(table_cursor, prefix)};
        REQUIRE(erased == 4);
        REQUIRE(table_cursor.size() == (kGeneticCodes.size() - erased));
    }

    SECTION("Iterate by prefix") {
        // populate table
        for (const auto& [key, value] : kGeneticCodes) {
            table_cursor.upsert(mdbx::slice{key}, mdbx::slice{value});
        }

        Bytes prefix{'A', 'A'};
        auto count{cursor_for_prefix(table_cursor, prefix, [](ByteView, ByteView) {
            // do nothing
        })};
        REQUIRE(count == 4);
    }

    SECTION("Iterate by limit") {
        // empty table
        cursor_for_count(table_cursor, save_all_data_map, /*max_count=*/5);
        REQUIRE(data_map.empty());

        // populate table
        for (const auto& [key, value] : kGeneticCodes) {
            table_cursor.upsert(mdbx::slice{key}, mdbx::slice{value});
        }

        // read entire table
        table_cursor.to_first();
        cursor_for_count(table_cursor, save_all_data_map, /*max_count=*/100);
        CHECK(data_map == kGeneticCodes);
        data_map.clear();

        // Read some first entries
        table_cursor.to_first();
        cursor_for_count(table_cursor, save_all_data_map, /*max_count=*/5);
        REQUIRE(data_map.size() == 5);
        CHECK(data_map.at("AAA") == "Lysine");
        CHECK(data_map.at("AAC") == "Asparagine");
        CHECK(data_map.at("AAG") == "Lysine");
        CHECK(data_map.at("AAU") == "Asparagine");
        CHECK(data_map.at("ACA") == "Threonine");
        data_map.clear();

        // Late start
        table_cursor.find("UUA");
        cursor_for_count(table_cursor, save_all_data_map, /*max_count=*/3);
        REQUIRE(data_map.size() == 3);
        CHECK(data_map.at("UUA") == "Leucine");
        CHECK(data_map.at("UUC") == "Phenylalanine");
        CHECK(data_map.at("UUG") == "Leucine");
        data_map.clear();

        // Reverse read
        table_cursor.to_last();
        cursor_for_count(table_cursor, save_all_data_map, /*max_count=*/4, CursorMoveDirection::Reverse);
        REQUIRE(data_map.size() == 4);
        CHECK(data_map.at("UUA") == "Leucine");
        CHECK(data_map.at("UUC") == "Phenylalanine");
        CHECK(data_map.at("UUG") == "Leucine");
        CHECK(data_map.at("UUU") == "Phenylalanine");
        data_map.clear();

        // Selective save 1
        const auto save_some_data{[&data_map](ByteView key, ByteView value) {
            if (value != string_view_to_byte_view("Threonine")) {
                data_map.emplace(byte_view_to_string_view(key), byte_view_to_string_view(value));
            }
        }};

        table_cursor.to_first();
        cursor_for_count(table_cursor, save_some_data, /*max_count=*/3);
        REQUIRE(data_map.size() == 3);
        CHECK(data_map.at("AAA") == "Lysine");
        CHECK(data_map.at("AAC") == "Asparagine");
        CHECK(data_map.at("AAG") == "Lysine");
        data_map.clear();

        // Selective save 2
        table_cursor.to_first();
        cursor_for_count(table_cursor, save_some_data, /*max_count=*/5);
        REQUIRE(data_map.size() == 4);
        CHECK(data_map.at("AAA") == "Lysine");
        CHECK(data_map.at("AAC") == "Asparagine");
        CHECK(data_map.at("AAG") == "Lysine");
        CHECK(data_map.at("AAU") == "Asparagine");
    }

    SECTION("Erase") {
        // populate table
        for (const auto& [key, value] : kGeneticCodes) {
            table_cursor.upsert(mdbx::slice{key}, mdbx::slice{value});
        }

        // Erase all records in forward order
        table_cursor.bind(txn, {table_name});
        cursor_erase(table_cursor, {});
        REQUIRE(txn.get_map_stat(table_cursor.map()).ms_entries == 0);

        // populate table
        for (const auto& [key, value] : kGeneticCodes) {
            table_cursor.upsert(mdbx::slice{key}, mdbx::slice{value});
        }

        // Erase all records in reverse order
        Bytes set_key{'X', 'X', 'X'};
        table_cursor.bind(txn, {table_name});
        cursor_erase(table_cursor, set_key, CursorMoveDirection::Reverse);
        REQUIRE(txn.get_map_stat(table_cursor.map()).ms_entries == 0);

        // populate table
        for (const auto& [key, value] : kGeneticCodes) {
            table_cursor.upsert(mdbx::slice{key}, mdbx::slice{value});
        }

        // Erase backwards from "CAA"
        set_key.assign({'C', 'A', 'A'});
        cursor_erase(table_cursor, set_key, CursorMoveDirection::Reverse);
        cursor_for_each(table_cursor, save_all_data_map);
        REQUIRE(data_map.begin()->second == "Glutamine");

        // Erase forward from "UAA"
        set_key.assign({'U', 'A', 'A'});
        cursor_erase(table_cursor, set_key, CursorMoveDirection::Forward);
        data_map.clear();
        cursor_for_each(table_cursor, save_all_data_map);
        REQUIRE(data_map.rbegin()->second == "Valine");
    }
}

TEST_CASE("Overflow pages") {
    const TempDirectory tmp_dir;
    EnvConfig db_config{tmp_dir.path().string(), /*create*/ true};
    db_config.inmemory = true;
    auto env{open_env(db_config)};
    auto txn{RWTxn(env)};

    const MapConfig test_map{"test"};

    SECTION("No overflow") {
        Cursor target(txn, test_map);
        Bytes key(20, '\0');
        Bytes value(max_value_size_for_leaf_page(*txn, key.size()), '\0');
        target.insert(to_slice(key), to_slice(value));
        txn.commit(/*renew=*/true);
        target.bind(txn, test_map);
        auto stats{target.get_map_stat()};
        REQUIRE(!stats.ms_overflow_pages);
    }

    SECTION("Let's overflow") {
        Cursor target(txn, test_map);
        Bytes key(20, '\0');
        Bytes value(db::max_value_size_for_leaf_page(*txn, key.size()) + /*any extra value */ 1, '\0');
        target.insert(db::to_slice(key), db::to_slice(value));
        txn.commit(/*renew=*/true);
        target.bind(txn, test_map);
        auto stats{target.get_map_stat()};
        REQUIRE(stats.ms_overflow_pages);
    }
}
}  // namespace zen::db
