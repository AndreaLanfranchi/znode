/*
   Copyright 2009-2010 Satoshi Nakamoto
   Copyright 2009-2014 The Bitcoin Core developers
   Copyright 2023 Horizen Labs
   Distributed under the MIT software license, see the accompanying
   file COPYING or http://www.opensource.org/licenses/mit-license.php.
*/

#include <zen/core/encoding/hex.hpp>
#include <zen/core/serialization/stream.hpp>

namespace zen::ser {

Scope DataStream::scope() const noexcept { return scope_; }

int DataStream::version() const noexcept { return version_; }

void DataStream::reserve(size_type count) { buffer_.reserve(count); }

void DataStream::resize(size_type new_size, value_type item) { buffer_.resize(new_size, item); }

void DataStream::write(ByteView data) { buffer_.append(data); }

void DataStream::write(const uint8_t* const ptr, DataStream::size_type count) { write({ptr, count}); }

DataStream::iterator_type DataStream::begin() {
    auto ret{buffer_.begin()};
    std::advance(ret, read_position_);
    return ret;
}

DataStream::iterator_type DataStream::end() { return buffer_.end(); }

void DataStream::insert(iterator_type where, value_type item) { buffer_.insert(where, item); }

void DataStream::erase(iterator_type where) { buffer_.erase(where); }

void DataStream::push_back(uint8_t byte) { buffer_.push_back(byte); }

tl::expected<ByteView, DeserializationError> DataStream::read(size_t count) {
    auto next_read_position{read_position_ + count};
    if (next_read_position > buffer_.length()) {
        return tl::unexpected(DeserializationError::kReadBeyondData);
    }
    ByteView ret(&buffer_[read_position_], count);
    std::swap(read_position_, next_read_position);
    return ret;
}

void DataStream::skip(DataStream::size_type count) noexcept {
    read_position_ = std::min(read_position_ + count, buffer_.size());
}

bool DataStream::eof() const noexcept { return read_position_ >= buffer_.size(); }

DataStream::size_type DataStream::tellp() const noexcept { return read_position_; }

void DataStream::seekp(size_type p) noexcept { read_position_ = std::min(p, buffer_.size()); }

std::string DataStream::to_string() const { return zen::hex::encode({buffer_.data(), buffer_.size()}, false); }

void DataStream::shrink() {
    buffer_.erase(0, read_position_);
    read_position_ = 0;
}

DataStream::size_type DataStream::size() const noexcept { return buffer_.size(); }

DataStream::size_type DataStream::avail() const noexcept { return buffer_.size() - read_position_; }

void DataStream::clear() noexcept {
    buffer_.clear();
    read_position_ = 0;
}

void DataStream::get_clear(DataStream& dst) {
    dst.write({&buffer_[read_position_], avail()});
    clear();
}
}  // namespace zen::ser
