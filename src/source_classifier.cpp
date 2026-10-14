#include "source_classifier.hpp"

namespace hid_mouse {
namespace {

constexpr ULONG_PTR kPointerSignature = 0xFF515700;
constexpr ULONG_PTR kPointerSignatureMask = 0xFFFFFF00;
constexpr ULONG_PTR kTouchBit = 0x80;

}  // namespace

SourceClass classify_hook_source(const DWORD flags, const ULONG_PTR extra_info) noexcept {
    if ((extra_info & kPointerSignatureMask) == kPointerSignature) {
        return (extra_info & kTouchBit) != 0
            ? SourceClass::touch_promoted_mouse
            : SourceClass::pen_promoted_mouse;
    }
    if ((flags & LLMHF_LOWER_IL_INJECTED) != 0) {
        return SourceClass::lower_integrity_injected;
    }
    if ((flags & LLMHF_INJECTED) != 0) {
        return SourceClass::software_injected;
    }
    return SourceClass::unknown;
}

}  // namespace hid_mouse
