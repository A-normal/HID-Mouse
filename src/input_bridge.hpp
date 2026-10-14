#pragma once

#include "app_types.hpp"

#include <Windows.h>

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>

namespace hid_mouse {

class EventLogger;

enum class BridgeMode {
    probe,
    once,
};

struct BridgeOptions {
    BridgeMode mode{BridgeMode::probe};
    OnceKind once{OnceKind::none};
    unsigned limit_seconds{};
    unsigned replay_delay_seconds{};
};

struct BridgeResult {
    bool ok{};
    bool captured{};
    bool timed_out{};
    bool cancelled{};
    CapturedAction action;
    DWORD win32_error{};
    std::string error;
};

class InputBridge {
public:
    explicit InputBridge(EventLogger& logger);
    ~InputBridge();

    InputBridge(const InputBridge&) = delete;
    InputBridge& operator=(const InputBridge&) = delete;

    BridgeResult run(const BridgeOptions& options);

private:
    struct HookEvent {
        std::uint64_t sequence{};
        WPARAM message{};
        POINT point{};
        DWORD mouse_data{};
        DWORD flags{};
        DWORD time{};
        ULONG_PTR extra_info{};
    };

    static constexpr std::size_t kHookQueueCapacity = 4096;
    static constexpr UINT kHookQueueMessage = WM_APP + 1;
    static constexpr UINT kCaptureCompleteMessage = WM_APP + 2;
    static constexpr UINT_PTR kLimitTimer = 1;
    static constexpr UINT_PTR kReplayDelayTimer = 2;

    static LRESULT CALLBACK window_proc(HWND window, UINT message, WPARAM wparam, LPARAM lparam);
    static LRESULT CALLBACK hook_proc(int code, WPARAM wparam, LPARAM lparam);
    static BOOL WINAPI console_handler(DWORD control_type);

    LRESULT handle_window_message(HWND window, UINT message, WPARAM wparam, LPARAM lparam);
    void observe_hook_event(WPARAM message, const MSLLHOOKSTRUCT& data);
    void try_capture(const HookEvent& event);
    bool push_hook_event(const HookEvent& event) noexcept;
    bool pop_hook_event(HookEvent& event) noexcept;
    void drain_hook_events();
    void log_hook_event(const HookEvent& event);
    void log_raw_input(HRAWINPUT handle);
    void log_device(HANDLE device, std::string_view reason);
    void enumerate_devices();
    bool register_raw_input();
    void unregister_raw_input() noexcept;
    void begin_replay_delay();
    void remove_hook();
    void finish_window(bool timed_out, bool cancelled);
    static std::string hook_message_name(WPARAM message);

    EventLogger& logger_;
    BridgeOptions options_;
    BridgeResult result_;
    HWND window_{};
    HHOOK hook_{};
    POINT previous_point_{};
    bool previous_point_valid_{};
    bool left_down_seen_{};
    SourceClass left_down_source_{SourceClass::unknown};
    std::uint64_t hook_sequence_{};
    std::uint64_t dropped_events_{};
    std::array<HookEvent, kHookQueueCapacity> hook_queue_{};
    std::size_t hook_head_{};
    std::size_t hook_tail_{};
    bool observation_enabled_{};
    bool replay_delay_active_{};

    static InputBridge* active_;
};

}  // namespace hid_mouse
