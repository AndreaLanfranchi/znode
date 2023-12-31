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

#pragma once

#include <array>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <string_view>

#include <boost/asio.hpp>
#include <boost/asio/ssl.hpp>

#include <core/common/base.hpp>

namespace znode::zk {

struct ParamFile {
    const std::string_view name{};               // Name of the file (e.g. sprout-proving.key)
    const std::string_view expected_checksum{};  // SHA256 checksum of the file
    const uintmax_t expected_size{0};            // Size of the file in bytes
};
static constexpr std::string_view kTrustedDownloadHost{"downloads.horizen.io"};
static constexpr std::string_view kTrustedDownloadPath{"/file/TrustedSetup/"};

static constexpr std::string_view kTrustedDownloadBaseUrl{"https://downloads.horizen.io/file/TrustedSetup/"};

static constexpr ParamFile kSproutProvingKey{
    "sprout-proving.key", "8bc20a7f013b2b58970cddd2e7ea028975c88ae7ceb9259a5344a16bc2c0eef7", 910173851};

static constexpr ParamFile kSproutVerifyingKey{
    "sprout-verifying.key", "4bd498dae0aacfd8e98dc306338d017d9c08dd0918ead18172bd0aec2fc5df82", 1449};

static constexpr ParamFile kSproutGroth16Params{
    "sprout-groth16.params", "b685d700c60328498fbde589c8c7c484c722b788b265b72af448a5bf0ee55b50", 725523612};

static constexpr ParamFile kSaplingOutputParams{
    "sapling-output.params", "2f0ebbcbb9bb0bcffe95a397e7eba89c29eb4dde6191c339db88570e3f3fb0e4", 3592860};

static constexpr ParamFile kSaplingSpendParams{
    "sapling-spend.params", "8e48ffd23abb3a5fd9c5589204f32d9c31285a04b78096ba40a79b75677efc13", 47958396};

static constexpr std::array<ParamFile, 5> kParamFiles{kSproutProvingKey, kSproutVerifyingKey, kSaplingOutputParams,
                                                      kSaplingSpendParams, kSproutGroth16Params};

//! \brief Validate the existence and correctness of the params files in the given directory
bool validate_param_files(boost::asio::io_context& asio_context, const std::filesystem::path& directory,
                          bool no_checksums);

//! \brief Download the params files from the trusted source and save them in the given directory
bool download_param_file(boost::asio::io_context& asio_context, const std::filesystem::path& directory,
                         const ParamFile& param_file);

//! \brief Computes the SHA256 checksum of the given file
//! \remarks Throws std::runtime_error if the file cannot be opened
std::optional<Bytes> get_file_sha256_checksum(const std::filesystem::path& file_path);

//! \brief Validates the checksum of the given file against the expected one
bool validate_file_checksum(const std::filesystem::path& file_path, ByteView expected_checksum);

}  // namespace znode::zk