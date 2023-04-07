/*
   Copyright 2009-2010 Satoshi Nakamoto
   Copyright 2009-2014 The Bitcoin Core developers
   Copyright 2023 Horizen Labs
   Distributed under the MIT software license, see the accompanying
   file COPYING or http://www.opensource.org/licenses/mit-license.php.
*/

#pragma once
#include <functional>

#include <tl/expected.hpp>

#include <zen/core/common/base.hpp>
#include <zen/core/common/secure_bytes.hpp>
#include <zen/core/serialization/base.hpp>
#include <zen/core/serialization/serialize.hpp>

namespace zen::serialization {

class Serializable;

class Archive {
  public:
    using reference_type = typename SecureBytes::reference;
    using size_type = typename SecureBytes::size_type;
    using difference_type = typename SecureBytes::difference_type;
    using value_type = typename SecureBytes::value_type;
    using iterator_type = typename SecureBytes::iterator;

    Archive(Scope scope, int version) : scope_{scope}, version_{version} {};

    [[nodiscard]] int get_version() const { return version_; }
    [[nodiscard]] Scope get_scope() const { return scope_; }

    void set_version(int version) { version_ = version; }

    //! \brief Reserves capacity
    void reserve(size_type count);

    //! \brief Reserves capacity
    void resize(size_type new_size, value_type item = 0);

    //! \brief Appends provided data to internal buffer
    void write(ByteView data);

    //! \brief Appends provided data to internal buffer
    void write(const uint8_t* ptr, size_type count);

    //! \brief Returns an iterator to beginning of the unconsumed part of data
    iterator_type begin();

    //! \brief Returns an iterator to end of the unconsumed part of data
    iterator_type end();

    //! \brief Appends a single byte to internal buffer
    void push_back(value_type item);

    //! \brief Inserts new element at specified position
    void insert(iterator_type where, value_type item);

    //! \brief Erase an element from specified position
    void erase(iterator_type where);

    //! \brief Returns a view of requested bytes count from the actual read position
    //! \remarks After the view is returned the read position is advanced by count
    [[nodiscard]] tl::expected<ByteView, Error> read(size_t count);

    //! \brief Advances the read position by count
    //! \remarks If the count of bytes to be skipped exceeds the boundary of the buffer the read position
    //! is moved at the end and eof() will return true
    void skip(size_type count) noexcept;

    //! \brief Whether the end of stream's data has been reached
    [[nodiscard]] bool eof() const noexcept;

    //! \brief Accesses one element of the buffer
    constexpr reference_type operator[](size_type pos) { return buffer_[pos + read_position_]; }

    //! \brief Returns the size of the contained data
    [[nodiscard]] size_type size() const noexcept;

    //! \brief Returns the computed size of the to-be-contained data
    //! \remarks Only when this stream is used as a calculator
    [[nodiscard]] size_type computed_size() const noexcept;

    //! \brief Returns the size of yet-to-be-consumed data
    [[nodiscard]] size_type avail() const noexcept;

    //! \brief Clears data and moves the read position to the beginning
    //! \remarks After this operation eof() == true
    void clear() noexcept;

    //! \brief Copies unconsumed data into dest and clears
    void get_clear(Archive& dst);

    //! \brief Returns the current read position
    [[nodiscard]] size_type tellp() const noexcept;

    //! \brief Moves to read position
    void seekp(size_type p) noexcept;

    //! \brief Removes the portion of already consumed data (up to read pos)
    void shrink();

    //! \brief Returns the hexed representation of the data buffer
    [[nodiscard]] std::string to_string() const;

    // Serialization for arithmetic types
    template <class T>
        requires std::is_arithmetic_v<T>
    [[nodiscard]] Error bind(T& object, Action action) {
        Error result{Error::kSuccess};
        switch (action) {
            using enum Action;
            case kComputeSize:
                computed_size_ += ser_sizeof(object);
                break;
            case kSerialize:
                write_data(*this, object);
                break;
            case kDeserialize:
                result = read_data(*this, object);
                shrink();
                break;
        }
        return result;
    }

    template <class T>
        requires std::is_same_v<T, intx::uint256>
    [[nodiscard]] Error bind(T& object, Action action) {
        ZEN_ASSERT(sizeof(object) == 32);
        Error result{Error::kSuccess};
        switch (action) {
            using enum Action;
            case kComputeSize:
                computed_size_ += sizeof(object);
                break;
            case kSerialize:
                for (size_t i{0}; i < 4; ++i) {
                    write_data(*this, object[i]);
                }
                break;
            case kDeserialize:
                for (size_t i{0}; i < 4; ++i) {
                    result = read_data(*this, object[i]);
                    if (result != Error::kSuccess) break;
                }
                break;
        }
        return result;
    }

    // Serialization for Serializable classes
    template <class T>
        requires std::derived_from<T, Serializable>
    [[nodiscard]] Error bind(T& object, Action action) {
        return object.serialization(*this, action);
    }

    // Serialization for bytes array (fixed size)
    template <class T, std::size_t N>
        requires std::is_fundamental_v<T>
    [[nodiscard]] Error bind(std::array<T, N>& object, Action action) {
        Error result{Error::kSuccess};
        const auto element_size{ser_sizeof(object[0])};
        const auto array_bytes_size{N * element_size};
        switch (action) {
            using enum Action;
            case kComputeSize:
                computed_size_ += array_bytes_size;
                break;
            case kSerialize:
                write(static_cast<uint8_t*>(object.data()), array_bytes_size);
                break;
            case kDeserialize:
                auto read_result{read(array_bytes_size)};
                if (!read_result) {
                    result = read_result.error();
                } else {
                    std::memcpy(&object[0], read_result->data(), array_bytes_size);
                }
                break;
        }
        return result;
    }

    // Serialization for basic_string<bytes>
    // Note ! In Bitcoin's objects variable sizes strings of bytes are
    // a special case as they're always the last element of a structure.
    // Due to this the size of the member is not recorded
    template <class T>
        requires std::is_same_v<T, Bytes>
    [[nodiscard]] Error bind(T& object, Action action) {
        Error result{Error::kSuccess};
        switch (action) {
            using enum Action;
            case kComputeSize:
                computed_size_ += object.size();
                break;
            case kSerialize:
                write(object);
                break;
            case kDeserialize:
                auto data_length{this->avail()};
                auto data{read(data_length)};
                ZEN_ASSERT(data);  // Should not return an error
                object.assign(*data);
                shrink();
                break;
        }
        return result;
    }

  private:
    Scope scope_{0};
    int version_{0};
    SecureBytes buffer_{};        // Data buffer
    size_type computed_size_{0};  // Total accrued size (for size computing)
    size_type read_position_{0};  // Current read position;
};
}  // namespace zen::serialization
