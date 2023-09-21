/*
   Copyright 2023 Horizen Labs
   Distributed under the MIT software license, see the accompanying
   file COPYING or http://www.opensource.org/licenses/mit-license.php.
*/

#pragma once

#include <condition_variable>
#include <mutex>
#include <optional>
#include <queue>
#include <stdexcept>
#include <unordered_set>

namespace zenpp::con {

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
        std::lock_guard lock(mutex_);
        return queue_.empty();
    }

    [[nodiscard]] size_t size() const noexcept {
        std::lock_guard lock(mutex_);
        return queue_.size();
    }

    [[nodiscard]] bool contains(const T& item) const noexcept {
        std::lock_guard lock(mutex_);
        return set_.contains(item);
    }

    [[nodiscard]] bool push(const T& item) {
        std::lock_guard lock(mutex_);
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
        std::unique_lock lock(mutex_);
        if (queue_.empty()) {
            return std::nullopt;
        }

        T item{queue_.front()};
        queue_.pop();
        set_.erase(item);
        return item;
    }

    void clear() noexcept {
        std::lock_guard lock(mutex_);
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

}  // namespace zenpp::con
