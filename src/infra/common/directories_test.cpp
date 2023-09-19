/*
   Copyright 2022 The Silkworm Authors
   Copyright 2023 Horizen Labs
   Distributed under the MIT software license, see the accompanying
   file COPYING or http://www.opensource.org/licenses/mit-license.php.
*/

#include <fstream>
#include <vector>

#include <catch2/catch.hpp>

#include <core/common/misc.hpp>

#include <infra/common/directories.hpp>
#include <infra/common/log_test.hpp>

namespace zenpp {

TEST_CASE("Process Path", "[misc]") {
    const auto process_path{get_process_absolute_full_path()};
    CHECK(process_path.has_filename());
#if defined(_MSC_VER)
    CHECK(process_path.has_extension());
#endif
    LOG_MESSAGE << "Running tests from " << process_path.string() << " in " << process_path.parent_path().string();
}

TEST_CASE("Directory", "[misc]") {
    SECTION("Directory in current dir") {
        const auto process_path{get_process_absolute_full_path()};
        Directory current_dir(process_path.parent_path());
        LOG_MESSAGE << "Accessed directory " << current_dir.path().string();
        CHECK(current_dir.exists());
        CHECK_FALSE(current_dir.is_pristine());
        const auto current_dir_size{current_dir.size(/*recurse=*/true)};
        CHECK(current_dir_size > 0);

        std::string random_name{get_random_alpha_string(15)};
        auto sub_dir{current_dir[std::filesystem::path(random_name)]};
        LOG_MESSAGE << "Accessed sub directory " << sub_dir.path().string();
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
            LOG_MESSAGE << "Using sub dir path " << sub_path.string() << " "
                        << (sub_path.is_absolute() ? "absolute" : "relative");
            auto sub_dir{current_dir[sub_path]};
            LOG_MESSAGE << "Used sub dir path " << sub_dir.path().string();
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
            LOG_MESSAGE << "Generated tmp directory " << tmp_generated_path.string();
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
            LOG_MESSAGE << "Generated tmp directory " << tmp_generated_path.string();
            CHECK(tmp_dir.is_pristine());
        }
        CHECK(tmp_generated_path.string().starts_with(os_tmp_path.string()));
        CHECK_FALSE(std::filesystem::exists(tmp_generated_path));
    }
}

TEST_CASE("Data Directory", "[misc]") {
    TempDirectory tmp_dir{};  // To not clash with existing data
    const std::vector<std::string> subdirs{
        std::string(DataDirectory::kChainDataName), std::string(DataDirectory::kEtlTmpName),
        std::string(DataDirectory::kNodesName), std::string(DataDirectory::kZkParamsName)};

    std::filesystem::path zen_data_dir{};
    {
        DataDirectory data_dir{tmp_dir.path()};
        zen_data_dir = data_dir.path();
        REQUIRE(data_dir.is_pristine());
        REQUIRE(data_dir.is_writable());

        // Deploy and verify all mandatory subdirs are present
        data_dir.deploy();
        for (auto& subdir : DataDirectory::kSubdirs) {
            CHECK(std::filesystem::exists(zen_data_dir / subdir));
        }

        // Clear and verify all mandatory subdirs are still there
        data_dir.clear(true);
        for (auto& subdir : DataDirectory::kSubdirs) {
            CHECK(std::filesystem::exists(zen_data_dir / subdir));
        }
    }

    // After destruction of DataDirectory the paths should be still in place
    for (const auto& subdir : subdirs) {
        CHECK(std::filesystem::exists(zen_data_dir / subdir));
    }
}
}  // namespace zenpp
