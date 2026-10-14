#include "platform.hpp"

#include <array>
#include <sstream>

namespace hid_mouse {

bool is_process_elevated(std::string& error) {
    HANDLE token{};
    if (!OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &token)) {
        error = format_win32_error(GetLastError());
        return false;
    }

    TOKEN_ELEVATION elevation{};
    DWORD returned{};
    const BOOL ok = GetTokenInformation(
        token, TokenElevation, &elevation, sizeof(elevation), &returned);
    const DWORD last_error = ok ? ERROR_SUCCESS : GetLastError();
    CloseHandle(token);
    if (!ok) {
        error = format_win32_error(last_error);
        return false;
    }
    if (elevation.TokenIsElevated == 0) {
        error = "process token is not elevated";
        return false;
    }
    return true;
}

SystemSnapshot collect_system_snapshot() {
    SystemSnapshot snapshot;
    snapshot.process_id = GetCurrentProcessId();
    ProcessIdToSessionId(snapshot.process_id, &snapshot.session_id);
    snapshot.remote_session = GetSystemMetrics(SM_REMOTESESSION) != 0;
    std::string elevation_error;
    snapshot.elevated = is_process_elevated(elevation_error);

    using RtlGetVersionFn = LONG(WINAPI*)(OSVERSIONINFOW*);
    if (const HMODULE ntdll = GetModuleHandleW(L"ntdll.dll")) {
        const auto rtl_get_version = reinterpret_cast<RtlGetVersionFn>(
            GetProcAddress(ntdll, "RtlGetVersion"));
        if (rtl_get_version != nullptr) {
            OSVERSIONINFOW version{};
            version.dwOSVersionInfoSize = sizeof(version);
            if (rtl_get_version(&version) == 0) {
                snapshot.windows_major = version.dwMajorVersion;
                snapshot.windows_minor = version.dwMinorVersion;
                snapshot.windows_build = version.dwBuildNumber;
            }
        }
    }

#if defined(_M_X64)
    snapshot.architecture = "x64";
#elif defined(_M_ARM64)
    snapshot.architecture = "arm64";
#else
    snapshot.architecture = "unsupported";
#endif
    return snapshot;
}

std::filesystem::path executable_directory() {
    std::wstring buffer(32768, L'\0');
    const DWORD length = GetModuleFileNameW(
        nullptr, buffer.data(), static_cast<DWORD>(buffer.size()));
    if (length == 0 || length >= buffer.size()) {
        return {};
    }
    buffer.resize(length);
    return std::filesystem::path(buffer).parent_path();
}

std::string wide_to_utf8(const std::wstring_view value) {
    if (value.empty()) {
        return {};
    }
    const int size = WideCharToMultiByte(
        CP_UTF8, WC_ERR_INVALID_CHARS, value.data(), static_cast<int>(value.size()),
        nullptr, 0, nullptr, nullptr);
    if (size <= 0) {
        return {};
    }
    std::string result(static_cast<std::size_t>(size), '\0');
    WideCharToMultiByte(
        CP_UTF8, WC_ERR_INVALID_CHARS, value.data(), static_cast<int>(value.size()),
        result.data(), size, nullptr, nullptr);
    return result;
}

std::string format_win32_error(const DWORD error) {
    if (error == ERROR_SUCCESS) {
        return "success";
    }
    wchar_t* message{};
    const DWORD length = FormatMessageW(
        FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM |
            FORMAT_MESSAGE_IGNORE_INSERTS,
        nullptr, error, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
        reinterpret_cast<wchar_t*>(&message), 0, nullptr);
    std::string result;
    if (length != 0 && message != nullptr) {
        std::wstring_view view(message, length);
        while (!view.empty() && (view.back() == L'\r' || view.back() == L'\n' ||
                                 view.back() == L' ')) {
            view.remove_suffix(1);
        }
        result = wide_to_utf8(view);
    }
    if (message != nullptr) {
        LocalFree(message);
    }
    if (result.empty()) {
        std::ostringstream fallback;
        fallback << "Win32 error " << error;
        result = fallback.str();
    }
    return result;
}

}  // namespace hid_mouse
