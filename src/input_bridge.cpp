#include "input_bridge.hpp"

#include "event_logger.hpp"
#include "platform.hpp"
#include "source_classifier.hpp"

#include <hidusage.h>

#include <algorithm>
#include <limits>
#include <string>
#include <vector>

namespace hid_mouse {
namespace {

constexpr wchar_t kWindowClassName[] = L"HIDMouseInputBridgeWindow";

std::uint64_t handle_value(const HANDLE handle) {
    return static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(handle));
}

std::int32_t signed_wheel_delta(const DWORD mouse_data) {
    return static_cast<std::int32_t>(static_cast<SHORT>(HIWORD(mouse_data)));
}

}  // namespace

InputBridge* InputBridge::active_ = nullptr;

InputBridge::InputBridge(EventLogger& logger) : logger_(logger) {}

InputBridge::~InputBridge() {
    if (hook_ != nullptr) {
        UnhookWindowsHookEx(hook_);
    }
    unregister_raw_input();
    if (active_ == this) {
        active_ = nullptr;
    }
}

BridgeResult InputBridge::run(const BridgeOptions& options) {
    options_ = options;
    result_ = {};
    hook_head_ = 0;
    hook_tail_ = 0;
    dropped_events_ = 0;
    hook_sequence_ = 0;
    left_down_seen_ = false;
    observation_enabled_ = true;
    replay_delay_active_ = false;
    GetCursorPos(&previous_point_);
    previous_point_valid_ = true;

    if (active_ != nullptr) {
        result_.error = "another InputBridge instance is active";
        result_.win32_error = ERROR_BUSY;
        return result_;
    }
    active_ = this;

    WNDCLASSEXW window_class{};
    window_class.cbSize = sizeof(window_class);
    window_class.lpfnWndProc = &InputBridge::window_proc;
    window_class.hInstance = GetModuleHandleW(nullptr);
    window_class.lpszClassName = kWindowClassName;
    if (RegisterClassExW(&window_class) == 0 && GetLastError() != ERROR_CLASS_ALREADY_EXISTS) {
        result_.win32_error = GetLastError();
        result_.error = "RegisterClassExW failed: " + format_win32_error(result_.win32_error);
        active_ = nullptr;
        return result_;
    }

    window_ = CreateWindowExW(
        0, kWindowClassName, L"HID-Mouse Input Bridge", WS_OVERLAPPED,
        0, 0, 0, 0, nullptr, nullptr, GetModuleHandleW(nullptr), this);
    if (window_ == nullptr) {
        result_.win32_error = GetLastError();
        result_.error = "CreateWindowExW failed: " + format_win32_error(result_.win32_error);
        active_ = nullptr;
        return result_;
    }

    if (!register_raw_input()) {
        result_.win32_error = GetLastError();
        result_.error = "RegisterRawInputDevices failed: " + format_win32_error(result_.win32_error);
        DestroyWindow(window_);
        window_ = nullptr;
        active_ = nullptr;
        return result_;
    }

    enumerate_devices();

    hook_ = SetWindowsHookExW(WH_MOUSE_LL, &InputBridge::hook_proc, GetModuleHandleW(nullptr), 0);
    if (hook_ == nullptr) {
        result_.win32_error = GetLastError();
        result_.error = "SetWindowsHookExW failed: " + format_win32_error(result_.win32_error);
        unregister_raw_input();
        DestroyWindow(window_);
        window_ = nullptr;
        active_ = nullptr;
        return result_;
    }

    if (options_.limit_seconds != 0) {
        if (SetTimer(window_, kLimitTimer, options_.limit_seconds * 1000U, nullptr) == 0) {
            result_.win32_error = GetLastError();
            result_.error = "SetTimer failed: " + format_win32_error(result_.win32_error);
            UnhookWindowsHookEx(hook_);
            hook_ = nullptr;
            unregister_raw_input();
            DestroyWindow(window_);
            window_ = nullptr;
            active_ = nullptr;
            return result_;
        }
    }

    SetConsoleCtrlHandler(&InputBridge::console_handler, TRUE);
    logger_.write("input_bridge_started", {
        {"mode", std::string(options_.mode == BridgeMode::probe ? "probe" : "once")},
        {"once_kind", std::string(to_string(options_.once))},
        {"limit_seconds", static_cast<std::uint64_t>(options_.limit_seconds)},
        {"replay_delay_seconds", static_cast<std::uint64_t>(options_.replay_delay_seconds)},
        {"suppression_enabled", false},
    });

    MSG message{};
    BOOL message_result{};
    while ((message_result = GetMessageW(&message, nullptr, 0, 0)) > 0) {
        TranslateMessage(&message);
        DispatchMessageW(&message);
    }
    if (message_result == -1 && result_.error.empty()) {
        result_.win32_error = GetLastError();
        result_.error = "GetMessageW failed: " + format_win32_error(result_.win32_error);
    }

    SetConsoleCtrlHandler(&InputBridge::console_handler, FALSE);
    remove_hook();
    unregister_raw_input();
    window_ = nullptr;
    active_ = nullptr;

    result_.ok = result_.error.empty();
    logger_.write("input_bridge_stopped", {
        {"ok", result_.ok},
        {"captured", result_.captured},
        {"timed_out", result_.timed_out},
        {"cancelled", result_.cancelled},
        {"dropped_hook_events", dropped_events_},
    });
    return result_;
}

