/*
   Copyright 2022 The Silkworm Authors
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

#include "infra/filesystem/directories.hpp"

#include <filesystem>
#include <memory>

#include <CLI/CLI.hpp>
#include <absl/container/btree_map.h>
#include <boost/format.hpp>
#include <magic_enum.hpp>

#include <core/common/cast.hpp>
#include <core/common/misc.hpp>

#include <infra/common/common.hpp>
#include <infra/common/log.hpp>
#include <infra/database/mdbx.hpp>
#include <infra/database/mdbx_tables.hpp>
#include <infra/network/addresses.hpp>
#include <infra/os/signals.hpp>

namespace fs = std::filesystem;
using namespace znode;
using namespace std::placeholders;

struct dbTableEntry {
    MDBX_dbi id{0};
    std::string name{};
    mdbx::txn::map_stat stat;
    mdbx::map_handle::info info;
    [[nodiscard]] size_t pages() const noexcept {
        return stat.ms_branch_pages + stat.ms_leaf_pages + stat.ms_overflow_pages;
    }
    [[nodiscard]] size_t size() const noexcept { return pages() * stat.ms_psize; }
};

struct dbTablesInfo {
    size_t mapsize{0};
    size_t filesize{0};
    size_t pageSize{0};
    size_t pages{0};
    size_t size{0};
    std::vector<dbTableEntry> tables{};
};

struct dbFreeEntry {
    size_t id{0};
    size_t pages{0};
    size_t size{0};
};

struct dbFreeInfo {
    size_t pages{0};
    size_t size{0};
    std::vector<dbFreeEntry> entries{};
};

dbFreeInfo get_freeInfo(::mdbx::txn& txn) {
    dbFreeInfo ret{};

    const ::mdbx::map_handle free_map{0};
    auto page_size{txn.get_map_stat(free_map).ms_psize};

    const auto collect_func{[&ret, &page_size](ByteView key, ByteView value) {
        size_t txId;
        std::memcpy(&txId, key.data(), sizeof(size_t));
        uint32_t pagesCount;
        std::memcpy(&pagesCount, value.data(), sizeof(uint32_t));
        const size_t pagesSize = static_cast<size_t>(static_cast<uint64_t>(pagesCount) * page_size);
        ret.pages += pagesCount;
        ret.size += pagesSize;
        ret.entries.push_back(dbFreeEntry{txId, pagesCount, pagesSize});
    }};

    auto free_crs{txn.open_cursor(free_map)};
    (void)db::cursor_for_each(free_crs, collect_func);

    return ret;
}

dbTablesInfo get_tablesInfo(::mdbx::txn& txn) {
    dbTablesInfo ret{};
    std::unique_ptr<dbTableEntry> table;

    ret.filesize = txn.env().get_info().mi_geo.current;

    // Get info from the free database
    const mdbx::map_handle free_map{0};
    auto stat = txn.get_map_stat(free_map);
    auto info = txn.get_handle_info(free_map);
    table.reset(new dbTableEntry{free_map.dbi, "FREE_DBI", stat, info});
    ret.pageSize += table->stat.ms_psize;
    ret.pages += table->pages();
    ret.size += table->size();
    ret.tables.push_back(*table);

    // Get info from the unnamed database
    const mdbx::map_handle main_map{1};
    stat = txn.get_map_stat(main_map);
    info = txn.get_handle_info(main_map);
    table.reset(new dbTableEntry{main_map.dbi, "MAIN_DBI", stat, info});
    ret.pageSize += table->stat.ms_psize;
    ret.pages += table->pages();
    ret.size += table->size();
    ret.tables.push_back(*table);

    const auto& collect_func{[&ret, &txn](ByteView key, ByteView) {
        const auto name{std::string(byte_view_to_string_view(key))};
        const auto map{txn.open_map(name)};
        const auto stat2{txn.get_map_stat(map)};
        const auto info2{txn.get_handle_info(map)};
        const auto* table2 = new dbTableEntry{map.dbi, std::string{name}, stat2, info2};

        ret.pageSize += table2->stat.ms_psize;
        ret.pages += table2->pages();
        ret.size += table2->size();
        ret.tables.push_back(*table2);
    }};

    // Get all tables from the unnamed database
    auto main_crs{txn.open_cursor(main_map)};
    db::cursor_for_each(main_crs, collect_func);
    return ret;
}

void do_list_tables(const db::EnvConfig& config) {
    static const std::string fmt_hdr{" %3s %-24s %10s %2s %10s %10s %10s %12s %10s %10s"};
    static const std::string fmt_row{" %3i %-24s %10u %2u %10u %10u %10u %12s %10s %10s"};

    auto env{db::open_env(config)};
    db::ROTxn txn(env);

    auto dbTablesInfo{get_tablesInfo(*txn)};
    auto dbFreeInfo{get_freeInfo(*txn)};

    std::cout << "\n Database tables    : " << dbTablesInfo.tables.size()
              << "\n Database page size : " << to_human_bytes(env.get_pagesize(), true) << "\n"
              << std::endl;
    // std::cout << " Effective pruning        : " << db::read_prune_mode(txn).to_string() << "\n" << std::endl;

    if (!dbTablesInfo.tables.empty()) {
        std::cout << (boost::format(fmt_hdr) % "Dbi" % "Table name" % "Records" % "D" % "Branch" % "Leaf" % "Overflow" %
                      "Size" % "Key" % "Value")
                  << std::endl;
        std::cout << (boost::format(fmt_hdr) % std::string(3, '-') % std::string(24, '-') % std::string(10, '-') %
                      std::string(2, '-') % std::string(10, '-') % std::string(10, '-') % std::string(10, '-') %
                      std::string(12, '-') % std::string(10, '-') % std::string(10, '-'))
                  << std::endl;

        for (const auto& item : dbTablesInfo.tables) {
            auto keyMode = magic_enum::enum_name(item.info.key_mode());
            auto valueMode = magic_enum::enum_name(item.info.value_mode());
            std::cout << (boost::format(fmt_row) % item.id % item.name % item.stat.ms_entries % item.stat.ms_depth %
                          item.stat.ms_branch_pages % item.stat.ms_leaf_pages % item.stat.ms_overflow_pages %
                          to_human_bytes(item.size(), true) % keyMode % valueMode)
                      << std::endl;
        }
    }

    std::cout << "\n"
              << " Database file size   (A) : " << (boost::format("%13s") % to_human_bytes(dbTablesInfo.filesize, true))
              << "\n"
              << " Data pages count         : " << (boost::format("%13u") % dbTablesInfo.pages) << "\n"
              << " Data pages size      (B) : " << (boost::format("%13s") % to_human_bytes(dbTablesInfo.size, true))
              << "\n"
              << " Free pages count         : " << (boost::format("%13u") % dbFreeInfo.pages) << "\n"
              << " Free pages size      (C) : " << (boost::format("%13s") % to_human_bytes(dbFreeInfo.size, true))
              << "\n"
              << " Reclaimable space        : "
              << (boost::format("%13s") %
                  to_human_bytes(dbTablesInfo.filesize - dbTablesInfo.size + dbFreeInfo.size, true))
              << " == A - B + C \n"
              << std::endl;

    txn.abort();
    env.close(config.shared);
}

void do_list_address_types(const db::EnvConfig& config) {
    auto env{db::open_env(config)};
    auto txn{env.start_read()};
    if (!db::has_map(txn, db::tables::kServices.name)) {
        throw std::runtime_error("No \"Services\" table found");
    }

    std::unordered_map<net::IPAddressType, size_t> histogram;
    auto cursor{db::open_cursor(txn, db::tables::kServices)};
    cursor.to_first(/*throw_not_found=*/false);

    db::cursor_for_each(cursor, [&histogram](ByteView /*key*/, ByteView value) {
        ser::SDataStream data(value, ser::Scope::kStorage, 0);
        net::NodeServiceInfo info{};
        success_or_throw(info.deserialize(data));
        histogram[info.service_.endpoint_.address_.get_type()]++;
    });

    // Sort histogram by usage (from most used to less used)
    std::vector<std::pair<net::IPAddressType, size_t>> histogram_sorted;
    std::copy(histogram.begin(), histogram.end(),
              std::back_inserter<std::vector<std::pair<net::IPAddressType, size_t>>>(histogram_sorted));
    std::sort(histogram_sorted.begin(), histogram_sorted.end(),
              [](std::pair<net::IPAddressType, size_t>& a, std::pair<net::IPAddressType, size_t>& b) -> bool {
                  return a.second == b.second ? a.first < b.first : a.second > b.second;
              });

    if (!histogram_sorted.empty()) {
        std::cout << "\n"
                  << (boost::format(" %-6s %8s") % "Type" % "Count") << "\n"
                  << (boost::format(" %-6s %8s") % std::string(6, '-') % std::string(8, '-')) << "\n";
        size_t total_usage_count{0};
        for (const auto& [address_type, usage_count] : histogram_sorted) {
            std::cout << (boost::format(" %-6s %8u") % magic_enum::enum_name(address_type) % usage_count) << "\n";
            total_usage_count += usage_count;
        }
        std::cout << (boost::format(" %-6s %8s") % std::string(6, '-') % std::string(8, '-')) << "\n"
                  << (boost::format(" %-6s %8u") % " " % total_usage_count) << "\n"
                  << std::endl;
    }
}

