/*
   Copyright 2022 The Silkworm Authors
   Copyright 2023 Horizen Labs
   Distributed under the MIT software license, see the accompanying
   file COPYING or http://www.opensource.org/licenses/mit-license.php.
*/

#pragma once
#include <filesystem>
#include <optional>
#include <stdexcept>
#include <vector>

#include <boost/noncopyable.hpp>

namespace zen {

//! \brief Returns the path to OS provided temporary storage location
std::filesystem::path get_os_temporary_path();

//! \brief Returns the path to OS provided default storage location
std::filesystem::path get_os_default_storage_path();

//! \brief Returns the path of current process's executable
std::filesystem::path get_process_absolute_full_path();

//! \brief Builds a temporary path from OS provided temporary storage location
//! \param [in] base_path : the path of the directory we're pointing to
//! \remark If no base_path provided then it is derived from get_os_temporary_path
std::filesystem::path get_unique_temporary_path(std::optional<std::filesystem::path> base_path = std::nullopt);

//! \brief Directory class acts as a wrapper around common functions and properties of a filesystem directory object
class Directory : private boost::noncopyable {
  public:
    //! \brief Creates an instance of a Directory object provided the path
    //! \param [in] path : the path of the directory we're pointing to
    //! \remark Path MUST be absolute
    explicit Directory(const std::filesystem::path& path);
    virtual ~Directory() = default;

    //! \brief Returns whether this Directory is uncontaminated (i.e. brand new with no contents)
    [[nodiscard]] bool is_pristine() const;

    //! \brief Whether the path effectively exists on filesystem
    [[nodiscard]] bool exists() const;

    //! \brief Creates the filesystem entry if it does not exist
    void create();

    //! \brief Returns the cumulative size of all contained files and subdirectories
    //! \param [in] recursive : whether to recurse subdirectories
    [[nodiscard]] size_t size(bool recurse) const;

    //! \brief Returns the std::filesystem::path of this Directory instance
    [[nodiscard]] const std::filesystem::path& path() const noexcept;

    //! \brief Removes all contained files and, optionally, subdirectories
    virtual void clear(bool recurse);

    //! \brief Accesses a subdirectory.
    //! \param [in] path : a relative subpath
    //! \remark Should the requested dir not exist then it is created
    Directory operator[](const std::filesystem::path& path);

  private:
    std::filesystem::path path_;  // The actual absolute path of this instance
};

//! \brief TempDirectory is a Directory which is automatically deleted on destructor of the instance.
//! The full path of the directory starts from a given path plus the discovery of a unique non-existent sub-path
//! through a linear search. Should no initial path be given, TempDirectory is built from the path indicated
//! for temporary files storage by host OS environment variables
class TempDirectory final : public Directory {
  public:
    //! \brief Creates an instance of a TempDirectory from OS temporary path
    TempDirectory() : Directory(get_unique_temporary_path()) {}

    //! \brief Creates an instance of a TempDirectory from a user provided path
    //! \param [in] base_path :  A path where to append this instance of temporary directory. MUST be absolute
    explicit TempDirectory(const std::filesystem::path& path) : Directory(get_unique_temporary_path(path)){};

    ~TempDirectory() override {
        std::ignore = std::filesystem::remove_all(path());  // Remove self
    }
};

//! \brief DataDirectory wraps the directory tree used by Zen as base storage path.
//! A typical DataDirectory has the following subdirs
//! \verbatim
//! <base_path>
//! |-- chaindata <-- Where main chain database is stored
//! |-- etl-tmp   <-- Where temporary files from etl collector are stored
//! |-- nodes     <-- Where database for discovered nodes is stored
class DataDirectory final : public Directory {
  public:
    using Directory::Directory;
    ~DataDirectory() override = default;

    //! \brief Override DataDirectory's clear method to avoid accidental loss of data
    void clear(bool) override{};

    //! \brief Ensures all subdirs are properly created
    void deploy() {
        std::ignore = operator[]("chaindata");
        std::ignore = operator[]("etl-tmp");
        std::ignore = operator[]("nodes");
    }
};
}  // namespace zen