LRESULT CALLBACK InputBridge::window_proc(
    const HWND window, const UINT message, const WPARAM wparam, const LPARAM lparam) {
    InputBridge* self = reinterpret_cast<InputBridge*>(GetWindowLongPtrW(window, GWLP_USERDATA));
    if (message == WM_NCCREATE) {
        const auto* create = reinterpret_cast<const CREATESTRUCTW*>(lparam);
        self = static_cast<InputBridge*>(create->lpCreateParams);
        SetWindowLongPtrW(window, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
    }
    if (self != nullptr) {
        return self->handle_window_message(window, message, wparam, lparam);
    }
    return DefWindowProcW(window, message, wparam, lparam);
}

LRESULT CALLBACK InputBridge::hook_proc(const int code, const WPARAM wparam, const LPARAM lparam) {
    if (code == HC_ACTION && active_ != nullptr && lparam != 0) {
        active_->observe_hook_event(wparam, *reinterpret_cast<const MSLLHOOKSTRUCT*>(lparam));
    }
    return CallNextHookEx(active_ != nullptr ? active_->hook_ : nullptr, code, wparam, lparam);
}

BOOL WINAPI InputBridge::console_handler(const DWORD control_type) {
    if (active_ == nullptr || active_->window_ == nullptr) {
        return FALSE;
    }
    switch (control_type) {
        case CTRL_C_EVENT:
        case CTRL_BREAK_EVENT:
        case CTRL_CLOSE_EVENT:
        case CTRL_LOGOFF_EVENT:
        case CTRL_SHUTDOWN_EVENT:
            PostMessageW(active_->window_, WM_CLOSE, 0, 0);
            return TRUE;
        default:
            return FALSE;
    }
}

LRESULT InputBridge::handle_window_message(
    const HWND window, const UINT message, const WPARAM wparam, const LPARAM lparam) {
    switch (message) {
        case WM_INPUT:
            if (!observation_enabled_) return DefWindowProcW(window, message, wparam, lparam);
            log_raw_input(reinterpret_cast<HRAWINPUT>(lparam));
            return DefWindowProcW(window, message, wparam, lparam);
        case WM_INPUT_DEVICE_CHANGE:
            if (!observation_enabled_) return 0;
            log_device(reinterpret_cast<HANDLE>(lparam),
                       wparam == GIDC_ARRIVAL ? "arrival" : "removal");
            return 0;
        case kHookQueueMessage:
            if (!observation_enabled_) return 0;
            drain_hook_events();
            return 0;
        case kCaptureCompleteMessage:
            drain_hook_events();
            begin_replay_delay();
            return 0;
        case WM_TIMER:
            if (wparam == kLimitTimer) {
                logger_.write("capture_limit_reached", {
                    {"seconds", static_cast<std::uint64_t>(options_.limit_seconds)},
                });
                finish_window(true, false);
                return 0;
            }
            if (wparam == kReplayDelayTimer) {
                KillTimer(window_, kReplayDelayTimer);
                replay_delay_active_ = false;
                logger_.write("replay_delay_completed", {
                    {"seconds", static_cast<std::uint64_t>(options_.replay_delay_seconds)},
                });
                finish_window(false, false);
                return 0;
            }
            break;
        case WM_CLOSE:
            logger_.write("console_stop_requested");
            finish_window(false, true);
            return 0;
        case WM_DESTROY:
            PostQuitMessage(0);
            return 0;
        default:
            break;
    }
    return DefWindowProcW(window, message, wparam, lparam);
}

void InputBridge::observe_hook_event(const WPARAM message, const MSLLHOOKSTRUCT& data) {
    if (!observation_enabled_) {
        return;
    }
    HookEvent event;
    event.sequence = ++hook_sequence_;
    event.message = message;
    event.point = data.pt;
    event.mouse_data = data.mouseData;
    event.flags = data.flags;
    event.time = data.time;
    event.extra_info = data.dwExtraInfo;

    if (!push_hook_event(event)) {
        ++dropped_events_;
    } else if (window_ != nullptr) {
        PostMessageW(window_, kHookQueueMessage, 0, 0);
    }

    if (options_.mode == BridgeMode::once && !result_.captured) {
        try_capture(event);
    }
}

void InputBridge::try_capture(const HookEvent& event) {
    const SourceClass source = classify_hook_source(event.flags, event.extra_info);
    switch (options_.once) {
        case OnceKind::click:
            if (event.message == WM_LBUTTONDOWN && !left_down_seen_) {
                left_down_seen_ = true;
                left_down_source_ = source;
            } else if (event.message == WM_LBUTTONUP && left_down_seen_) {
                result_.captured = true;
                result_.action.kind = ActionKind::left_click;
                result_.action.source = left_down_source_;
                result_.action.hook_sequence = event.sequence;
            }
            break;
        case OnceKind::move:
            if (event.message == WM_MOUSEMOVE) {
                if (previous_point_valid_) {
                    const LONG dx = event.point.x - previous_point_.x;
                    const LONG dy = event.point.y - previous_point_.y;
                    if (dx != 0 || dy != 0) {
                        result_.captured = true;
                        result_.action.kind = ActionKind::relative_move;
                        result_.action.source = source;
                        result_.action.dx = dx;
                        result_.action.dy = dy;
                        result_.action.hook_sequence = event.sequence;
                    }
                }
                previous_point_ = event.point;
                previous_point_valid_ = true;
            }
            break;
        case OnceKind::wheel:
            if (event.message == WM_MOUSEWHEEL || event.message == WM_MOUSEHWHEEL) {
                result_.captured = true;
                result_.action.kind = event.message == WM_MOUSEWHEEL
                    ? ActionKind::vertical_wheel
                    : ActionKind::horizontal_wheel;
                result_.action.source = source;
                result_.action.wheel_delta = signed_wheel_delta(event.mouse_data);
                result_.action.hook_sequence = event.sequence;
            }
            break;
        case OnceKind::none:
            break;
    }
    if (result_.captured && window_ != nullptr) {
        PostMessageW(window_, kCaptureCompleteMessage, 0, 0);
    }
}

bool InputBridge::push_hook_event(const HookEvent& event) noexcept {
    const std::size_t next = (hook_head_ + 1) % hook_queue_.size();
    if (next == hook_tail_) {
        return false;
    }
    hook_queue_[hook_head_] = event;
    hook_head_ = next;
    return true;
}

bool InputBridge::pop_hook_event(HookEvent& event) noexcept {
    if (hook_tail_ == hook_head_) {
        return false;
    }
    event = hook_queue_[hook_tail_];
    hook_tail_ = (hook_tail_ + 1) % hook_queue_.size();
    return true;
}

void InputBridge::drain_hook_events() {
    HookEvent event;
    while (pop_hook_event(event)) {
        log_hook_event(event);
    }
}

void InputBridge::log_hook_event(const HookEvent& event) {
    const SourceClass source = classify_hook_source(event.flags, event.extra_info);
    logger_.write("low_level_mouse", {
        {"hook_sequence", event.sequence},
        {"message", hook_message_name(event.message)},
        {"x", static_cast<std::int64_t>(event.point.x)},
        {"y", static_cast<std::int64_t>(event.point.y)},
        {"mouse_data", static_cast<std::uint64_t>(event.mouse_data)},
        {"wheel_delta", static_cast<std::int64_t>(signed_wheel_delta(event.mouse_data))},
        {"flags", static_cast<std::uint64_t>(event.flags)},
        {"hook_time_ms", static_cast<std::uint64_t>(event.time)},
        {"extra_info", static_cast<std::uint64_t>(event.extra_info)},
        {"source_class", std::string(to_string(source))},
        {"remote_session_candidate", GetSystemMetrics(SM_REMOTESESSION) != 0},
        {"suppressed", false},
        {"decision", std::string(options_.mode == BridgeMode::probe ? "observe" : "observe_for_once")},
    });
}

void InputBridge::log_raw_input(const HRAWINPUT handle) {
    UINT size{};
    if (GetRawInputData(handle, RID_INPUT, nullptr, &size, sizeof(RAWINPUTHEADER)) != 0 || size == 0) {
        logger_.write("raw_input_error", {
            {"stage", std::string("size")},
            {"win32_error", static_cast<std::uint64_t>(GetLastError())},
        });
        return;
    }
    std::vector<std::byte> buffer(size);
    if (GetRawInputData(handle, RID_INPUT, buffer.data(), &size, sizeof(RAWINPUTHEADER)) == UINT_MAX) {
        logger_.write("raw_input_error", {
            {"stage", std::string("read")},
            {"win32_error", static_cast<std::uint64_t>(GetLastError())},
        });
        return;
    }
    const auto* input = reinterpret_cast<const RAWINPUT*>(buffer.data());
    if (input->header.dwType != RIM_TYPEMOUSE) {
        return;
    }
    const auto& mouse = input->data.mouse;
    logger_.write("raw_mouse", {
        {"device_handle", handle_value(input->header.hDevice)},
        {"raw_flags", static_cast<std::uint64_t>(mouse.usFlags)},
        {"button_flags", static_cast<std::uint64_t>(mouse.usButtonFlags)},
        {"button_data", static_cast<std::int64_t>(static_cast<SHORT>(mouse.usButtonData))},
        {"raw_buttons", static_cast<std::uint64_t>(mouse.ulRawButtons)},
        {"dx", static_cast<std::int64_t>(mouse.lLastX)},
        {"dy", static_cast<std::int64_t>(mouse.lLastY)},
        {"extra_information", static_cast<std::uint64_t>(mouse.ulExtraInformation)},
    });
}

void InputBridge::log_device(const HANDLE device, const std::string_view reason) {
    UINT name_size{};
    std::wstring name;
    if (GetRawInputDeviceInfoW(device, RIDI_DEVICENAME, nullptr, &name_size) == 0 && name_size != 0) {
        std::vector<wchar_t> buffer(name_size + 1, L'\0');
        UINT capacity = name_size;
        if (GetRawInputDeviceInfoW(device, RIDI_DEVICENAME, buffer.data(), &capacity) != UINT_MAX) {
            name.assign(buffer.data());
        }
    }

    RID_DEVICE_INFO info{};
    info.cbSize = sizeof(info);
    UINT info_size = sizeof(info);
    const bool has_info = GetRawInputDeviceInfoW(device, RIDI_DEVICEINFO, &info, &info_size) != UINT_MAX;

    std::vector<LogField> fields{
        {"reason", std::string(reason)},
        {"device_handle", handle_value(device)},
        {"device_path", wide_to_utf8(name)},
        {"device_info_ok", has_info},
    };
    if (has_info) {
        fields.push_back({"device_type", static_cast<std::uint64_t>(info.dwType)});
        if (info.dwType == RIM_TYPEMOUSE) {
            fields.push_back({"mouse_id", static_cast<std::uint64_t>(info.mouse.dwId)});
            fields.push_back({"button_count", static_cast<std::uint64_t>(info.mouse.dwNumberOfButtons)});
            fields.push_back({"sample_rate", static_cast<std::uint64_t>(info.mouse.dwSampleRate)});
            fields.push_back({"horizontal_wheel", info.mouse.fHasHorizontalWheel != FALSE});
        } else if (info.dwType == RIM_TYPEHID) {
            fields.push_back({"vendor_id", static_cast<std::uint64_t>(info.hid.dwVendorId)});
            fields.push_back({"product_id", static_cast<std::uint64_t>(info.hid.dwProductId)});
            fields.push_back({"usage_page", static_cast<std::uint64_t>(info.hid.usUsagePage)});
            fields.push_back({"usage", static_cast<std::uint64_t>(info.hid.usUsage)});
        }
    }
    logger_.write("raw_device", std::move(fields));
}

void InputBridge::enumerate_devices() {
    UINT count{};
    if (GetRawInputDeviceList(nullptr, &count, sizeof(RAWINPUTDEVICELIST)) != 0) {
        logger_.write("raw_device_list_error", {
            {"stage", std::string("count")},
            {"win32_error", static_cast<std::uint64_t>(GetLastError())},
        });
        return;
    }
    std::vector<RAWINPUTDEVICELIST> devices(count);
    if (count != 0 && GetRawInputDeviceList(devices.data(), &count, sizeof(RAWINPUTDEVICELIST)) == UINT_MAX) {
        logger_.write("raw_device_list_error", {
            {"stage", std::string("read")},
            {"win32_error", static_cast<std::uint64_t>(GetLastError())},
        });
        return;
    }
    logger_.write("raw_device_snapshot", {
        {"device_count", static_cast<std::uint64_t>(count)},
    });
    for (UINT index = 0; index < count; ++index) {
        log_device(devices[index].hDevice, "startup");
    }
}

bool InputBridge::register_raw_input() {
    RAWINPUTDEVICE mouse{};
    mouse.usUsagePage = HID_USAGE_PAGE_GENERIC;
    mouse.usUsage = HID_USAGE_GENERIC_MOUSE;
    mouse.dwFlags = RIDEV_INPUTSINK | RIDEV_DEVNOTIFY;
    mouse.hwndTarget = window_;
    return RegisterRawInputDevices(&mouse, 1, sizeof(mouse)) != FALSE;
}

void InputBridge::unregister_raw_input() noexcept {
    if (window_ == nullptr) {
        return;
    }
    RAWINPUTDEVICE mouse{};
    mouse.usUsagePage = HID_USAGE_PAGE_GENERIC;
    mouse.usUsage = HID_USAGE_GENERIC_MOUSE;
    mouse.dwFlags = RIDEV_REMOVE;
    mouse.hwndTarget = nullptr;
    RegisterRawInputDevices(&mouse, 1, sizeof(mouse));
}

void InputBridge::begin_replay_delay() {
    if (replay_delay_active_ || !result_.captured || window_ == nullptr) {
        return;
    }

    observation_enabled_ = false;
    KillTimer(window_, kLimitTimer);
    remove_hook();
    unregister_raw_input();
    logger_.write("capture_completed", {
        {"action", std::string(to_string(result_.action.kind))},
        {"source_class", std::string(to_string(result_.action.source))},
        {"hook_sequence", result_.action.hook_sequence},
        {"dx", static_cast<std::int64_t>(result_.action.dx)},
        {"dy", static_cast<std::int64_t>(result_.action.dy)},
        {"wheel_delta", static_cast<std::int64_t>(result_.action.wheel_delta)},
    });

    if (options_.replay_delay_seconds == 0) {
        logger_.write("replay_delay_completed", {
            {"seconds", static_cast<std::uint64_t>(0)},
        });
        finish_window(false, false);
        return;
    }

    replay_delay_active_ = true;
    logger_.write("replay_delay_started", {
        {"seconds", static_cast<std::uint64_t>(options_.replay_delay_seconds)},
        {"other_input_policy", std::string("not_observed")},
    });
    if (SetTimer(window_, kReplayDelayTimer, options_.replay_delay_seconds * 1000U, nullptr) == 0) {
        result_.win32_error = GetLastError();
        result_.error = "SetTimer for replay delay failed: " + format_win32_error(result_.win32_error);
        replay_delay_active_ = false;
        finish_window(false, false);
    }
}

void InputBridge::remove_hook() {
    if (hook_ == nullptr) {
        return;
    }
    const BOOL unhooked = UnhookWindowsHookEx(hook_);
    const DWORD error = unhooked ? ERROR_SUCCESS : GetLastError();
    logger_.write("hook_removed", {
        {"ok", unhooked != FALSE},
        {"win32_error", static_cast<std::uint64_t>(error)},
    });
    if (unhooked) {
        hook_ = nullptr;
    }
}

void InputBridge::finish_window(const bool timed_out, const bool cancelled) {
    result_.timed_out = timed_out;
    result_.cancelled = cancelled;
    if (window_ != nullptr) {
        KillTimer(window_, kLimitTimer);
        KillTimer(window_, kReplayDelayTimer);
        unregister_raw_input();
        const HWND window = window_;
        window_ = nullptr;
        DestroyWindow(window);
    }
}

std::string InputBridge::hook_message_name(const WPARAM message) {
    switch (message) {
        case WM_MOUSEMOVE: return "WM_MOUSEMOVE";
        case WM_LBUTTONDOWN: return "WM_LBUTTONDOWN";
        case WM_LBUTTONUP: return "WM_LBUTTONUP";
        case WM_RBUTTONDOWN: return "WM_RBUTTONDOWN";
        case WM_RBUTTONUP: return "WM_RBUTTONUP";
        case WM_MBUTTONDOWN: return "WM_MBUTTONDOWN";
        case WM_MBUTTONUP: return "WM_MBUTTONUP";
        case WM_XBUTTONDOWN: return "WM_XBUTTONDOWN";
        case WM_XBUTTONUP: return "WM_XBUTTONUP";
        case WM_MOUSEWHEEL: return "WM_MOUSEWHEEL";
        case WM_MOUSEHWHEEL: return "WM_MOUSEHWHEEL";
        default: return "unknown";
    }
}

}  // namespace hid_mouse
