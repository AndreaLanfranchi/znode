/*
   Copyright 2009-2010 Satoshi Nakamoto
   Copyright 2009-2013 The Bitcoin Core developers
   Copyright 2022 The Silkworm Authors
   Copyright 2023 Horizen Labs
   Distributed under the MIT software license, see the accompanying
   file COPYING or http://www.opensource.org/licenses/mit-license.php.
*/

#pragma once
#include <cstdint>
#include <map>
#include <mutex>

#include <boost/noncopyable.hpp>

#include <zen/core/common/assert.hpp>
#include <zen/core/common/base.hpp>

namespace zen {

//! \brief The amount of memory currently being used by this process, in bytes.
//! \remarks if resident=true it will report the resident set in RAM (if supported specific OS) otherwise returns the
//! full virtual arena
[[nodiscard]] size_t get_mem_usage(bool resident = true);

//! \brief Returns system's page size in bytes
[[nodiscard]] size_t get_system_page_size();

//! \brief Fills ptr of size with a string of 0s
void memory_cleanse(void* ptr, size_t size);

//! \brief A manager for locked memory pages (prevents memory from being paged out)
//! \remarks Thread safe
template <class Locker>
class LockedPagesManagerBase : private boost::noncopyable {
  public:
    LockedPagesManagerBase() : LockedPagesManagerBase(get_system_page_size()){};

    explicit LockedPagesManagerBase(size_t page_size) : page_size_{page_size} {
        ZEN_ASSERT((page_size >= 512 && page_size <= 1_GiB));  // Martian values
        ZEN_ASSERT((page_size & (page_size - 1)) == 0);        // Must be power of two
        page_mask_ = ~(page_size - 1);
    };

    ~LockedPagesManagerBase() { clear(); }

    //! \brief For all pages in affected range, increase lock count
    bool lock_range(const void* address, const size_t size) noexcept {
        if (address == nullptr || size == 0) return false;
        bool ret{true};
        std::scoped_lock lock(mutex_);
        const auto [start, end]{get_range_boundaries(address, size)};
        for (size_t page{start}; page <= end; page += page_size_) {
            if (auto item = locked_pages_.find(page); item != locked_pages_.end()) {
                ++item->second;
                continue;
            }
            // Try lock the page
            if (locker_.lock(reinterpret_cast<void*>(page), page_size_)) {
                locked_pages_.insert({page, 1});
            } else {
                // Failed to lock the page
                ret = false;
                break;
            }
        }
        return ret;
    }

    //! \brief For all pages in affected range, decrease lock count
    bool unlock_range(const void* address, const size_t size) noexcept {
        if (address == nullptr || size == 0) return false;
        bool ret{true};
        std::scoped_lock lock(mutex_);
        const auto [start, end]{get_range_boundaries(address, size)};
        for (size_t page{start}; page <= end; page += page_size_) {
            auto item = locked_pages_.find(page);
            if (item == locked_pages_.end()) continue;  // Not previously locked
            ZEN_ASSERT(item->second > 0);               // It MUST have a value
            if (--item->second == 0) {                  // Decrement lock count
                if (!locker_.unlock(reinterpret_cast<void*>(page), page_size_)) {
                    ret = false;
                    ++item->second;
                    break;
                }
                locked_pages_.erase(item);
            }
        }
        return ret;
    }

    //! \brief Returns the number of locked pages
    [[nodiscard]] size_t size() const noexcept {
        std::scoped_lock lock(mutex_);
        return locked_pages_.size();
    }

    //! \brief Whether this has locked pages
    [[nodiscard]] bool empty() const noexcept {
        std::scoped_lock lock(mutex_);
        return locked_pages_.empty();
    }

    //! \brief Whether a specific page address has been locked
    [[nodiscard]] bool contains(const size_t address) const noexcept {
        std::scoped_lock lock(mutex_);
        return locked_pages_.contains(address & page_mask_);
    }

    //! \brief Removes all locks
    void clear() noexcept {
        std::scoped_lock lock(mutex_);
        for (auto it{locked_pages_.cbegin()}; it != locked_pages_.cend();) {
            if (locker_.unlock(reinterpret_cast<void*>(it->first), page_size_)) {
                locked_pages_.erase(it++);
            } else {
                ++it;
            }
        }
    }

  private:
    Locker locker_;
    mutable std::mutex mutex_;
    size_t page_size_{0};
    size_t page_mask_{0};
    std::map<size_t, intptr_t> locked_pages_;

    [[nodiscard]] std::pair<size_t, size_t> get_range_boundaries(const void* address, size_t size) const {
        const auto base_address = reinterpret_cast<size_t>(address);
        const auto start_page = base_address & page_mask_;
        const auto end_page = (base_address + size - 1) & page_mask_;
        return {start_page, end_page};
    }
};

//! \brief OS-dependent memory page locking/unlocking.
//! \remarks Not to be used as stand-alone instance
//! \remarks Defined as policy class to make stubbing for test possible.
class MemoryPageLocker {
  public:
    MemoryPageLocker() = default;
    ~MemoryPageLocker() = default;

    //! \brief Locks memory pages
    //! \remarks addr and len must be multiple of the system's page size
    bool lock(const void* addr, size_t len);

    //! \brief Unlocks memory pages
    //! \remarks addr and len must be multiple of the system's page size
    bool unlock(const void* addr, size_t len);
};

class LockedPagesManager final : public LockedPagesManagerBase<MemoryPageLocker> {
  public:
    static LockedPagesManager& instance() {
        std::call_once(create_instance_once_, LockedPagesManager::create_instance);
        return *LockedPagesManager::instance_;
    }

  private:
    LockedPagesManager() : LockedPagesManagerBase<MemoryPageLocker>(get_system_page_size()){};

    static void create_instance() {
        // Using a local static instance guarantees that the object is initialized
        // when it's first needed and also deinitialized after all objects that use
        // it are done with it.
        static LockedPagesManager instance;
        LockedPagesManager::instance_ = &instance;
    }
    static LockedPagesManager* instance_;  // Singleton
    static std::once_flag create_instance_once_;
};

//! \brief Directly locks memory objects.
//! \remarks Intended for non-dynamically allocated objects
template <typename T>
bool lock_object_memory(const T& t) {
    return LockedPagesManager::instance().lock_range(static_cast<const void*>(&t), sizeof(t));
}

//! \brief Directly unlocks memory objects.
//! \remarks Intended for non-dynamically allocated objects
template <typename T>
bool unlock_object_memory(const T& t) {
    auto ptr = const_cast<void*>(reinterpret_cast<const void*>(&t));
    memory_cleanse(ptr, sizeof(t));
    return LockedPagesManager::instance().unlock_range(ptr, sizeof(t));
}
}  // namespace zen
