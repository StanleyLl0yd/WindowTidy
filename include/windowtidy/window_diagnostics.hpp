#pragma once
#include <windows.h>

namespace wt {
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
