#pragma once
#include <windows.h>
#include <array>
#include <cstdint>

namespace wt {
struct Hotkey {
    UINT mods = 0;
    UINT vk = 0;
    constexpr bool operator==(const Hotkey&) const = default;
};

constexpr DWORD PackHotkey(Hotkey hotkey) noexcept {
    return (static_cast<DWORD>(hotkey.mods & 0xffffu) << 16u) |
           static_cast<DWORD>(hotkey.vk & 0xffffu);
}

constexpr Hotkey UnpackHotkey(DWORD value) noexcept {
    return {static_cast<UINT>(value >> 16u), static_cast<UINT>(value & 0xffffu)};
}

constexpr bool ValidHotkey(Hotkey hotkey) noexcept {
    constexpr UINT kAllowed = MOD_ALT | MOD_CONTROL | MOD_SHIFT;
    if (hotkey.vk == 0) return hotkey.mods == 0; // explicitly disabled
    if ((hotkey.mods & kAllowed) == 0 || (hotkey.mods & ~kAllowed) != 0) return false;
    if (hotkey.vk > 0xffu) return false;
    if (hotkey.vk == VK_SHIFT || hotkey.vk == VK_CONTROL || hotkey.vk == VK_MENU ||
        hotkey.vk == VK_LSHIFT || hotkey.vk == VK_RSHIFT ||
        hotkey.vk == VK_LCONTROL || hotkey.vk == VK_RCONTROL ||
        hotkey.vk == VK_LMENU || hotkey.vk == VK_RMENU ||
        hotkey.vk == VK_LWIN || hotkey.vk == VK_RWIN) return false;
    return true;
}

constexpr bool UniqueHotkeys(const std::array<Hotkey, 5>& hotkeys) noexcept {
    for (size_t i = 0; i < hotkeys.size(); ++i) {
        if (!ValidHotkey(hotkeys[i])) return false;
        if (hotkeys[i].vk == 0) continue;
        for (size_t j = i + 1; j < hotkeys.size(); ++j) {
            if (hotkeys[i] == hotkeys[j]) return false;
        }
    }
    return true;
}
} // namespace wt
