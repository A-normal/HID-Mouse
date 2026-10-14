#pragma once

#include <Windows.h>

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <mutex>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

namespace hid_mouse {

using LogValue = std::variant<std::string, std::int64_t, std::uint64_t, bool>;

struct LogField {
    std::string key;
    LogValue value;
};

class EventLogger {
public:
    EventLogger();
    ~EventLogger();

    EventLogger(const EventLogger&) = delete;
    EventLogger& operator=(const EventLogger&) = delete;

    bool open(const std::filesystem::path& path, std::string& error);
    [[nodiscard]] bool enabled() const noexcept;
    [[nodiscard]] const std::filesystem::path& path() const noexcept;

    void write(std::string_view event, std::vector<LogField> fields = {});
    void close();

private:
    static std::string escape_json(std::string_view value);
    static std::string utc_now();

    std::ofstream stream_;
    std::filesystem::path path_;
    LARGE_INTEGER qpc_start_{};
    LARGE_INTEGER qpc_frequency_{};
    std::uint64_t sequence_{};
    mutable std::mutex mutex_;
};

}  // namespace hid_mouse