int main(int argc, char* argv[]) {
    os::Signals::init();
    CLI::App app_main("Znode db tool");
    app_main.get_formatter()->column_width(50);
    app_main.require_subcommand(1);      // At least 1 subcommand is required
    const log::Settings log_settings{};  // Holds logging settings

    /*
     * Database options (path required)
     */
    std::string data_dir{};

    auto* db_opts = app_main.add_option_group("Db", "Database options");
    db_opts->get_formatter()->column_width(35);
    std::ignore = db_opts->add_option("--datadir", data_dir, "Path to database")->capture_default_str();
    auto* nodes_opt = db_opts->add_flag("--nodes", "Open nodes database");
    auto* shared_opt = db_opts->add_flag("--shared", "Open database in shared mode");
    auto* exclusive_opt = db_opts->add_flag("--exclusive", "Open database in exclusive mode")->excludes(shared_opt);

    /*
     * Common opts and flags
     */

    // auto app_yes_opt = app_main.add_flag("-Y,--yes", "Assume yes to all requests of confirmation");
    // auto app_dry_opt = app_main.add_flag("--dry", "Don't commit to db. Only simulate");

    /*
     * Subcommands
     */

    auto* cmd_tables = app_main.add_subcommand("tables", "List db and tables info");
    auto* cmd_address_types = app_main.add_subcommand("addr_types", "List network addresses types");

    // auto cmd_tables_scan_opt = cmd_tables->add_flag("--scan", "Scan real data size (long)");

    /*
     * Parse arguments and validate
     */
    CLI11_PARSE(app_main, argc, argv)

    try {
        // Locate the db directory we're interested into
        if (data_dir.empty()) {
            if (!*nodes_opt) {
                data_dir = (DataDirectory::default_path() / DataDirectory::kChainDataName).string();
            } else {
                data_dir = (DataDirectory::default_path() / DataDirectory::kNodesName).string();
            }
        } else {
            // Verify that the provided path is a directory
            if (!fs::is_directory(data_dir)) {
                throw std::runtime_error("Invalid path: " + data_dir);
            }
            // Verify datadir has either chaindata or nodes subdir
            if (!*nodes_opt) {
                data_dir = (fs::path(data_dir) / DataDirectory::kChainDataName).string();
            } else {
                data_dir = (fs::path(data_dir) / DataDirectory::kNodesName).string();
            }
            if (!fs::is_directory(data_dir)) {
                throw std::runtime_error("Invalid database path: " + data_dir);
            }
        }

        db::EnvConfig src_config{data_dir};
        src_config.create = false;  // Database must exist
        src_config.shared = static_cast<bool>(*shared_opt);
        src_config.exclusive = static_cast<bool>(*exclusive_opt);

        // Execute the requested subcommand
        if (*cmd_tables) {
            do_list_tables(src_config);
        } else if (*cmd_address_types) {
            do_list_address_types(src_config);
        }
        return 0;

    } catch (const std::exception& ex) {
        std::cerr << "\nUnexpected \"" << typeid(ex).name() << "\" : " << ex.what() << "\n" << std::endl;
    } catch (...) {
        std::cerr << "\nUnexpected undefined error\n" << std::endl;
    }
    return -1;
}