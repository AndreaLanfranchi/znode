/*
   Copyright 2009-2010 Satoshi Nakamoto
   Copyright 2009-2014 The Bitcoin Core developers
   Copyright 2023 Horizen Labs
   Distributed under the MIT software license, see the accompanying
   file COPYING or http://www.opensource.org/licenses/mit-license.php.
*/

#pragma once
#include <array>
#include <functional>
#include <optional>
#include <span>

#include <boost/asio/ip/address.hpp>

#include <core/common/base.hpp>
#include <core/common/cast.hpp>
#include <core/common/secure_bytes.hpp>
#include <core/serialization/base.hpp>
#include <core/serialization/errors.hpp>
#include <core/serialization/serialize.hpp>

namespace zenpp::ser {

class Serializable;

class DataStream {
  public:
    using size_type = typename SecureBytes::size_type;
    using difference_type = typename SecureBytes::difference_type;
    using reference = typename SecureBytes::reference;
    using const_reference = typename SecureBytes::const_reference;
    using value_type = typename SecureBytes::value_type;
    using iterator = typename SecureBytes::iterator;
    using const_iterator = typename SecureBytes::const_iterator;
    using reverse_iterator = typename SecureBytes::reverse_iterator;

    explicit DataStream() = default;
    explicit DataStream(ByteView data);
    explicit DataStream(std::span<value_type> data);
    DataStream(DataStream& other) noexcept = default;
    DataStream(DataStream&& other) noexcept = default;
    virtual ~DataStream() = default;

    //! \brief Reserves capacity
    outcome::result<void> reserve(size_type count);

    //! \brief Adjusts the size of the underlying buffer
    outcome::result<void> resize(size_type new_size, value_type item = value_type{});

    //! \brief Appends provided data to internal buffer
    outcome::result<void> write(ByteView data);

    //! \brief Appends provided data to internal buffer
    outcome::result<void> write(const uint8_t* ptr, size_type count);

    //! \brief Returns an iterator to beginning of the unconsumed part of data
    iterator begin();

    //! \brief Rewinds the read position by count
    //! \remarks If count is not provided the read position is moved at the beginning of the buffer
    void rewind(std::optional<size_type> count = std::nullopt) noexcept;

    //! \brief Returns an iterator to end of the unconsumed part of data
    iterator end();

    //! \brief Appends a single byte to internal buffer
    void push_back(value_type item);

    //! \brief Inserts new element at specified position
    void insert(iterator where, value_type item);

    //! \brief Erase a data element from specified position
    //! \note Read position is adjusted accordingly
    void erase(iterator where);

    //! \brief Erase a data range from specified position
    //! \param pos Position of the first byte to be erased
    //! \param count Number of bytes to be erased (default: all bytes from pos to the end of the buffer)
    //! \note Read position is adjusted accordingly
    void erase(size_type pos, std::optional<size_type> count = std::nullopt);

    //! \brief Returns a view of requested bytes count from the actual read position
    //! \remarks After the view is returned the read position is advanced by count
    //! \remarks If count is omitted the whole unconsumed part of data is returned
    [[nodiscard]] outcome::result<ByteView> read(std::optional<size_t> count = std::nullopt) noexcept;

    //! \brief Advances the read position by count without returning any data
    //! \remarks If the count of bytes to be skipped exceeds the boundary of the buffer the read position
    //! is moved at the end and eof() will return true
    void ignore(size_type count = 1) noexcept;

    //! \brief Whether the end of stream's data has been reached
    [[nodiscard]] bool eof() const noexcept;

    //! \brief Accesses one element of the buffer
    constexpr reference operator[](size_type pos) { return buffer_[pos]; }

    //! \brief Returns the size of the contained data
    [[nodiscard]] size_type size() const noexcept;

    //! \brief Whether this archive contains any data
    [[nodiscard]] bool empty() const noexcept;

    //! \brief Returns the size of yet-to-be-consumed data
    [[nodiscard]] size_type avail() const noexcept;

