#pragma once

#include <Windows.h>

#include <cstdint>
#include <filesystem>
#include <string>
#include <string_view>

namespace hid_mouse {

struct SystemSnapshot {
    bool elevated{};
    bool remote_session{};
    DWORD process_id{};
    DWORD session_id{};
    DWORD windows_major{};
    DWORD windows_minor{};
    DWORD windows_build{};
    std::string architecture;
};

bool is_process_elevated(std::string& error);
SystemSnapshot collect_system_snapshot();
std::filesystem::path executable_directory();
std::string wide_to_utf8(std::wstring_view value);
std::string format_win32_error(DWORD error);

}  // namespace hid_mouse
