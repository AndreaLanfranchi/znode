/*
   Copyright 2022 The Silkworm Authors
   Copyright 2023 Horizen Labs
   Distributed under the MIT software license, see the accompanying
   file COPYING or http://www.opensource.org/licenses/mit-license.php.
*/

#pragma once
#include <memory>
#include <mutex>
#include <stack>
#include <vector>

#include <boost/noncopyable.hpp>
#include <gsl/pointers>

namespace zenpp {

//! \brief A dynamic pool of objects usually expensive to create
template <class T, class TDtor = std::default_delete<T>>
class ObjectPool : private boost::noncopyable {
  public:
    explicit ObjectPool(bool thread_safe = false) : thread_safe_{thread_safe} {}

    void add(gsl::owner<T*> ptr) {
        std::unique_lock<std::mutex> lock(mutex_, std::defer_lock);
        if (thread_safe_) lock.lock();
        pool_.push({ptr, TDtor()});
    }

    gsl::owner<T*> acquire() noexcept {
        std::unique_lock<std::mutex> lock(mutex_, std::defer_lock);
        if (thread_safe_) lock.lock();
        if (pool_.empty()) return nullptr;
        gsl::owner<T*> ret(pool_.top().release());
        pool_.pop();
        return ret;
    }

    [[nodiscard]] bool empty() const noexcept {
        std::unique_lock<std::mutex> lock(mutex_, std::defer_lock);
        if (thread_safe_) lock.lock();
        return pool_.empty();
    }

    [[nodiscard]] size_t size() const noexcept {
        std::unique_lock<std::mutex> lock(mutex_, std::defer_lock);
        if (thread_safe_) lock.lock();
        return pool_.size();
    }

  private:
    using PointerType = std::unique_ptr<T, TDtor>;
    std::stack<PointerType, std::vector<PointerType>> pool_{};
    bool thread_safe_{false};
    mutable std::mutex mutex_;
};

}  // namespace zenpp
