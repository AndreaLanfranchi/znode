/*
   Copyright 2009-2010 Satoshi Nakamoto
   Copyright 2009-2014 The Bitcoin Core developers
   Copyright 2023 Horizen Labs
   Distributed under the MIT software license, see the accompanying
   file COPYING or http://www.opensource.org/licenses/mit-license.php.
*/

#include <utility>

#include <zen/core/common/overflow.hpp>
#include <zen/core/encoding/hex.hpp>
#include <zen/core/serialization/stream.hpp>

namespace zen::serialization {

DataStream::DataStream(const ByteView data) {
    buffer_.reserve(data.size());
    buffer_.insert(buffer_.end(), data.begin(), data.end());
}

DataStream::DataStream(const std::span<value_type> data) {
    buffer_.reserve(data.size());
    buffer_.insert(buffer_.end(), data.begin(), data.end());
}

void DataStream::reserve(size_type count) { buffer_.reserve(count); }

void DataStream::resize(size_type new_size, value_type item) { buffer_.resize(new_size, item); }

Error DataStream::write(ByteView data) {
    auto next_size(safe_add(buffer_.size(), data.size()));
    if (!next_size.has_value()) {
        return Error::kOverflow;
    }
    buffer_.append(data);
    return Error::kSuccess;
}

Error DataStream::write(const uint8_t* const ptr, DataStream::size_type count) { return write({ptr, count}); }

DataStream::iterator DataStream::begin() { return buffer_.begin() + static_cast<difference_type>(read_position_); }

void DataStream::rewind(std::optional<size_type> count) noexcept {
    if (!count.has_value()) {
        read_position_ = 0;
    } else if (count.value() <= read_position_) {
        read_position_ -= count.value();
    }
}

DataStream::iterator DataStream::end() { return buffer_.end(); }

void DataStream::insert(iterator where, value_type item) { buffer_.insert(std::move(where), item); }

void DataStream::erase(iterator where) { buffer_.erase(std::move(where)); }

void DataStream::push_back(uint8_t byte) { buffer_.push_back(byte); }

tl::expected<ByteView, Error> DataStream::read(size_t count) {
    auto next_read_position{safe_add(read_position_, count)};
    if (!next_read_position || *next_read_position > buffer_.length()) {
        return tl::unexpected(Error::kReadBeyondData);
    }
    ByteView ret(&buffer_[read_position_], count);
    std::swap(read_position_, *next_read_position);
    return ret;
}

Error DataStream::skip(DataStream::size_type count) noexcept {
    auto requested_position(safe_add(read_position_, count));
    if (!requested_position.has_value() || requested_position.value() > buffer_.size()) {
        return Error::kReadBeyondData;
    }
    read_position_ = requested_position.value();
    return Error::kSuccess;
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

bool DataStream::empty() const noexcept { return buffer_.empty(); }

DataStream::size_type DataStream::avail() const noexcept { return buffer_.size() - read_position_; }

void DataStream::clear() noexcept {
    buffer_.clear();
    read_position_ = 0;
}

void DataStream::get_clear(DataStream& dst) {
    dst.write({&buffer_[read_position_], avail()});
    clear();
}

SDataStream::SDataStream(Scope scope, int version) : DataStream(), scope_(scope), version_(version) {}

SDataStream::SDataStream(ByteView data, Scope scope, int version)
    : DataStream(data), scope_(scope), version_(version) {}

SDataStream::SDataStream(const std::span<value_type> data, Scope scope, int version)
    : DataStream(data), scope_(scope), version_(version) {}

SDataStream::size_type SDataStream::computed_size() const noexcept { return computed_size_; }

void SDataStream::clear() noexcept {
    DataStream::clear();
    computed_size_ = 0;
}

}  // namespace zen::serialization