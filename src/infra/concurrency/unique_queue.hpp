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

#include <condition_variable>
#include <mutex>
#include <optional>
#include <queue>
#include <stdexcept>
#include <unordered_set>

namespace znode::con {

//! \brief A thread safe queue of unique items with optional maximum capacity
//! \details When container reaches capacity every insertion evicts the oldest element (FIFO)
template <typename T>
class UniqueQueue {
  public:
    UniqueQueue() = default;
    ~UniqueQueue() = default;

    explicit UniqueQueue(const size_t capacity) : capacity_{capacity} {
        if (capacity == 0U) {
            throw std::invalid_argument("UniqueQueue Capacity must be greater than zero");
        }
    }

    [[nodiscard]] bool empty() const noexcept {
        std::scoped_lock lock(mutex_);
        return queue_.empty();
    }

    [[nodiscard]] size_t size() const noexcept {
        std::scoped_lock lock(mutex_);
        return queue_.size();
    }

    [[nodiscard]] bool contains(const T& item) const noexcept {
        std::scoped_lock lock(mutex_);
        return set_.contains(item);
    }

    [[nodiscard]] bool push(const T& item) {
        std::scoped_lock lock(mutex_);
        const bool inserted{set_.insert(item).second};
        if (inserted) {
            queue_.push(item);
            if (queue_.size() > capacity_) {
                set_.erase(queue_.front());
                queue_.pop();
            }
            cond_.notify_one();
        }
        return inserted;
    }

    [[nodiscard]] std::optional<T> pop() {
        std::scoped_lock lock(mutex_);
        if (queue_.empty()) {
            return std::nullopt;
        }

        T item{queue_.front()};
        queue_.pop();
        set_.erase(item);
        return item;
    }

    void clear() noexcept {
        std::scoped_lock lock(mutex_);
        std::queue<T> empty_queue;
        std::unordered_set<T> empty_set;
        std::swap(queue_, empty_queue);
        std::swap(set_, empty_set);
    }

  private:
    std::size_t capacity_{std::numeric_limits<std::size_t>::max()};
    std::queue<T> queue_;
    std::unordered_set<T> set_;
    mutable std::mutex mutex_;
    std::condition_variable cond_;
};

}  // namespace znode::con
