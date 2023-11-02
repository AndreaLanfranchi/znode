/*
   Copyright 2022 The Silkworm Authors
   Copyright 2023 The Znode Authors
   Distributed under the MIT software license, see the accompanying
   file COPYING or http://www.opensource.org/licenses/mit-license.php.
*/

#include "log.hpp"

#include <fstream>
#include <iostream>
#include <mutex>
#include <ostream>
#include <regex>
#include <thread>

#include <absl/time/clock.h>
#include <boost/algorithm/string/predicate.hpp>

namespace znode::log {

namespace {
    Settings settings_{};
    std::mutex out_mtx{};
    std::unique_ptr<std::fstream> file_{nullptr};

    std::pair<const char*, const char*> get_level_settings(Level level) {
        switch (level) {
            using enum Level;
            case kTrace:
                return {"TRACE", kColorCoal};
            case kTrace1:
                return {" TRC1", kColorGray};
            case kTrace2:
                return {" TRC2", kColorGray};
            case kTrace3:
                return {" TRC3", kColorGray};
            case kDebug:
                return {"DEBUG", kBackgroundPurple};
            case kInfo:
                return {" INFO", kColorGreen};
            case kWarning:
                return {" WARN", kColorOrangeHigh};
            case kError:
                return {"ERROR", kColorRed};
            case kCritical:
                return {" CRIT", kBackgroundRed};
            default:
                return {"     ", kColorReset};
        }
    }

    absl::TimeZone get_time_zone() {
        if (settings_.log_timezone.empty() or boost::iequals(settings_.log_timezone, "UTC")) {
            return absl::UTCTimeZone();
        }
        absl::TimeZone ret;
        if (not absl::LoadTimeZone(settings_.log_timezone, &ret)) {
            std::cerr << "Could not load time zone [" << settings_.log_timezone << "] defaulting to UTC" << std::endl;
            ret = absl::UTCTimeZone();
        }
        return ret;
    }

}  // namespace

thread_local std::string thread_name_{};

void init(const Settings& settings) {
    settings_ = settings;
    if (not settings_.log_file.empty()) {
        tee_file(std::filesystem::path(settings.log_file));
    }
    init_terminal();
}

Settings& get_settings() noexcept { return settings_; }

void tee_file(const std::filesystem::path& path) {
    file_ = std::make_unique<std::fstream>(path.string(), std::ios::out bitor std::ios::app);
    if (not file_->is_open()) {
        file_.reset();
        std::cerr << "Could not open log file " << path.string() << std::endl;
    }
}

Level get_verbosity() noexcept { return settings_.log_verbosity; }

void set_verbosity(Level level) { settings_.log_verbosity = level; }

bool test_verbosity(Level level) { return level <= settings_.log_verbosity; }

void set_thread_name(const char* name) { thread_name_ = std::string(name); }

void set_thread_name(const std::string_view& name) { thread_name_ = std::string(name); }

uint64_t get_thread_id() {
    std::stringstream sstream;
    sstream << std::this_thread::get_id();
    return std::stoull(sstream.str());
}

std::string get_thread_name() {
    if (thread_name_.empty()) {
        thread_name_.assign(std::to_string(get_thread_id()));
    }
    return thread_name_;
}

struct separate_thousands : std::numpunct<char> {
    char separator;
    explicit separate_thousands(char sep) : separator(sep) {}
    [[nodiscard]] char do_thousands_sep() const override { return separator; }
    [[nodiscard]] string_type do_grouping() const override { return "\3"; }  // groups of 3 digit
};

BufferBase::BufferBase(Level level) : should_print_(level <= settings_.log_verbosity) {
    if (not should_print_) return;

    if (settings_.log_thousands_sep not_eq 0) {
        sstream_.imbue(std::locale(sstream_.getloc(), new separate_thousands(settings_.log_thousands_sep)));
    }

    const auto [prefix, color] = get_level_settings(level);

    // Prefix
    sstream_ << kColorReset << " " << color << prefix << kColorReset << " ";

    // TimeStamp
    thread_local const absl::TimeZone time_zone{get_time_zone()};
    const absl::Time now{absl::Now()};
    sstream_ << kColorCyan << absl::FormatTime("[%m-%d|%H:%M:%E3S] ", now, time_zone) << kColorReset;

    // ThreadId
    if (settings_.log_threads) {
        sstream_ << "[" << get_thread_name() << "] ";
    }
}

BufferBase::BufferBase(Level level, std::string_view msg, const std::vector<std::string>& args) : BufferBase(level) {
    if (not should_print_) return;
    sstream_ << std::left << std::setw(25) << std::setfill(' ') << msg;
    bool left{true};
    for (const auto& arg : args) {
        sstream_ << (left ? kColorGreen : kColorWhiteHigh) << arg << kColorReset << (left ? "=" : " ") << kColorReset;
        left = !left;
    }
}

void BufferBase::flush() const {
    if (!should_print_) return;

    // Pattern to identify colorization
    static const std::regex color_pattern(R"(\x1b[[0-9;]{1,}m)");

    bool colorized{true};
    std::string line{sstream_.str()};
    if (settings_.log_nocolor) {
        line = std::regex_replace(line, color_pattern, "");
        colorized = false;
    }
    const std::unique_lock out_lck{out_mtx};
    auto& out = settings_.log_std_out ? std::cout : std::cerr;
    out << line << std::endl;
    if (file_ and file_->is_open()) {
        *file_ << (colorized ? std::regex_replace(line, color_pattern, "") : line) << std::endl;
    }
}
}  // namespace znode::log
