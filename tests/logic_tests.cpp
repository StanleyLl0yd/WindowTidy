#include "windowtidy/geometry.hpp"
#include "windowtidy/hotkeys.hpp"
#include "windowtidy/localization.hpp"
#include "windowtidy/menu_title.hpp"
#include "windowtidy/tray_target.hpp"
#include "windowtidy/window_diagnostics.hpp"
#include <array>
#include <cassert>
#include <cstdio>
#include <cwchar>

int main() {
    using namespace wt;
    assert(SafeMenuLabel(L"Notes & tasks") == L"Notes && tasks");
    assert(SafeMenuLabel(L"Title\tCtrl+K\n") == L"Title Ctrl+K ");
    assert(SafeMenuLabel(L"") == L"");
    // Shared window-report redaction must not include any user-supplied title text.
    assert(DiagnosticWindowTitle(L"private@example.invalid", true, L"[hidden]") == L"[hidden]");
    assert(DiagnosticWindowTitle(L"C:\\Test\\private.txt", true, L"[hidden]") == L"[hidden]");
    assert(DiagnosticWindowTitle(L"Title\tsecond line\n", false, L"[hidden]") == L"Title second line ");
    assert(DiagnosticWindowTitle(L"", true, L"[hidden]") == L"[hidden]");
    // Native dialog accelerators must be present and unique in both translations.
    constexpr std::array settingsLabels{
        TextId::TidyLabel, TextId::TopmostLabel, TextId::NextLabel,
        TextId::PreviousLabel, TextId::MinimizeLabel, TextId::SkipTopmost,
        TextId::OnlyCurrent, TextId::MoveModeLabel, TextId::RunAtStartup
    };
    for (const auto lang : {Language::English, Language::Russian}) {
        std::array<wchar_t, settingsLabels.size()> keys{};
        for (size_t i = 0; i < settingsLabels.size(); ++i) {
            const wchar_t* marker = std::wcschr(Text(settingsLabels[i], lang), L'&');
            assert(marker != nullptr && marker[1] != 0 && marker[1] != L'&');
            keys[i] = marker[1];
            for (size_t previous = 0; previous < i; ++previous) {
                assert(keys[i] != keys[previous]);
            }
        }
    }

    assert(!CaptureWindowIdentity(nullptr).hwnd);
    assert(!MatchesWindowIdentity({}));
    const HWND probe = CreateWindowExW(0, L"STATIC", L"Identity probe", WS_POPUP,
                                      0, 0, 16, 16, nullptr, nullptr, GetModuleHandleW(nullptr), nullptr);
    assert(probe != nullptr);
    const auto identity = CaptureWindowIdentity(probe);
    assert(identity.hwnd == probe && identity.pid == GetCurrentProcessId());
    assert(identity.thread == GetCurrentThreadId());
    assert(MatchesWindowIdentity(identity));
    auto wrongPid = identity;
    ++wrongPid.pid;
    assert(!MatchesWindowIdentity(wrongPid));
    auto wrongThread = identity;
    ++wrongThread.thread;
    assert(!MatchesWindowIdentity(wrongThread));
    assert(DestroyWindow(probe));
    assert(!MatchesWindowIdentity(identity));
    assert(IsShellClass(L"TopLevelWindowForOverflowXamlIsland"));
    assert(IsShellClass(L"NotifyIconOverflowWindow"));
    assert(IsShellClass(L"Shell_TrayWnd"));
    assert(IsShellClass(L"shell_traywnd"));
    assert(IsShellClass(L"Shell_SecondaryTrayWnd"));
    assert(!IsShellClass(L"TaskManagerWindow"));
    assert(!IsShellClass(L"MMCMainFrame"));
    assert(!IsShellClass(L"Qt51519QWindow"));
    assert(!IsShellClass(L"Chrome_WidgetWin_1"));
    static_assert(ClassifyTidyStyle(WS_MINIMIZEBOX, 0, false, true) == TidyStyleReason::Eligible);
    static_assert(ClassifyTidyStyle(0, 0, false, false) == TidyStyleReason::NoMinimizeButton);
    static_assert(ClassifyTidyStyle(WS_MINIMIZEBOX, WS_EX_TOOLWINDOW, false, false) == TidyStyleReason::ToolWindow);
    static_assert(ClassifyTidyStyle(WS_MINIMIZEBOX, 0, true, false) == TidyStyleReason::OwnedWindow);
    static_assert(ClassifyTidyStyle(WS_MINIMIZEBOX, WS_EX_TOPMOST, false, true) == TidyStyleReason::ExcludedTopmost);
    static_assert(ClassifyTidyStyle(WS_MINIMIZEBOX, WS_EX_TOPMOST, false, false) == TidyStyleReason::Eligible);
    static_assert(!CanMoveByStyle(WS_POPUP));
    static_assert(CanMoveByStyle(WS_CAPTION));
    static_assert(LanguageForUiLang(MAKELANGID(LANG_RUSSIAN, SUBLANG_DEFAULT)) == Language::Russian);
    static_assert(LanguageForUiLang(MAKELANGID(LANG_ENGLISH, SUBLANG_ENGLISH_US)) == Language::English);
    static_assert(LanguageForUiLang(MAKELANGID(LANG_GERMAN, SUBLANG_GERMAN)) == Language::English);
    static_assert(Text(TextId::TrayExit, Language::Russian)[0] != Text(TextId::TrayExit, Language::English)[0]);
    static_assert(PackHotkey({MOD_ALT, 'H'}) == 0x00010048);
    static_assert(UnpackHotkey(0x0003005a) == Hotkey{MOD_CONTROL | MOD_ALT, 'Z'});
    static_assert(ValidHotkey({}));
    static_assert(!ValidHotkey({0, 'H'}));
    static_assert(!ValidHotkey({MOD_ALT, VK_MENU}));
    static_assert(!ValidHotkey({MOD_WIN, 'H'}));
    static_assert(!ValidHotkey({MOD_ALT, VK_LMENU}));
    static_assert(!ValidHotkey({MOD_CONTROL, VK_RCONTROL}));
    static_assert(!ValidHotkey({MOD_SHIFT, VK_LSHIFT}));
    static_assert(!ValidHotkey({MOD_ALT, 256}));
    static_assert(ValidHotkey({MOD_CONTROL | MOD_ALT, VK_RIGHT}));
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

    const RECT shifted{-900, -600, 100, 400};
    const RECT tooFar{500, 500, 1400, 1600};
    const RECT inside = ClampToWork(tooFar, shifted);
    assert(inside.left == -800 && inside.top == -600);
    assert(inside.right == 100 && inside.bottom == 400);

    const RECT vertical{0, -1200, 1080, 0};
    const RECT mid = CenterInWork(vertical, RECT{0, 0, 900, 500});
    assert(mid.left == 90 && mid.top == -850);
    assert(mid.right == 990 && mid.bottom == -350);

    // Work areas for primary, negative-origin, top/bottom/left/right taskbars,
    // portrait screens and odd pixel dimensions (geometry only, not actual DPI).
    constexpr std::array<RECT, 7> workAreas{{
        {0, 40, 1920, 1080}, {0, 0, 1920, 1032},
        {64, 0, 1920, 1080}, {0, 0, 1856, 1080},
        {-1920, -1080, 0, 0}, {-1080, -1920, 0, 0},
        {13, -1379, 1378, 0}
    }};
    constexpr std::array<RECT, 8> desiredWindows{{
        {100, 100, 800, 600}, {2200, 1500, 3400, 2600},
        {-2400, -2100, -1600, -1700}, {0, 0, 1, 1},
        {500, 500, 500, 500}, {0, 0, 10000, 10000},
        {100, 150, 90, 140}, {-1800, -500, 100, 900}
    }};
    const auto within = [](const RECT& rectangle, const RECT& work) {
        return Width(rectangle) >= 1 && Height(rectangle) >= 1 &&
            rectangle.left >= work.left && rectangle.top >= work.top &&
            rectangle.right <= work.right && rectangle.bottom <= work.bottom;
    };
    for (const RECT& work : workAreas) {
        for (const RECT& desired : desiredWindows) {
            assert(within(ClampToWork(desired, work), work));
            assert(within(CenterInWork(work, desired), work));
            for (const RECT& destination : workAreas) {
                assert(within(PreserveRelative(work, destination, desired), destination));
            }
        }
    }

    const RECT invalid{0, 0, 0, 0};
    const RECT unchanged = PreserveRelative(invalid, right, original);
    assert(unchanged.left == original.left && unchanged.right == original.right);
    assert(unchanged.top == original.top && unchanged.bottom == original.bottom);

    std::puts("Window Tidy logic tests passed");
    return 0;
}
