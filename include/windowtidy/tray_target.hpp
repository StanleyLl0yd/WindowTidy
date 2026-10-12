#pragma once
#include <windows.h>

namespace wt {
struct WindowIdentity {
    HWND hwnd{};
    DWORD pid{};
    DWORD thread{};
};

inline WindowIdentity CaptureWindowIdentity(HWND window) noexcept {
    if (!window) return {};
    DWORD pid{};
    const DWORD thread = GetWindowThreadProcessId(window, &pid);
    return thread != 0 && pid != 0 ? WindowIdentity{window, pid, thread} : WindowIdentity{};
}

inline bool MatchesWindowIdentity(const WindowIdentity& captured) noexcept {
    if (!captured.hwnd || !captured.pid || !captured.thread) return false;
    DWORD pid{};
    const DWORD thread = GetWindowThreadProcessId(captured.hwnd, &pid);
    return thread == captured.thread && pid == captured.pid;
}
} // namespace wt
