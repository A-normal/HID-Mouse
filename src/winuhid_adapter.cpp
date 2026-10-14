#include "winuhid_adapter.hpp"

#include "event_logger.hpp"
#include "platform.hpp"

#include <algorithm>
#include <limits>

namespace hid_mouse {

WinUHidAdapter::WinUHidAdapter(EventLogger& logger) : logger_(logger) {}

WinUHidAdapter::~WinUHidAdapter() {
    destroy_mouse();
    unload();
}

bool WinUHidAdapter::load(const std::filesystem::path& directory, std::string& error) {
    const auto core_path = directory / L"WinUHid.dll";
    const auto devices_path = directory / L"WinUHidDevs.dll";

    core_module_ = LoadLibraryExW(
        core_path.c_str(), nullptr,
        LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR | LOAD_LIBRARY_SEARCH_SYSTEM32);
    if (core_module_ == nullptr) {
        const DWORD code = GetLastError();
        error = "failed to load " + wide_to_utf8(core_path.wstring()) + ": " +
                format_win32_error(code);
        logger_.write("winuhid_load", {
            {"component", std::string("core")},
            {"path", wide_to_utf8(core_path.wstring())},
            {"ok", false},
            {"win32_error", static_cast<std::uint64_t>(code)},
        });
        return false;
    }

    if (!resolve(core_module_, "WinUHidGetDriverInterfaceVersion",
                 get_driver_interface_version_, error)) {
        unload();
        return false;
    }

    devices_module_ = LoadLibraryExW(
        devices_path.c_str(), nullptr,
        LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR | LOAD_LIBRARY_SEARCH_SYSTEM32);
    if (devices_module_ == nullptr) {
        const DWORD code = GetLastError();
        error = "failed to load " + wide_to_utf8(devices_path.wstring()) + ": " +
                format_win32_error(code);
        logger_.write("winuhid_load", {
            {"component", std::string("devices")},
            {"path", wide_to_utf8(devices_path.wstring())},
            {"ok", false},
            {"win32_error", static_cast<std::uint64_t>(code)},
        });
        unload();
        return false;
    }

    if (!resolve(devices_module_, "WinUHidMouseCreate", mouse_create_, error) ||
        !resolve(devices_module_, "WinUHidMouseReportMotion", mouse_report_motion_, error) ||
        !resolve(devices_module_, "WinUHidMouseReportButton", mouse_report_button_, error) ||
        !resolve(devices_module_, "WinUHidMouseReportScroll", mouse_report_scroll_, error) ||
        !resolve(devices_module_, "WinUHidMouseDestroy", mouse_destroy_, error)) {
        unload();
        return false;
    }

    logger_.write("winuhid_load", {
        {"component", std::string("userspace")},
        {"directory", wide_to_utf8(directory.wstring())},
        {"ok", true},
    });
    return true;
}

DWORD WinUHidAdapter::driver_interface_version(std::string& error) const {
    if (get_driver_interface_version_ == nullptr) {
        error = "WinUHid user-mode API is not loaded";
        return 0;
    }
    SetLastError(ERROR_SUCCESS);
    const DWORD version = get_driver_interface_version_();
    const DWORD code = version != 0 ? ERROR_SUCCESS : GetLastError();
    if (version == 0) {
        error = "WinUHid driver is unavailable: " + format_win32_error(code);
    }
    logger_.write("winuhid_driver_probe", {
        {"interface_version", static_cast<std::uint64_t>(version)},
        {"ok", version != 0},
        {"win32_error", static_cast<std::uint64_t>(code)},
    });
    return version;
}

bool WinUHidAdapter::create_mouse(std::string& error) {
    if (mouse_ != nullptr) {
        return true;
    }
    if (mouse_create_ == nullptr) {
        error = "WinUHid mouse API is not loaded";
        return false;
    }
    SetLastError(ERROR_SUCCESS);
    mouse_ = mouse_create_(nullptr);
    const DWORD code = mouse_ != nullptr ? ERROR_SUCCESS : GetLastError();
    logger_.write("virtual_mouse_create", {
        {"ok", mouse_ != nullptr},
        {"win32_error", static_cast<std::uint64_t>(code)},
    });
    if (mouse_ == nullptr) {
        error = "WinUHidMouseCreate failed: " + format_win32_error(code);
        return false;
    }
    return true;
}

bool WinUHidAdapter::send(const CapturedAction& action, std::string& error) {
    if (mouse_ == nullptr) {
        error = "virtual mouse is not created";
        return false;
    }
    bool ok{};
    switch (action.kind) {
        case ActionKind::left_click:
            ok = send_click(error);
            break;
        case ActionKind::relative_move:
            ok = send_move(action.dx, action.dy, error);
            break;
        case ActionKind::vertical_wheel:
            ok = send_wheel(action.wheel_delta, false, error);
            break;
        case ActionKind::horizontal_wheel:
            ok = send_wheel(action.wheel_delta, true, error);
            break;
        case ActionKind::none:
            error = "captured action is empty";
            ok = false;
            break;
    }
    logger_.write("virtual_mouse_send", {
        {"action", std::string(to_string(action.kind))},
        {"source_class", std::string(to_string(action.source))},
        {"dx", static_cast<std::int64_t>(action.dx)},
        {"dy", static_cast<std::int64_t>(action.dy)},
        {"wheel_delta", static_cast<std::int64_t>(action.wheel_delta)},
        {"ok", ok},
        {"win32_error", static_cast<std::uint64_t>(ok ? ERROR_SUCCESS : GetLastError())},
    });
    return ok;
}

bool WinUHidAdapter::reset_buttons(std::string& error) {
    if (mouse_ == nullptr || mouse_report_button_ == nullptr) {
        return true;
    }
    bool ok = true;
    DWORD first_error = ERROR_SUCCESS;
    for (std::uint8_t index = 0; index < 5; ++index) {
        if (mouse_report_button_(mouse_, index, FALSE) == FALSE) {
            if (ok) first_error = GetLastError();
            ok = false;
        }
    }
    logger_.write("virtual_mouse_neutral", {
        {"ok", ok},
        {"win32_error", static_cast<std::uint64_t>(first_error)},
    });
    if (!ok) {
        error = "failed to release all virtual buttons: " + format_win32_error(first_error);
    }
    return ok;
}

void WinUHidAdapter::destroy_mouse() {
    if (mouse_ == nullptr) {
        return;
    }
    std::string ignored;
    reset_buttons(ignored);
    mouse_destroy_(mouse_);
    mouse_ = nullptr;
    logger_.write("virtual_mouse_destroy", {{"ok", true}});
}

void WinUHidAdapter::unload() {
    if (devices_module_ != nullptr) {
        FreeLibrary(devices_module_);
        devices_module_ = nullptr;
    }
    if (core_module_ != nullptr) {
        FreeLibrary(core_module_);
        core_module_ = nullptr;
    }
    get_driver_interface_version_ = nullptr;
    mouse_create_ = nullptr;
    mouse_report_motion_ = nullptr;
    mouse_report_button_ = nullptr;
    mouse_report_scroll_ = nullptr;
    mouse_destroy_ = nullptr;
}

template <typename Function>
bool WinUHidAdapter::resolve(
    const HMODULE module, const char* name, Function& function, std::string& error) {
    function = reinterpret_cast<Function>(GetProcAddress(module, name));
    if (function == nullptr) {
        const DWORD code = GetLastError();
        error = std::string("missing WinUHid export ") + name + ": " + format_win32_error(code);
        return false;
    }
    return true;
}

bool WinUHidAdapter::send_move(std::int32_t dx, std::int32_t dy, std::string& error) {
    while (dx != 0 || dy != 0) {
        const auto part_x = static_cast<SHORT>(std::clamp<std::int32_t>(
            dx, std::numeric_limits<SHORT>::min(), std::numeric_limits<SHORT>::max()));
        const auto part_y = static_cast<SHORT>(std::clamp<std::int32_t>(
            dy, std::numeric_limits<SHORT>::min(), std::numeric_limits<SHORT>::max()));
        if (mouse_report_motion_(mouse_, part_x, part_y) == FALSE) {
            const DWORD code = GetLastError();
            error = "WinUHidMouseReportMotion failed: " + format_win32_error(code);
            return false;
        }
        dx -= part_x;
        dy -= part_y;
    }
    return true;
}

bool WinUHidAdapter::send_click(std::string& error) {
    if (!report_button(0, true, error)) {
        return false;
    }
    Sleep(10);
    return report_button(0, false, error);
}

bool WinUHidAdapter::send_wheel(const std::int32_t delta, const bool horizontal, std::string& error) {
    std::int32_t remaining = delta;
    while (remaining != 0) {
        const auto part = static_cast<SHORT>(std::clamp<std::int32_t>(
            remaining, std::numeric_limits<SHORT>::min(), std::numeric_limits<SHORT>::max()));
        if (mouse_report_scroll_(mouse_, part, horizontal ? TRUE : FALSE) == FALSE) {
            const DWORD code = GetLastError();
            error = "WinUHidMouseReportScroll failed: " + format_win32_error(code);
            return false;
        }
        remaining -= part;
    }
    return true;
}

bool WinUHidAdapter::report_button(
    const std::uint8_t zero_based_index, const bool down, std::string& error) {
    // WinUHidMouse.cpp consumes a zero-based bit index even though the public
    // WUHM_BUTTON_* constants in this pinned revision are one-based.
    if (mouse_report_button_(mouse_, zero_based_index, down ? TRUE : FALSE) == FALSE) {
        const DWORD code = GetLastError();
        error = "WinUHidMouseReportButton failed: " + format_win32_error(code);
        return false;
    }
    return true;
}

}  // namespace hid_mouse
