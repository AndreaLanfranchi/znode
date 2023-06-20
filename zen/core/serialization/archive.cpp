/*
   Copyright 2009-2010 Satoshi Nakamoto
   Copyright 2009-2014 The Bitcoin Core developers
   Copyright 2023 Horizen Labs
   Distributed under the MIT software license, see the accompanying
   file COPYING or http://www.opensource.org/licenses/mit-license.php.
*/

#include <zen/core/encoding/hex.hpp>
#include <zen/core/serialization/archive.hpp>

namespace zen::serialization {

void Archive::reserve(size_type count) { buffer_.reserve(count); }

void Archive::resize(size_type new_size, value_type item) { buffer_.resize(new_size, item); }

void Archive::write(ByteView data) { buffer_.append(data); }

void Archive::write(const uint8_t* const ptr, Archive::size_type count) { write({ptr, count}); }

Archive::iterator_type Archive::begin() {
    auto ret{buffer_.begin()};
    std::advance(ret, read_position_);
    return ret;
}

Archive::iterator_type Archive::end() { return buffer_.end(); }

void Archive::insert(iterator_type where, value_type item) { buffer_.insert(where, item); }

void Archive::erase(iterator_type where) { buffer_.erase(where); }

void Archive::push_back(uint8_t byte) { buffer_.push_back(byte); }

tl::expected<ByteView, Error> Archive::read(size_t count) {
    auto next_read_position{read_position_ + count};
    if (next_read_position > buffer_.length()) {
        return tl::unexpected(Error::kReadBeyondData);
    }
    ByteView ret(&buffer_[read_position_], count);
    std::swap(read_position_, next_read_position);
    return ret;
}

void Archive::skip(Archive::size_type count) noexcept {
    read_position_ = std::min(read_position_ + count, buffer_.size());
}

bool Archive::eof() const noexcept { return read_position_ >= buffer_.size(); }

Archive::size_type Archive::tellp() const noexcept { return read_position_; }

void Archive::seekp(size_type p) noexcept { read_position_ = std::min(p, buffer_.size()); }

std::string Archive::to_string() const { return zen::hex::encode({buffer_.data(), buffer_.size()}, false); }

void Archive::shrink() {
    buffer_.erase(0, read_position_);
    read_position_ = 0;
}

Archive::size_type Archive::size() const noexcept { return buffer_.size(); }

bool Archive::empty() const noexcept { return buffer_.empty(); }

Archive::size_type Archive::computed_size() const noexcept { return computed_size_; }

Archive::size_type Archive::avail() const noexcept { return buffer_.size() - read_position_; }

void Archive::clear() noexcept {
    buffer_.clear();
    read_position_ = 0;
    computed_size_ = 0;
}

void Archive::get_clear(Archive& dst) {
    dst.write({&buffer_[read_position_], avail()});
    clear();
}
}  // namespace zen::serialization
