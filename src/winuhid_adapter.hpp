#pragma once

#include "app_types.hpp"

#include <Windows.h>
#include <WinUHid.h>
#include <WinUHidMouse.h>

#include <cstdint>
#include <filesystem>
#include <string>

namespace hid_mouse {

class EventLogger;

class WinUHidAdapter {
public:
    explicit WinUHidAdapter(EventLogger& logger);
    ~WinUHidAdapter();

    WinUHidAdapter(const WinUHidAdapter&) = delete;
    WinUHidAdapter& operator=(const WinUHidAdapter&) = delete;

    bool load(const std::filesystem::path& directory, std::string& error);
    [[nodiscard]] DWORD driver_interface_version(std::string& error) const;
    bool create_mouse(std::string& error);
    bool send(const CapturedAction& action, std::string& error);
    bool reset_buttons(std::string& error);
    void destroy_mouse();
    void unload();

private:
    template <typename Function>
    static bool resolve(HMODULE module, const char* name, Function& function, std::string& error);

    bool send_move(std::int32_t dx, std::int32_t dy, std::string& error);
    bool send_click(std::string& error);
    bool send_wheel(std::int32_t delta, bool horizontal, std::string& error);
    bool report_button(std::uint8_t zero_based_index, bool down, std::string& error);

    using GetDriverInterfaceVersionFn = decltype(&WinUHidGetDriverInterfaceVersion);
    using MouseCreateFn = decltype(&WinUHidMouseCreate);
    using MouseReportMotionFn = decltype(&WinUHidMouseReportMotion);
    using MouseReportButtonFn = decltype(&WinUHidMouseReportButton);
    using MouseReportScrollFn = decltype(&WinUHidMouseReportScroll);
    using MouseDestroyFn = decltype(&WinUHidMouseDestroy);

    EventLogger& logger_;
    HMODULE core_module_{};
    HMODULE devices_module_{};
    PWINUHID_MOUSE_DEVICE mouse_{};
    GetDriverInterfaceVersionFn get_driver_interface_version_{};
    MouseCreateFn mouse_create_{};
    MouseReportMotionFn mouse_report_motion_{};
    MouseReportButtonFn mouse_report_button_{};
    MouseReportScrollFn mouse_report_scroll_{};
    MouseDestroyFn mouse_destroy_{};
};

}  // namespace hid_mouse
