#include "event_logger.hpp"

#include <array>
#include <cstdio>
#include <ctime>
#include <iomanip>
#include <sstream>
#include <type_traits>

namespace hid_mouse {

EventLogger::EventLogger() {
    QueryPerformanceCounter(&qpc_start_);
    QueryPerformanceFrequency(&qpc_frequency_);
}

EventLogger::~EventLogger() {
    close();
}

bool EventLogger::open(const std::filesystem::path& path, std::string& error) {
    std::scoped_lock lock(mutex_);
    stream_.open(path, std::ios::binary | std::ios::out | std::ios::trunc);
    if (!stream_) {
        error = "unable to create log file";
        return false;
    }
    path_ = path;
    return true;
}

bool EventLogger::enabled() const noexcept {
    std::scoped_lock lock(mutex_);
    return stream_.is_open();
}

const std::filesystem::path& EventLogger::path() const noexcept {
    return path_;
}

void EventLogger::write(const std::string_view event, std::vector<LogField> fields) {
    std::scoped_lock lock(mutex_);
    if (!stream_.is_open()) {
        return;
    }

    LARGE_INTEGER qpc{};
    QueryPerformanceCounter(&qpc);
    const auto delta = static_cast<std::uint64_t>(qpc.QuadPart - qpc_start_.QuadPart);
    const auto frequency = static_cast<std::uint64_t>(qpc_frequency_.QuadPart);
    const auto monotonic_us = frequency == 0 ? 0 : (delta * 1'000'000ULL) / frequency;

    stream_ << '{'
            << "\"schema_version\":1"
            << ",\"sequence\":" << ++sequence_
            << ",\"utc\":\"" << utc_now() << '"'
            << ",\"utc_source\":\"GetSystemTimePreciseAsFileTime\""
            << ",\"qpc_ticks\":" << static_cast<std::uint64_t>(qpc.QuadPart)
            << ",\"qpc_frequency\":" << frequency
            << ",\"monotonic_us\":" << monotonic_us
            << ",\"event\":\"" << escape_json(event) << '"';

    for (const auto& field : fields) {
        stream_ << ",\"" << escape_json(field.key) << "\":";
        std::visit([this](const auto& value) {
            using T = std::decay_t<decltype(value)>;
            if constexpr (std::is_same_v<T, std::string>) {
                stream_ << '"' << escape_json(value) << '"';
            } else if constexpr (std::is_same_v<T, bool>) {
                stream_ << (value ? "true" : "false");
            } else {
                stream_ << value;
            }
        }, field.value);
    }
    stream_ << "}\n";
    stream_.flush();
}

void EventLogger::close() {
    std::scoped_lock lock(mutex_);
    if (stream_.is_open()) {
        stream_.flush();
        stream_.close();
    }
}

std::string EventLogger::escape_json(const std::string_view value) {
    std::ostringstream escaped;
    for (const unsigned char character : value) {
        switch (character) {
            case '"': escaped << "\\\""; break;
            case '\\': escaped << "\\\\"; break;
            case '\b': escaped << "\\b"; break;
            case '\f': escaped << "\\f"; break;
            case '\n': escaped << "\\n"; break;
            case '\r': escaped << "\\r"; break;
            case '\t': escaped << "\\t"; break;
            default:
                if (character < 0x20) {
                    escaped << "\\u"
                            << std::hex << std::setw(4) << std::setfill('0')
                            << static_cast<unsigned>(character)
                            << std::dec;
                } else {
                    escaped << static_cast<char>(character);
                }
        }
    }
    return escaped.str();
}

std::string EventLogger::utc_now() {
    FILETIME file_time{};
    GetSystemTimePreciseAsFileTime(&file_time);
    ULARGE_INTEGER ticks{};
    ticks.LowPart = file_time.dwLowDateTime;
    ticks.HighPart = file_time.dwHighDateTime;

    constexpr std::uint64_t kWindowsToUnixEpoch = 116444736000000000ULL;
    const std::uint64_t unix_ticks = ticks.QuadPart - kWindowsToUnixEpoch;
    const std::time_t seconds = static_cast<std::time_t>(unix_ticks / 10'000'000ULL);
    const auto fraction = unix_ticks % 10'000'000ULL;
    std::tm utc{};
    gmtime_s(&utc, &seconds);

    std::array<char, 40> buffer{};
    std::snprintf(buffer.data(), buffer.size(),
                  "%04d-%02d-%02dT%02d:%02d:%02d.%07lluZ",
                  utc.tm_year + 1900, utc.tm_mon + 1, utc.tm_mday,
                  utc.tm_hour, utc.tm_min, utc.tm_sec,
                  static_cast<unsigned long long>(fraction));
    return buffer.data();
}

}  // namespace hid_mouse
