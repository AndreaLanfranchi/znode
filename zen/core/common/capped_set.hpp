/*
   Copyright 2023 Horizen Labs
   Distributed under the MIT software license, see the accompanying
   file COPYING or http://www.opensource.org/licenses/mit-license.php.
*/

#pragma once
#include <queue>
#include <unordered_set>

#include <zen/core/common/assert.hpp>

namespace zen {

//! \brief  An STL-like set container capped in size
//! \details When container reaches capacity every insertion evicts the oldest element
//! \remark Not thread safe
template <typename T>
class CappedSet {
    using iterator_t = typename std::unordered_set<T>::iterator;

  public:
    explicit CappedSet(const size_t capacity) : capacity_{capacity} {
        ZEN_ASSERT(capacity > 0);  // Can't create zero capped container
    }
    ~CappedSet() = default;

    std::pair<iterator_t, bool> insert(const T& item) {
        if (auto it{items_.find(item)}; it != items_.end()) {
            return {it, false};
        }

        // We will insert - make room if necessary
        if (items_.size() == capacity_) {
            items_.erase(items_queue_.front());
            items_queue_.pop();
        }

        auto ret{items_.insert(item)};
        items_queue_.push(ret.first);
        return ret;
    }

    iterator_t begin() { return items_.begin(); }
    iterator_t end() { return items_.end(); }

    bool contains(const T& item) { return items_.contains(item); }

    [[nodiscard]] size_t capacity() const noexcept { return capacity_; }
    [[nodiscard]] size_t size() const noexcept { return items_.size(); }
    [[nodiscard]] bool empty() const noexcept { return items_.empty(); }
    void clear() noexcept {
        items_.clear();
        std::queue<typename std::unordered_set<T>::iterator> empty_queue;
        std::swap(items_queue_, empty_queue);
    }

    friend bool operator==(const CappedSet<T>& Lhs_, const CappedSet<T>& Rhs_) { return Lhs_.items_ == Rhs_.items_; }

  private:
    size_t capacity_;
    std::unordered_set<T> items_{};
    std::queue<iterator_t> items_queue_;
};

}  // namespace zen
