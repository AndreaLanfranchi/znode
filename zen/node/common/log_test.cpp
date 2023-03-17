/*
   Copyright 2022 The Silkworm Authors
   Copyright 2023 Horizen Labs
   Distributed under the MIT software license, see the accompanying
   file COPYING or http://www.opensource.org/licenses/mit-license.php.
*/

#include "log_test.hpp"

#include <iostream>
#include <thread>

#include <catch2/catch.hpp>

namespace zen::log {
// Custom LogBuffer just for testing to access buffered content
template <Level level>
class TestLogBuffer : public LogBuffer<level> {
  public:
    [[nodiscard]] std::string content() const { return LogBuffer<level>::ss_.str(); }
};

// Utility test function enforcing that log buffered content is empty (or not) as expected
template <Level level>
void check_log_empty(bool expected) {
    auto log_buffer = TestLogBuffer<level>();
    log_buffer << "test";
    if (expected) {
        CHECK(log_buffer.content().empty());
    } else {
        CHECK(log_buffer.content().find("test") != std::string::npos);
    }
}

// Utility class using RAII to swap the underlying buffers of the provided streams
class StreamSwap {
  public:
    StreamSwap(std::ostream& o1, const std::ostream& o2) : buffer_(o1.rdbuf()), stream_(o1) { o1.rdbuf(o2.rdbuf()); }
    ~StreamSwap() { stream_.rdbuf(buffer_); }

  private:
    std::streambuf* buffer_;
    std::ostream& stream_;
};

TEST_CASE("LogBuffer", "[common][log]") {
    // Temporarily override std::cout and std::cerr with null stream to avoid terminal output
    StreamSwap cout_swap{std::cout, null_stream()};
    StreamSwap cerr_swap{std::cerr, null_stream()};

    using enum zen::log::Level;
    SECTION("LogBuffer stores nothing for verbosity higher than default") {
        check_log_empty<kDebug>(true);
        check_log_empty<kTrace>(true);
    }

    SECTION("LogBuffer stores content for verbosity lower than or equal to default") {
        check_log_empty<kInfo>(false);
        check_log_empty<kWarning>(false);
        check_log_empty<kError>(false);
        check_log_empty<kCritical>(false);
        check_log_empty<kNone>(false);
    }

    SECTION("LogBuffer stores nothing for verbosity higher than configured one") {
        SetLogVerbosityGuard guard{kWarning};
        check_log_empty<kInfo>(true);
        check_log_empty<kDebug>(true);
        check_log_empty<kTrace>(true);
    }

    SECTION("LogBuffer stores content for verbosity lower than or equal to configured one") {
        SetLogVerbosityGuard guard{kWarning};
        check_log_empty<kWarning>(false);
        check_log_empty<kError>(false);
        check_log_empty<kCritical>(false);
        check_log_empty<kNone>(false);
    }

    SECTION("Settings enable/disable thread tracing") {
        // Default thread tracing
        std::stringstream thread_id_stream;
        thread_id_stream << std::this_thread::get_id();
        auto log_buffer1 = TestLogBuffer<kInfo>();
        log_buffer1 << "test";
        CHECK(log_buffer1.content().find(thread_id_stream.str()) == std::string::npos);

        // Enable thread tracing
        Settings log_settings;
        log_settings.log_threads = true;
        init(log_settings);
        auto log_buffer2 = TestLogBuffer<kInfo>();
        log_buffer2 << "test";
        CHECK(log_buffer2.content().find(thread_id_stream.str()) != std::string::npos);

        // Disable thread tracing
        log_settings.log_threads = false;
        init(log_settings);
        auto log_buffer3 = TestLogBuffer<kInfo>();
        log_buffer3 << "test";
        CHECK(log_buffer3.content().find(thread_id_stream.str()) == std::string::npos);
    }
}
}  // namespace zen::log
