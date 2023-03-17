/*
   Copyright 2022 The Silkworm Authors
   Copyright 2023 Horizen Labs
   Distributed under the MIT software license, see the accompanying
   file COPYING or http://www.opensource.org/licenses/mit-license.php.
*/

#include <fstream>

#include <catch2/catch.hpp>

#include <zen/core/common/misc.hpp>

#include <zen/node/common/directories.hpp>
#include <zen/node/common/log_test.hpp>

namespace zen {

TEST_CASE("Process Path", "[misc]") {
    const auto process_path{get_process_absolute_full_path()};
    CHECK(process_path.has_filename());
#if defined(_MSC_VER)
    CHECK(process_path.has_extension());
#endif
    ZEN_LOG << "Running tests from " << process_path.string() << " in " << process_path.parent_path().string();
}

TEST_CASE("Directory", "[misc]") {
    SECTION("Directory in current dir") {
        const auto process_path{get_process_absolute_full_path()};
        Directory current_dir(process_path.parent_path());
        ZEN_LOG << "Accessed directory " << current_dir.path().string();
        CHECK(current_dir.exists());
        CHECK_FALSE(current_dir.is_pristine());
        const auto current_dir_size{current_dir.size(/*recurse=*/true)};
        CHECK(current_dir_size > 0);

        std::string random_name{get_random_alpha_string(15)};
        auto sub_dir{current_dir[std::filesystem::path(random_name)]};
        ZEN_LOG << "Accessed sub directory " << sub_dir.path().string();
        CHECK(sub_dir.exists());
        CHECK(sub_dir.is_pristine());

        // Drop a file into sub_dir
        {
            std::string filename{sub_dir.path().string() + "/fake.txt"};
            std::ofstream f(filename.c_str());
            f << "Some fake text" << std::flush;
            f.close();
        }

        CHECK_FALSE(sub_dir.is_pristine());
        const auto sub_dir_size{sub_dir.size(/*recurse=*/false)};
        CHECK(sub_dir_size > 0);

        CHECK(current_dir.size(/*recurse=*/true) == current_dir_size + sub_dir_size);
        sub_dir.clear(/*recurse=*/true);
        CHECK(sub_dir.is_pristine());

        std::filesystem::remove_all(sub_dir.path());
        CHECK_FALSE(sub_dir.exists());
    }

    SECTION("Create Subdir from absolute path") {
        bool has_thrown{false};
        try {
            const auto process_path{get_process_absolute_full_path()};
            Directory current_dir(process_path.parent_path());
            std::string random_name{get_random_alpha_string(15)};
            std::filesystem::path sub_path{current_dir.path() / random_name};
            ZEN_LOG << "Using sub dir path " << sub_path.string() << " "
                    << (sub_path.is_absolute() ? "absolute" : "relative");
            auto sub_dir{current_dir[sub_path]};
            ZEN_LOG << "Used sub dir path " << sub_dir.path().string();
            CHECK(sub_dir.exists());
        } catch (const std::invalid_argument&) {
            has_thrown = true;
        }
        CHECK(has_thrown);
    }
}

TEST_CASE("Temp Directory", "[misc]") {
    SECTION("Create from current process path") {
        const auto process_path{get_process_absolute_full_path()};
        std::filesystem::path tmp_generated_path;

        {
            TempDirectory tmp_dir(process_path.parent_path());
            tmp_generated_path = tmp_dir.path();
            ZEN_LOG << "Generated tmp directory " << tmp_generated_path.string();
            CHECK(tmp_dir.is_pristine());
        }
        CHECK_FALSE(std::filesystem::exists(tmp_generated_path));
    }
    SECTION("Create from null") {
        std::filesystem::path tmp_generated_path;
        std::filesystem::path os_tmp_path{get_os_temporary_path()};
        {
            TempDirectory tmp_dir{};
            tmp_generated_path = tmp_dir.path();
            ZEN_LOG << "Generated tmp directory " << tmp_generated_path.string();
            CHECK(tmp_dir.is_pristine());
        }
        CHECK(tmp_generated_path.string().starts_with(os_tmp_path.string()));
        CHECK_FALSE(std::filesystem::exists(tmp_generated_path));
    }
}

TEST_CASE("Data Directory", "[misc]") {
    const auto os_storage_path{get_os_default_storage_path()};
    std::filesystem::path zen_data_dir{};
    {
        DataDirectory data_dir{os_storage_path};
        zen_data_dir = data_dir.path();
        CHECK_FALSE(std::filesystem::exists(data_dir.path() / "chaindata"));
        CHECK_FALSE(std::filesystem::exists(data_dir.path() / "etl-tmp"));
        CHECK_FALSE(std::filesystem::exists(data_dir.path() / "nodes"));

        data_dir.deploy();
        CHECK(std::filesystem::exists(data_dir.path() / "chaindata"));
        CHECK(std::filesystem::exists(data_dir.path() / "etl-tmp"));
        CHECK(std::filesystem::exists(data_dir.path() / "nodes"));

        data_dir.clear(true);
        CHECK(std::filesystem::exists(data_dir.path() / "chaindata"));
        CHECK(std::filesystem::exists(data_dir.path() / "etl-tmp"));
        CHECK(std::filesystem::exists(data_dir.path() / "nodes"));
    }

    // After destruction the path should be still in place
    CHECK(std::filesystem::exists(zen_data_dir / "chaindata"));
    CHECK(std::filesystem::exists(zen_data_dir / "etl-tmp"));
    CHECK(std::filesystem::exists(zen_data_dir / "nodes"));

    // Clean up
    std::ignore = std::filesystem::remove_all(zen_data_dir);
}

}  // namespace zen
