#pragma once
#include <windows.h>
#include <cwchar>
#include <string>
#include <string_view>

namespace wt {
// A clipboard report must never include user-supplied window titles.
inline std::wstring DiagnosticWindowTitle(std::wstring_view title, bool redact,
                                           std::wstring_view hiddenLabel) {
    std::wstring result(redact ? hiddenLabel : title);
    if (!redact) {
        for (auto& character : result) {
            if (character < L' ') character = L' ';
        }
    }
    return result;
}

// Exact Win32 class names, never localized titles. Explorer uses two overflow classes
// across Windows versions; other shell classes are not application targets.
inline bool IsShellClass(const wchar_t* name) noexcept {
    return _wcsicmp(name, L"Shell_TrayWnd") == 0 ||
           _wcsicmp(name, L"Shell_SecondaryTrayWnd") == 0 ||
           _wcsicmp(name, L"Progman") == 0 ||
           _wcsicmp(name, L"WorkerW") == 0 ||
           _wcsicmp(name, L"NotifyIconOverflowWindow") == 0 ||
           _wcsicmp(name, L"TopLevelWindowForOverflowXamlIsland") == 0;
}

enum class TidyStyleReason {
    Eligible,
    NoMinimizeButton,
    ToolWindow,
    OwnedWindow,
    ExcludedTopmost
};

constexpr TidyStyleReason ClassifyTidyStyle(
    LONG_PTR style, LONG_PTR extendedStyle, bool owned, bool skipTopmost) noexcept {
    if ((style & WS_MINIMIZEBOX) == 0) return TidyStyleReason::NoMinimizeButton;
    if ((extendedStyle & WS_EX_TOOLWINDOW) != 0) return TidyStyleReason::ToolWindow;
    if (owned) return TidyStyleReason::OwnedWindow;
    if (skipTopmost && (extendedStyle & WS_EX_TOPMOST) != 0)
        return TidyStyleReason::ExcludedTopmost;
    return TidyStyleReason::Eligible;
}

constexpr bool CanMoveByStyle(LONG_PTR style) noexcept {
    return (style & WS_CAPTION) != 0;
}
} // namespace wt
