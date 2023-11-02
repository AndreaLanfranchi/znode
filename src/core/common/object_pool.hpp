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

#pragma once
#include <memory>
#include <mutex>
#include <stack>
#include <vector>

#include <boost/noncopyable.hpp>
#include <gsl/pointers>

namespace znode {

//! \brief A dynamic pool of objects usually expensive to create
template <class T, class TDtor = std::default_delete<T>>
class ObjectPool : private boost::noncopyable {
  public:
    explicit ObjectPool(bool thread_safe = false) : thread_safe_{thread_safe} {}

    void add(gsl::owner<T*> ptr) {
        std::unique_lock lock(mutex_, std::defer_lock);
        if (thread_safe_) lock.lock();
        pool_.push({ptr, TDtor()});
    }

    gsl::owner<T*> acquire() noexcept {
        std::unique_lock lock(mutex_, std::defer_lock);
        if (thread_safe_) lock.lock();
        if (pool_.empty()) return nullptr;
        gsl::owner<T*> ret(pool_.top().release());
        pool_.pop();
        return ret;
    }

    [[nodiscard]] bool empty() const noexcept {
        std::unique_lock lock(mutex_, std::defer_lock);
        if (thread_safe_) lock.lock();
        return pool_.empty();
    }

    [[nodiscard]] size_t size() const noexcept {
        std::unique_lock lock(mutex_, std::defer_lock);
        if (thread_safe_) lock.lock();
        return pool_.size();
    }

  private:
    using PointerType = std::unique_ptr<T, TDtor>;
    std::stack<PointerType, std::vector<PointerType>> pool_{};
    bool thread_safe_{false};
    mutable std::mutex mutex_;
};

}  // namespace znode
