#pragma once

#include <cstdint>
#include <string_view>

namespace hid_mouse {

enum class SourceClass {
    unknown,
    software_injected,
    lower_integrity_injected,
    touch_promoted_mouse,
    pen_promoted_mouse,
};

enum class OnceKind {
    none,
    click,
    move,
    wheel,
};

enum class ActionKind {
    none,
    left_click,
    relative_move,
    vertical_wheel,
    horizontal_wheel,
};

struct CapturedAction {
    ActionKind kind{ActionKind::none};
    SourceClass source{SourceClass::unknown};
    std::int32_t dx{};
    std::int32_t dy{};
    std::int32_t wheel_delta{};
    std::uint64_t hook_sequence{};
};

std::string_view to_string(SourceClass value) noexcept;
std::string_view to_string(OnceKind value) noexcept;
std::string_view to_string(ActionKind value) noexcept;

}  // namespace hid_mouse
