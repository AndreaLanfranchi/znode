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

#include <core/common/base.hpp>
#include <core/crypto/hash256.hpp>
#include <core/serialization/serializable.hpp>
#include <core/types/hash.hpp>

#include <infra/network/errors.hpp>
#include <infra/network/messages.hpp>
#include <infra/network/payloads.hpp>

namespace zenpp::net {

class MessageHeader : public ser::Serializable {
  public:
    MessageHeader() : Serializable(){};

    std::array<uint8_t, kMessageHeaderMagicLength> network_magic{};  // Message magic (origin network)
    std::array<uint8_t, kMessageHeaderCommandLength>
        command{};               // ASCII string identifying the packet content, NULL padded
    uint32_t payload_length{0};  // Length of payload in bytes
    std::array<uint8_t, kMessageHeaderChecksumLength>
        payload_checksum{};  // First 4 bytes of sha256(sha256(payload)) in internal byte order

    //! \brief Returns the message definition
    [[nodiscard]] const MessageDefinition& get_definition() const noexcept;

    //! \brief Returns the decoded message type
    [[nodiscard]] MessageType get_type() const noexcept { return get_definition().message_type; }

    //! \brief Sets the message type and fills the command field
    //! \remarks On non pristine headers, this function has no effect
    void set_type(MessageType type) noexcept;

    //! \brief Reset the header to its factory state
    void reset() noexcept;

    //! \brief Check if the header is in its factory state
    [[nodiscard]] bool pristine() const noexcept;

    //! \brief Performs a sanity check on the header
    [[nodiscard]] outcome::result<void> validate(int protocol_version, ByteView magic) noexcept;

  private:
    MessageType message_type_{MessageType::kMissingOrUnknown};
    friend class ser::SDataStream;
    outcome::result<void> serialization(ser::SDataStream& stream, ser::Action action) override;
};

enum class MessagePriority {
    kHigh = 0,
    kNormal = 1,
    kLow = 2,
};

class Message {
  public:
    //! \brief Construct a blank NetMessage
    Message() : ser_stream_{ser::Scope::kNetwork, 0} {};

    //! \brief Construct a blank NetMessage with a specific version
    explicit Message(int version) : ser_stream_{ser::Scope::kNetwork, version} {};

    //! \brief Construct a NetMessage with network magic provided
    explicit Message(int version, const std::array<uint8_t, 4>& magic)
        : ser_stream_{ser::Scope::kNetwork, version}, network_magic_{magic} {};

    //! \brief Construct a NetMessage with network magic provided
    explicit Message(int version, std::array<uint8_t, 4>&& magic)
        : ser_stream_{ser::Scope::kNetwork, version}, network_magic_{magic} {};

    // Not movable nor copyable
    Message(const Message&) = delete;
    Message(const Message&&) = delete;
    Message& operator=(const Message&) = delete;
    ~Message() = default;

    //! \brief Gets the overall size of the message as serialized bytes count
    [[nodiscard]] size_t size() const noexcept { return ser_stream_.size(); }

    [[nodiscard]] bool is_complete() const noexcept { return header_validated_ and payload_validated_; }

    //! \brief Returns the message type (i.e. command)
    [[nodiscard]] MessageType get_type() const noexcept;

    //! \brief Returns the message header
    [[nodiscard]] const MessageHeader& header() const noexcept { return header_; }

    [[nodiscard]] MessageHeader& header() noexcept { return header_; }

    [[nodiscard]] ser::SDataStream& data() noexcept { return ser_stream_; }

    //! \brief Sets the message version (generally inherited from the protocol version)
    void set_version(int version) noexcept { ser_stream_.set_version(version); }

    //! \brief Returns the message version
    [[nodiscard]] int get_version() const noexcept { return ser_stream_.get_version(); }

    //! \brief Resets the message to its factory state
    void reset() noexcept;

    //! \brief Validates the message header, payload and checksum
    [[nodiscard]] outcome::result<void> validate() noexcept;

    //! \brief Writes data into message buffer and tries to deserialize and validate
    //! \remarks Input data is consumed until the message is fully validated or an error occurs
    //! \remarks Any error returned `Error::kMessageHeaderIncomplete` or `Error::kMessageBodyIncomplete`
    //!          must be considered fatal
    [[nodiscard]] outcome::result<void> write(ByteView& input);

    //! \brief Populates the message header and payload
    outcome::result<void> push(MessagePayload& payload) noexcept;

  private:
    MessageHeader header_{};                                             // Where the message header is deserialized
    ser::SDataStream ser_stream_;                                        // Contains all the message raw data
    std::array<uint8_t, kMessageHeaderMagicLength> network_magic_{0x0};  // Message magic (network)
    bool header_validated_{false};   // Whether the header has been validated already
    bool payload_validated_{false};  // Whether the payload has been validated already

    //! \brief Validates the message header
    [[nodiscard]] outcome::result<void> validate_header() noexcept;

    //! \brief Validates the message payload
    [[nodiscard]] outcome::result<void> validate_payload() noexcept;

    //! \brief Validates the payload in case of vectorized contents
    [[nodiscard]] outcome::result<void> validate_payload_vector(const MessageDefinition& message_definition) noexcept;

    //! \brief Validates the message header's checksum against the payload
    [[nodiscard]] outcome::result<void> validate_payload_checksum() noexcept;
};
}  // namespace zenpp::net
