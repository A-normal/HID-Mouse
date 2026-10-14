#include "app_types.hpp"

namespace hid_mouse {

std::string_view to_string(const SourceClass value) noexcept {
    switch (value) {
        case SourceClass::unknown: return "unknown";
        case SourceClass::software_injected: return "software_injected";
        case SourceClass::lower_integrity_injected: return "lower_integrity_injected";
        case SourceClass::touch_promoted_mouse: return "touch_promoted_mouse";
        case SourceClass::pen_promoted_mouse: return "pen_promoted_mouse";
    }
    return "unknown";
}

std::string_view to_string(const OnceKind value) noexcept {
    switch (value) {
        case OnceKind::none: return "none";
        case OnceKind::click: return "click";
        case OnceKind::move: return "move";
        case OnceKind::wheel: return "wheel";
    }
    return "none";
}

std::string_view to_string(const ActionKind value) noexcept {
    switch (value) {
        case ActionKind::none: return "none";
        case ActionKind::left_click: return "left_click";
        case ActionKind::relative_move: return "relative_move";
        case ActionKind::vertical_wheel: return "vertical_wheel";
        case ActionKind::horizontal_wheel: return "horizontal_wheel";
    }
    return "none";
}

}  // namespace hid_mouse
