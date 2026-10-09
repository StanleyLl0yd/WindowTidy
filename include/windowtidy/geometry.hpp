#pragma once
#include <windows.h>
#include <algorithm>
#include <cmath>

namespace wt {
inline int Width(const RECT& r) noexcept { return r.right - r.left; }
inline int Height(const RECT& r) noexcept { return r.bottom - r.top; }

inline RECT ClampToWork(RECT desired, const RECT& work) noexcept {
    const int ww = Width(work);
    const int wh = Height(work);
    if (ww <= 0 || wh <= 0) return desired;
    const int w = std::clamp(Width(desired), 1, ww);
    const int h = std::clamp(Height(desired), 1, wh);
    const int left = std::clamp(desired.left, work.left, work.right - w);
    const int top = std::clamp(desired.top, work.top, work.bottom - h);
    return RECT{left, top, left + w, top + h};
}

inline RECT PreserveRelative(const RECT& src, const RECT& dst, const RECT& original) noexcept {
    if (Width(src) <= 0 || Height(src) <= 0 || Width(dst) <= 0 || Height(dst) <= 0) return original;
    const double scaleX = static_cast<double>(Width(dst)) / Width(src);
    const double scaleY = static_cast<double>(Height(dst)) / Height(src);
    const int x = dst.left + static_cast<int>(std::lround((original.left - src.left) * scaleX));
    const int y = dst.top + static_cast<int>(std::lround((original.top - src.top) * scaleY));
    const int w = std::max(1, static_cast<int>(std::lround(Width(original) * scaleX)));
    const int h = std::max(1, static_cast<int>(std::lround(Height(original) * scaleY)));
    return ClampToWork(RECT{x, y, x + w, y + h}, dst);
}

inline RECT CenterInWork(const RECT& dst, const RECT& original) noexcept {
    if (Width(dst) <= 0 || Height(dst) <= 0) return original;
    const int w = std::clamp(Width(original), 1, Width(dst));
    const int h = std::clamp(Height(original), 1, Height(dst));
    const int left = dst.left + (Width(dst) - w) / 2;
    const int top = dst.top + (Height(dst) - h) / 2;
    return RECT{left, top, left + w, top + h};
}
} // namespace wt
