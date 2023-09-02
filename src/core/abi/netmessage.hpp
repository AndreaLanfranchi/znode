/*
   Copyright 2009-2010 Satoshi Nakamoto
   Copyright 2009-2013 The Bitcoin Core developers
   Copyright 2023 Horizen Labs
   Distributed under the MIT software license, see the accompanying
   file COPYING or http://www.opensource.org/licenses/mit-license.php.
*/

#pragma once

#include <memory>
#include <optional>

#include <core/abi/messages.hpp>
#include <core/common/base.hpp>
#include <core/crypto/hash256.hpp>
#include <core/serialization/serializable.hpp>
#include <core/types/hash.hpp>

namespace zenpp::abi {

class NetMessageHeader : public serialization::Serializable {
  public:
    NetMessageHeader() : Serializable(){};

    std::array<uint8_t, 4> network_magic{};     // Message magic (origin network)
    std::array<uint8_t, 12> command{};          // ASCII string identifying the packet content, NULL padded
    uint32_t payload_length{0};                 // Length of payload in bytes
    std::array<uint8_t, 4> payload_checksum{};  // First 4 bytes of sha256(sha256(payload)) in internal byte order

    //! \brief Returns the message definition
    [[nodiscard]] const MessageDefinition& get_definition() const noexcept;

    //! \brief Returns the decoded message type
    [[nodiscard]] NetMessageType get_type() const noexcept { return get_definition().message_type; }

    //! \brief Sets the message type and fills the command field
    //! \remarks On non pristine headers, this function has no effect
    void set_type(NetMessageType type) noexcept;

    //! \brief Reset the header to its factory state
    void reset() noexcept;

    //! \brief Check if the header is in its factory state
    [[nodiscard]] bool pristine() const noexcept;

    //! \brief Performs a sanity check on the header
    [[nodiscard]] serialization::Error validate() noexcept;

  private:
    NetMessageType message_type_{NetMessageType::kMissingOrUnknown};
    friend class serialization::SDataStream;
    serialization::Error serialization(serialization::SDataStream& stream, serialization::Action action) override;
};

class NetMessage {
  public:
    //! \brief Construct a blank NetMessage
    NetMessage() : ser_stream_{serialization::Scope::kNetwork, 0} {};

    //! \brief Construct a blank NetMessage with a specific version
    explicit NetMessage(int version) : ser_stream_{serialization::Scope::kNetwork, version} {};

    //! \brief Construct a NetMessage with network magic provided
    explicit NetMessage(int version, std::array<uint8_t, 4>& magic)
        : ser_stream_{serialization::Scope::kNetwork, version} {
        header_.network_magic = magic;
    };

    // Not movable nor copyable
    NetMessage(const NetMessage&) = delete;
    NetMessage(const NetMessage&&) = delete;
    NetMessage& operator=(const NetMessage&) = delete;
    ~NetMessage() = default;

    [[nodiscard]] size_t size() const noexcept { return ser_stream_.size(); }
    [[nodiscard]] NetMessageType get_type() const noexcept { return header_.get_type(); }
    [[nodiscard]] NetMessageHeader& header() noexcept { return header_; }
    [[nodiscard]] serialization::SDataStream& data() noexcept { return ser_stream_; }
    [[nodiscard]] serialization::Error parse(ByteView& input_data, ByteView network_magic = {}) noexcept;

    //! \brief Sets the message version (generally inherited from the protocol version)
    void set_version(int version) noexcept;

    //! \brief Returns the message version
    [[nodiscard]] int get_version() const noexcept;

    //! \brief Validates the message header, payload and checksum
    [[nodiscard]] serialization::Error validate() noexcept;

    //! \brief Populates the message header and payload
    serialization::Error push(NetMessageType message_type, NetMessagePayload& payload, ByteView magic) noexcept;

  private:
    NetMessageHeader header_{};              // Where the message header is deserialized
    serialization::SDataStream ser_stream_;  // Contains all the message raw data

    [[nodiscard]] serialization::Error validate_checksum() noexcept;
};
}  // namespace zenpp::abi
