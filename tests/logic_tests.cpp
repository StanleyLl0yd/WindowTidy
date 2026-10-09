#include "windowtidy/geometry.hpp"
#include "windowtidy/hotkeys.hpp"
#include <cassert>
#include <array>
#include <cstdio>

int main() {
    using namespace wt;
    static_assert(PackHotkey({MOD_ALT, 'H'}) == 0x00010048);
    static_assert(UnpackHotkey(0x0003005a) == Hotkey{MOD_CONTROL | MOD_ALT, 'Z'});
    static_assert(ValidHotkey({}));
    static_assert(!ValidHotkey({0, 'H'}));
    static_assert(!ValidHotkey({MOD_ALT, VK_MENU}));
    static_assert(!ValidHotkey({MOD_WIN, 'H'}));
    static_assert(UniqueHotkeys({Hotkey{MOD_ALT, 'H'}, {MOD_CONTROL, 'Z'}, {}, {}, {}}));
    static_assert(!UniqueHotkeys({Hotkey{MOD_ALT, 'H'}, {MOD_ALT, 'H'}, {}, {}, {}}));

    const RECT left{-1920, 0, 0, 1040};
    const RECT right{0, 0, 2560, 1400};
    const RECT original{-1440, 260, -480, 780};
    const RECT moved = PreserveRelative(left, right, original);
    assert(moved.left == 640 && moved.top == 350);
    assert(moved.right == 1920 && moved.bottom == 1050);

    const RECT centered = CenterInWork(right, RECT{100, 100, 900, 700});
    assert(centered.left == 880 && centered.top == 400);
    assert(centered.right == 1680 && centered.bottom == 1000);

    const RECT clamped = ClampToWork(RECT{-500, -500, 4000, 2000}, right);
    assert(clamped.left == 0 && clamped.top == 0);
    assert(clamped.right == 2560 && clamped.bottom == 1400);

    const RECT small = ClampToWork(RECT{9, 9, 10, 10}, right);
    assert(small.right - small.left == 1 && small.bottom - small.top == 1);

    std::puts("Window Tidy logic tests passed");
    return 0;
}