    //! \brief Clears data and moves the read position to the beginning
    //! \remarks After this operation eof() == true
    virtual void clear() noexcept;

    //! \brief Copies unconsumed data into dest and clears
    outcome::result<void> get_clear(DataStream& dst);

    //! \brief Returns the current read position
    [[nodiscard]] size_type tellg() const noexcept;

    //! \brief Moves to the desidered read position
    //! \remarks If the provided position exceeds the boundary of the buffer the read position then
    //! the read cursor is moved at the end and eof() will return true
    size_type seekg(size_type position) noexcept;

    //! \brief Removes the portion of data from the beginning up to the provided position or the read position
    //! (whichever is smaller) \remarks If no position is provided the whole buffer is cleared up to current read
    //! position
    void consume(std::optional<size_type> pos = std::nullopt) noexcept;

    //! \brief Returns the hexed representation of the data buffer
    [[nodiscard]] std::string to_string() const;

  private:
    SecureBytes buffer_{};        // Data buffer
    size_type read_position_{0};  // Current read position;
};

//! \brief Stream for serialization / deserialization of Bitcoin objects
class SDataStream : public DataStream {
  public:
    SDataStream() = default;
    SDataStream(Scope scope, int version);
    SDataStream(ByteView data, Scope scope, int version);
    SDataStream(std::span<value_type> data, Scope scope, int version);

    [[nodiscard]] Scope get_scope() const noexcept { return scope_; }
    [[nodiscard]] int get_version() const noexcept { return version_; }
    void set_version(int version) noexcept { version_ = version; }

    //! \brief Returns the computed size of the to-be-contained data
    //! \remarks Only when this stream is used as a calculator
    [[nodiscard]] size_type computed_size() const noexcept;

    //! \brief Clears data and moves the read position to the beginning
    //! \remarks After this operation eof() == true
    void clear() noexcept override;

    // Serialization for bool
    [[nodiscard]] outcome::result<void> bind(bool& object, Action action) {
        switch (action) {
            using enum Action;
            case kComputeSize:
                ++computed_size_;
                break;
            case kSerialize:
                return write_data(*this, object);
            case kDeserialize:
                return read_data(*this, object);
        }
        return outcome::success();
    }

    // Serialization for arithmetic types
    template <Integral T>
    [[nodiscard]] outcome::result<void> bind(T& object, Action action) {
        switch (action) {
            using enum Action;
            case kComputeSize:
                computed_size_ += ssizeof<T>;
                break;
            case kSerialize:
                return write_data(*this, object);
            case kDeserialize:
                return read_data(*this, object);
                break;
        }
        return outcome::success();
    }

    template <BigUnsignedIntegral T>
    [[nodiscard]] outcome::result<void> bind(T& object, Action action) {
        /*
         * When used at fixed precision, the size of type boost cpp_int is always one machine word larger
         * than you would expect for an N-bit integer
         * https://www.boost.org/doc/libs/1_81_0/libs/multiprecision/doc/html/boost_multiprecision/tut/ints/cpp_int.html
         * */
        std::array<uint8_t, ssizeof<T>> bytes{0x0};
        switch (action) {
            using enum Action;
            case kComputeSize:
                computed_size_ += bytes.size();
                break;
            case kSerialize:
                boost::multiprecision::export_bits(object, bytes.begin(), CHAR_BIT);
                return write(bytes);
            case kDeserialize:
                if (const auto read_result{bind(bytes, action)}; read_result.has_error()) [[unlikely]] {
                    return read_result.error();
                }
                boost::multiprecision::import_bits(object, bytes.begin(), bytes.end(), CHAR_BIT);
                break;
        }
        return outcome::success();
    }

    // Serialization for Serializable classes
    template <class T>
    requires std::derived_from<T, Serializable>
    [[nodiscard]] outcome::result<void> bind(T& object, Action action) { return object.serialization(*this, action); }

