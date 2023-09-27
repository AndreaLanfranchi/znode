/*
   Copyright 2009-2010 Satoshi Nakamoto
   Copyright 2009-2014 The Bitcoin Core developers
   Copyright 2023 Horizen Labs
   Distributed under the MIT software license, see the accompanying
   file COPYING or http://www.opensource.org/licenses/mit-license.php.
*/

#include "stream.hpp"

#include <limits>
#include <utility>

#include <core/encoding/hex.hpp>

namespace zenpp::ser {

DataStream::DataStream(const ByteView data) {
    buffer_.reserve(data.size());
    buffer_.insert(buffer_.end(), data.begin(), data.end());
}

DataStream::DataStream(const std::span<value_type> data) {
    buffer_.reserve(data.size());
    buffer_.insert(buffer_.end(), data.begin(), data.end());
}

outcome::result<void> DataStream::reserve(size_type count) {
    if (count > kMaxStreamSize) return Error::kInputTooLarge;
    buffer_.reserve(count);
}

outcome::result<void> DataStream::resize(size_type new_size, value_type item) {
    if (new_size > kMaxStreamSize) return Error::kInputTooLarge;
    buffer_.resize(new_size, item);
}

outcome::result<void> DataStream::write(ByteView data) {
    if (size() + data.size() > kMaxStreamSize) return Error::kInputTooLarge;
    buffer_.insert(buffer_.end(), data.begin(), data.end());
    return outcome::success();
}

outcome::result<void> DataStream::write(const uint8_t* const ptr, DataStream::size_type count) {
    return write({ptr, count});
}

DataStream::iterator DataStream::begin() { return buffer_.begin() + static_cast<difference_type>(read_position_); }

void DataStream::rewind(std::optional<size_type> count) noexcept {
    if (not count.has_value()) {
        read_position_ = 0;
    } else if (count.value() <= read_position_) {
        read_position_ -= count.value();
    }
}

DataStream::iterator DataStream::end() { return buffer_.end(); }

void DataStream::insert(iterator where, value_type item) { buffer_.insert(std::move(where), item); }

void DataStream::erase(iterator where) {
    if (where == end()) return;

    const auto pos(static_cast<size_type>(std::distance(buffer_.begin(), where)));
    buffer_.erase(std::move(where));
    if (read_position_ > 0U and read_position_ > pos) {
        --read_position_;
    }
    read_position_ = std::min(read_position_, buffer_.size());
}

void DataStream::erase(const size_type pos, std::optional<size_type> count) {
    if ((count.has_value() and *count == 0) or pos >= buffer_.size()) return;

    const auto max_count{buffer_.size() - pos};
    count = std::min(count.value_or(std::numeric_limits<size_type>::max()), max_count);
    buffer_.erase(pos, *count);
    if (read_position_ > 0U && read_position_ > pos) {
        auto move_back_count{std::min(read_position_ - pos, *count)};
        read_position_ -= move_back_count;
    }
}

void DataStream::push_back(uint8_t byte) { buffer_.push_back(byte); }

outcome::result<ByteView> DataStream::read(std::optional<size_t> count) noexcept {
    const auto bytes_being_read{count.value_or(avail())};
    if (bytes_being_read > avail()) return Error::kReadOverflow;
    ByteView ret(&buffer_[read_position_], bytes_being_read);
    read_position_ += bytes_being_read;
    return ret;
}

void DataStream::ignore(size_type count) noexcept { read_position_ += std::min<size_type>(count, avail()); }

bool DataStream::eof() const noexcept { return read_position_ >= buffer_.size(); }

DataStream::size_type DataStream::tellg() const noexcept { return read_position_; }

DataStream::size_type DataStream::seekg(size_type position) noexcept {
    read_position_ = std::min(position, buffer_.size());
    return read_position_;
}

std::string DataStream::to_string() const { return enc::hex::encode({buffer_.data(), buffer_.size()}, false); }

void DataStream::consume(std::optional<size_type> pos) noexcept {
    const size_t count{std::min(read_position_, pos.value_or(std::numeric_limits<size_type>::max()))};
    buffer_.erase(0, count);
    read_position_ -= count;
}

DataStream::size_type DataStream::size() const noexcept { return buffer_.size(); }

bool DataStream::empty() const noexcept { return buffer_.empty(); }

DataStream::size_type DataStream::avail() const noexcept { return buffer_.size() - read_position_; }

void DataStream::clear() noexcept {
    buffer_.clear();
    read_position_ = 0;
}

outcome::result<void> DataStream::get_clear(DataStream& dst) {
    if (const auto write_result{dst.write({&buffer_[read_position_], avail()})}; not write_result) {
        return write_result.error();
    }
    clear();
    return outcome::success();
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
}  // namespace zenpp::ser
