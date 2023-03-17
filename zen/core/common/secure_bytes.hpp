/*
   Copyright 2009-2010 Satoshi Nakamoto
   Copyright 2009-2013 The Bitcoin Core developers
   Copyright 2023 Horizen Labs
   Distributed under the MIT software license, see the accompanying
   file COPYING or http://www.opensource.org/licenses/mit-license.php.
*/

#pragma once

#include <string>

#include <boost/noncopyable.hpp>

#include <zen/core/common/memory.hpp>

namespace zen {

//! \brief Custom allocator that ensures allocated memory is wiped out on deallocation and also locked against page-out
template <typename T>
struct secure_allocator : public std::allocator<T>, private boost::noncopyable {
    using base = std::allocator<T>;
    secure_allocator() noexcept = default;
    secure_allocator(const secure_allocator& a) noexcept : base(a) {}
    template <typename U>
    explicit secure_allocator(const secure_allocator<U>& a) noexcept : base(a) {}
    ~secure_allocator() noexcept = default;
    template <typename Other>
    struct rebind {
        using other = secure_allocator<Other>;
    };
    [[nodiscard]] constexpr T* allocate(size_t n) {
        if (T * ptr{base::allocate(n)}; ptr != nullptr) [[likely]] {
            std::ignore = LockedPagesManager::instance().lock_range(ptr, sizeof(T) * n);
            return ptr;
        }
        return nullptr;
    }

    constexpr void deallocate(T* ptr, size_t n) {
        if (ptr != nullptr) [[likely]] {
            memory_cleanse(ptr, sizeof(T) * n);
            std::ignore = LockedPagesManager::instance().unlock_range(ptr, sizeof(T) * n);
        }
        base::deallocate(ptr, n);
    }
};

//! \brief This is exactly like Bytes, but with a custom locked and secure allocator.
using SecureBytes = std::basic_string<uint8_t, std::char_traits<uint8_t>, secure_allocator<uint8_t>>;
}  // namespace zen
