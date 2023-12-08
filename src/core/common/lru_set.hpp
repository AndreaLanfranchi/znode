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
#include <list>
#include <mutex>
#include <unordered_map>

namespace znode {

//! \brief A thread-safe LRU set with capped size
//! \details Every time an item is added to the set it is moved to the front of the list.
//! If the set is full, the last item is removed from the list. The set is thread-safe if the template parameter
//! is set to true.
template <class Key, class Hasher = std::hash<Key>>
class LruSet {
  public:
    LruSet() = delete;
    explicit LruSet(size_t max_size, bool thread_safe = false) : max_size_{max_size}, thread_safe_{thread_safe} {}

    //! \brief Adds an item to the set
    //! \return true if the item was added, false if it was already present
    bool insert(const Key& item) {
        std::unique_lock lock(mutex_, std::defer_lock);
        if (thread_safe_) lock.lock();
        const auto it = map_.find(item);
        if (it != map_.end()) {
            list_.splice(list_.begin(), list_, it->second);
            return false;
        }
        map_[item] = list_.insert(list_.begin(), item);
        if (list_.size() > max_size_) {
            map_.erase(list_.back());
            list_.pop_back();
        }
        return true;
    }

    [[nodiscard]] Key front() const {
        std::unique_lock lock(mutex_, std::defer_lock);
        if (thread_safe_) lock.lock();
        return list_.front();
    }

    [[nodiscard]] Key back() const {
        std::unique_lock lock(mutex_, std::defer_lock);
        if (thread_safe_) lock.lock();
        return list_.back();
    }

    [[nodiscard]] bool contains(const Key& item) const {
        std::unique_lock lock(mutex_, std::defer_lock);
        if (thread_safe_) lock.lock();
        return map_.contains(item);
    }

    [[nodiscard]] size_t size() const {
        std::unique_lock lock(mutex_, std::defer_lock);
        if (thread_safe_) lock.lock();
        return list_.size();
    }

    [[nodiscard]] size_t max_size() const { return max_size_; }

    [[nodiscard]] bool empty() const {
        std::unique_lock lock(mutex_, std::defer_lock);
        if (thread_safe_) lock.lock();
        return list_.empty();
    }

    [[nodiscard]] std::list<Key> items() const {
        std::unique_lock lock(mutex_, std::defer_lock);
        if (thread_safe_) lock.lock();
        return list_;
    }

    void clear() {
        std::unique_lock lock(mutex_, std::defer_lock);
        if (thread_safe_) lock.lock();
        list_.clear();
        map_.clear();
    }

  private:
    const size_t max_size_;
    const bool thread_safe_;
    std::list<Key> list_;
    std::unordered_map<Key, typename std::list<Key>::iterator, Hasher> map_;
    mutable std::mutex mutex_;
};

}  // namespace znode
