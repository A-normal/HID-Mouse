#pragma once

#include "app_types.hpp"

#include <Windows.h>

namespace hid_mouse {

SourceClass classify_hook_source(DWORD flags, ULONG_PTR extra_info) noexcept;

}  // namespace hid_mouse
