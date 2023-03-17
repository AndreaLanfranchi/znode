/*
   Copyright 2023 Horizen Labs
   Distributed under the MIT software license, see the accompanying
   file COPYING or http://www.opensource.org/licenses/mit-license.php.
*/

#include <zen/core/common/cast.hpp>
#include <zen/core/crypto/hasher.hpp>

namespace zen::crypto {

void Hasher::update(ByteView data) noexcept {
    // If some room left in buffer fill it
    if (buffer_offset_ != 0) {
        const size_t room_size{std::min(buffer_.size() - buffer_offset_, data.size())};
        memcpy(&buffer_[buffer_offset_], data.data(), room_size);
        data.remove_prefix(room_size);  // Already consumed
        buffer_offset_ += room_size;
        total_bytes_ += room_size;
        if (buffer_offset_ == buffer_.size()) {
            transform(buffer_.data());
            buffer_offset_ = 0;
        }
    }

    // Process remaining data in chunks
    while (data.size() >= block_size_) {
        total_bytes_ += block_size_;
        transform(data.data());
        data.remove_prefix(block_size_);
    }

    // Accumulate leftover in buffer
    if (!data.empty()) {
        memcpy(&buffer_[0], data.data(), data.size());
        buffer_offset_ = data.size();
        total_bytes_ += data.size();
    }
}
void Hasher::update(std::string_view data) noexcept { update(string_view_to_byte_view(data)); }

}  // namespace zen::crypto
