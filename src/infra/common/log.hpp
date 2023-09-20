/*
   Copyright 2022 The Silkworm Authors
   Copyright 2023 Horizen Labs
   Distributed under the MIT software license, see the accompanying
   file COPYING or http://www.opensource.org/licenses/mit-license.php.
*/

#pragma once
#include <filesystem>
#include <sstream>
#include <vector>

#include <infra/os/terminal.hpp>

namespace zenpp::log {

//! \brief Available severity levels
enum class Level {
    kNone,      // Simple logging line with no severity (e.g. build info)
    kCritical,  // An error there's no way we can recover from
    kError,     // We encountered an error we might be able to recover from
    kWarning,   // Something happened and user might have the possibility to amend the situation
    kInfo,      // Info messages on regular operations
    kDebug,     // Debug information
    kTrace      // Trace calls to functions
};

//! \brief Holds logging configuration
struct Settings {
    bool log_std_out{false};            // Whether console logging goes to std::cout or std::cerr (default)
    std::string log_timezone{"UTC"};    // UTC or a valid IANA time zone (e.g. Europe/Rome) WIP
    bool log_nocolor{false};            // Whether to disable colorized output
    bool log_threads{false};            // Whether to print thread ids in log lines
    Level log_verbosity{Level::kInfo};  // Log verbosity level
    std::string log_file;               // Log to file
    char log_thousands_sep{'\''};       // Thousands separator
};

//! \brief Initializes logging facilities
//! \note This function is not thread safe as it's meant to be used at start of process and never called again
void init(const Settings& settings);

//! \brief Returns the current logging settings
Settings& get_settings() noexcept;

//! \brief Get the current logging verbosity
//! \note This function is not thread safe as it's meant to be used in tests
Level get_verbosity() noexcept;

//! \brief Sets logging verbosity
//! \note This function is not thread safe as it's meant to be used at start of process and never called again
void set_verbosity(Level level);

//! \brief Sets the name for this thread when logging traces also threads
void set_thread_name(const char* name);

//! \brief Sets the name for this thread when logging traces also threads
void set_thread_name(const std::string_view& name);

//! \brief Returns the id of current thread in a printable form
uint64_t get_thread_id();

//! \brief Returns the currently set name for the thread or the thread id
std::string get_thread_name();

//! \brief Checks if provided log level will be effectively printed on behalf of current settings
//! \return True / False
//! \remarks Some logging operations may implement computations which would be completely wasted if the outcome is not
//! printed
bool test_verbosity(Level level);

//! \brief Sets a file output for log teeing
//! \note This function is not thread safe as it's meant to be used at start of process and never called again
void tee_file(const std::filesystem::path& path);

class BufferBase {
  public:
    explicit BufferBase(Level level);
    explicit BufferBase(Level level, std::string_view msg, const std::vector<std::string>& args);
    ~BufferBase() { flush(); }

    // Accumulators
    template <class T>
    inline void append(T const& obj) {
        if (should_print_) sstream_ << obj;
    }
    template <class T>
    BufferBase& operator<<(T const& obj) {
        append(obj);
        return *this;
    }

  protected:
    void flush() const;
    const bool should_print_;
    std::stringstream sstream_;
};

template <Level level>
class LogBuffer : public BufferBase {
  public:
    explicit LogBuffer() : BufferBase(level) {}
    explicit LogBuffer(std::string_view msg, std::vector<std::string> args = {}) : BufferBase(level, msg, args) {}
};

using Trace = LogBuffer<Level::kTrace>;
using Debug = LogBuffer<Level::kDebug>;
using Info = LogBuffer<Level::kInfo>;
using Warning = LogBuffer<Level::kWarning>;
using Error = LogBuffer<Level::kError>;
using Critical = LogBuffer<Level::kCritical>;
using Message = LogBuffer<Level::kNone>;

}  // namespace zenpp::log

#define LOG_BUFFER(level_)                     \
    if (!zenpp::log::test_verbosity(level_)) { \
    } else                                     \
        zenpp::log::LogBuffer<level_>()

#define LOGF_BUFFER(level_)                    \
    if (!zenpp::log::test_verbosity(level_)) { \
    } else                                     \
        zenpp::log::LogBuffer<level_>() << __func__ << " (" << __LINE__ << ") "

#define LOG_TRACE LOG_BUFFER(zenpp::log::Level::kTrace)
#define LOG_DEBUG LOG_BUFFER(zenpp::log::Level::kDebug)
#define LOG_INFO LOG_BUFFER(zenpp::log::Level::kInfo)
#define LOG_WARNING LOG_BUFFER(zenpp::log::Level::kWarning)
#define LOG_ERROR LOG_BUFFER(zenpp::log::Level::kError)
#define LOG_CRITICAL LOG_BUFFER(zenpp::log::Level::kCritical)
#define LOG_MESSAGE LOG_BUFFER(zenpp::log::Level::kNone)

#define LOGF_TRACE LOGF_BUFFER(zenpp::log::Level::kTrace)
#define LOGF_DEBUG LOGF_BUFFER(zenpp::log::Level::kDebug)
#define LOGF_INFO LOGF_BUFFER(zenpp::log::Level::kInfo)
#define LOGF_WARNING LOGF_BUFFER(zenpp::log::Level::kWarning)
#define LOGF_ERROR LOGF_BUFFER(zenpp::log::Level::kError)
#define LOGF_CRITICAL LOGF_BUFFER(zenpp::log::Level::kCritical)
#define LOGF_MESSAGE LOGF_BUFFER(zenpp::log::Level::kNone)