    // Serialization for bytes array (fixed size)
    template <class T, std::size_t N>
    requires std::is_fundamental_v<T>
    [[nodiscard]] outcome::result<void> bind(std::array<T, N>& object, Action action) {
        const auto element_size{ssizeof<T>};
        const auto array_bytes_size{N * element_size};
        switch (action) {
            using enum Action;
            case kComputeSize:
                computed_size_ += array_bytes_size;
                break;
            case kSerialize:
                return write(static_cast<uint8_t*>(object.data()), array_bytes_size);
            case kDeserialize:
                if (auto read_result{read(array_bytes_size)}; read_result.has_error()) [[unlikely]] {
                    return read_result.error();
                } else {
                    std::memcpy(&object[0], read_result.value().data(), array_bytes_size);
                }
                break;
        }
        return outcome::success();
    }

    // Serialization for basic_string<bytes>
    // Note ! In Bitcoin's objects variable sizes strings of bytes are
    // a special case as they're always the last element of a structure.
    // Due to this the size of the member is not recorded
    template <class T>
    requires std::is_same_v<T, Bytes>
    [[nodiscard]] outcome::result<void> bind(T& object, Action action) {
        switch (action) {
            using enum Action;
            case kComputeSize:
                computed_size_ += object.size();
                break;
            case kSerialize:
                return write(object);
            case kDeserialize:
                if (auto read_result{read()}; read_result.has_error()) [[unlikely]] {
                    return read_result.error();
                } else {
                    object.assign(read_result.value());
                }
                break;
        }
        return outcome::success();
    }

    // Serialization for std::string
    template <class T>
    requires std::is_same_v<T, std::string>
    [[nodiscard]] outcome::result<void> bind(T& object, Action action) {
        switch (action) {
            using enum Action;
            case kComputeSize:
                computed_size_ += ser_compact_sizeof(object.size()) + object.size();
                break;
            case kSerialize:
                if (const auto result{write_compact(*this, static_cast<uint64_t>(object.size()))}; result.has_error())
                    [[unlikely]] {
                    return result.error();
                }
                return write(string_view_to_byte_view(object));
            case kDeserialize:
                if (const auto data_length{read_compact(*this)}; data_length.has_error()) [[unlikely]] {
                    return data_length.error();
                } else {
                    auto data{read(data_length.value())};
                    if (data.has_error()) return data.error();
                    object.assign(data.value().begin(), data.value().end());
                }
                break;
        }
        return outcome::success();
    }

    // Serialization for ip::address
    // see https://en.wikipedia.org/wiki/IPv6#IPv4-mapped_IPv6_addresses
    template <class T>
    requires std::is_same_v<T, boost::asio::ip::address>
    [[nodiscard]] outcome::result<void> bind(T& object, Action action) {
        std::array<uint8_t, 16> bytes{0x0};
        switch (action) {
            using enum Action;
            case kComputeSize:
                computed_size_ += bytes.size();
                break;
            case kSerialize:
                // Always transform in corresponding v6 representation
                if (object.is_v4()) {
                    auto object_v6{boost::asio::ip::address_v6::v4_mapped(object.to_v4())};
                    bytes = object_v6.to_bytes();
                } else {
                    bytes = object.to_v6().to_bytes();
                }
                return bind(bytes, action);
            case kDeserialize:
                if (auto read_result{bind(bytes, action)}; read_result.has_error()) [[unlikely]] {
                    return read_result.error();
                } else {
                    if (reinterpret_cast<uint64_t*>(bytes.data())[0] == 0U) {
                        object = boost::asio::ip::make_address_v4(
                            std::array<uint8_t, 4>{bytes[12], bytes[13], bytes[14], bytes[15]});
                    } else {
                        object = boost::asio::ip::make_address_v6(bytes);
                    }
                }
                break;
        }
        return outcome::success();
    }

  private:
    Scope scope_;
    int version_{0};
    size_type computed_size_{0};  // Total accrued size (for size computing)
};

}  // namespace zenpp::ser
