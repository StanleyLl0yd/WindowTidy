#include <windows.h>
#include <shellapi.h>
#include <commctrl.h>
#include <dwmapi.h>

#include <algorithm>
#include <array>
#include <cwchar>
#include <string>
#include <utility>
#include <vector>

#include "windowtidy/geometry.hpp"
#include "windowtidy/hotkeys.hpp"
#include "windowtidy/localization.hpp"
#include "windowtidy/menu_title.hpp"
#include "windowtidy/tray_target.hpp"
#include "windowtidy/window_diagnostics.hpp"
#include "../res/resource.h"

namespace {
constexpr wchar_t kAppName[] = L"Window Tidy";
constexpr wchar_t kWindowClass[] = L"WindowTidyWindow_38132E6B";
constexpr wchar_t kMutexName[] = L"Local\\WindowTidy_6C0E39A0_75A3_4B67_9F26_0E21B8862EEA";
constexpr wchar_t kConfigKey[] = L"Software\\WindowTidy";
constexpr wchar_t kRunKey[] = L"Software\\Microsoft\\Windows\\CurrentVersion\\Run";
constexpr wchar_t kRunValue[] = L"WindowTidy";

enum class MoveMode : DWORD { Preserve = 0, Center = 1, Maximize = 2 };
enum HotkeyIndex : size_t { Tidy = 0, Topmost = 1, Next = 2, Previous = 3, Minimize = 4 };
constexpr size_t kActionCount = 5;
constexpr int kHotkeyBaseId = 100;
constexpr UINT_PTR kTrayRetryTimer = 1;
constexpr size_t kMaxTrayTargets = 24;
constexpr size_t kMaxDiagnosticsWindows = 24;
static_assert(ID_TRAY_TARGET_BASE + kMaxTrayTargets * kActionCount < 0x10000);

struct Settings {
    bool skipTopmost = true;
    bool onlyCurrentMonitor = false;
    MoveMode moveMode = MoveMode::Preserve;
    std::array<wt::Hotkey, kActionCount> shortcuts{{
        {MOD_CONTROL | MOD_ALT, 'Z'},
        {MOD_CONTROL | MOD_ALT, 'T'},
        {MOD_CONTROL | MOD_ALT, VK_RIGHT},
        {MOD_CONTROL | MOD_ALT, VK_LEFT},
        {MOD_ALT, 'H'}
    }};
};

struct TidyEntry {
    wt::WindowIdentity identity{};
    bool maximized{};
};
struct TidyContext {
    HWND exemptRoot{};
    HMONITOR currentMonitor{};
    const Settings& settings;
};
struct Monitor {
    HMONITOR handle{};
    RECT screen{};
    RECT work{};
};
struct TrayTarget {
    wt::WindowIdentity identity{};
    std::wstring title;
};

HINSTANCE g_instance{};
HWND g_window{};
HANDLE g_mutex{};
UINT g_taskbarCreated{};
HICON g_smallIcon{};
NOTIFYICONDATAW g_tray{};
bool g_trayAdded = false;
bool g_settingsDialogOpen = false;
wt::WindowIdentity g_trayTarget{};
Settings g_settings{};
wt::Language g_language = wt::Language::English;
std::array<bool, kActionCount> g_hotkeyActive{};
std::array<DWORD, kActionCount> g_hotkeyErrors{};
std::array<unsigned, kActionCount> g_hotkeyMessages{};
std::array<bool, kActionCount> g_hotkeyLastTargetValid{};
std::vector<TidyEntry> g_tidied;
std::vector<wt::WindowIdentity> g_lastTidyFailures; // Handles only; window titles are not cached.
wt::WindowIdentity g_lastMinimizeFailure{};
struct MoveFailure {
    wt::WindowIdentity target{};
    DWORD error{};
    bool targetMonitorNotReached{};
};
MoveFailure g_lastMoveFailure{};

const wchar_t* Tr(wt::TextId id) {
    return wt::Text(id, g_language);
}

constexpr std::array<int, kActionCount> kHotkeyControls{
    IDC_HK_TIDY, IDC_HK_TOPMOST, IDC_HK_NEXT, IDC_HK_PREV, IDC_HK_MINIMIZE
};
constexpr std::array<wt::TextId, kActionCount> kHotkeyNames{
    wt::TextId::ActionTidy, wt::TextId::ActionTopmost, wt::TextId::ActionNext,
    wt::TextId::ActionPrevious, wt::TextId::ActionMinimize
};
constexpr std::array<const wchar_t*, kActionCount> kHotkeyValues{
    L"Tidy", L"Topmost", L"MoveNext", L"MovePrev", L"MinimizeActive"
};

void Warn(const std::wstring& message, HWND owner = nullptr) {
    MessageBoxW(owner, message.c_str(), kAppName, MB_OK | MB_ICONWARNING);
}

DWORD ReadDword(const wchar_t* subkey, const wchar_t* name, DWORD fallback) {
    DWORD value = fallback;
    DWORD bytes = sizeof(value);
    std::wstring key = std::wstring(kConfigKey) + L"\\" + subkey;
    const LONG rc = RegGetValueW(HKEY_CURRENT_USER, key.c_str(), name,
                                  RRF_RT_REG_DWORD, nullptr, &value, &bytes);
    return rc == ERROR_SUCCESS ? value : fallback;
}

bool WriteDword(const wchar_t* subkey, const wchar_t* name, DWORD value) {
    const std::wstring key = std::wstring(kConfigKey) + L"\\" + subkey;
    HKEY handle{};
    if (RegCreateKeyExW(HKEY_CURRENT_USER, key.c_str(), 0, nullptr, 0,
                        KEY_SET_VALUE, nullptr, &handle, nullptr) != ERROR_SUCCESS) return false;
    const LONG rc = RegSetValueExW(handle, name, 0, REG_DWORD,
                                  reinterpret_cast<const BYTE*>(&value), sizeof(value));
    RegCloseKey(handle);
    return rc == ERROR_SUCCESS;
}

Settings LoadSettings() {
    Settings settings;
    settings.skipTopmost = ReadDword(L"Tidy", L"SkipTopmost", 1) != 0;
    settings.onlyCurrentMonitor = ReadDword(L"Tidy", L"OnlyCurrentMonitor", 0) != 0;
    const DWORD rawMode = ReadDword(L"Move", L"Mode", 0);
    settings.moveMode = rawMode <= 2 ? static_cast<MoveMode>(rawMode) : MoveMode::Preserve;
    for (size_t i = 0; i < kActionCount; ++i) {
        const DWORD raw = ReadDword(L"Hotkeys", kHotkeyValues[i], wt::PackHotkey(settings.shortcuts[i]));
        const wt::Hotkey hotkey = wt::UnpackHotkey(raw);
        if (wt::ValidHotkey(hotkey)) settings.shortcuts[i] = hotkey;
    }
    return settings;
}

bool SaveSettings(const Settings& settings) {
    bool success = WriteDword(L"Tidy", L"SkipTopmost", settings.skipTopmost ? 1 : 0);
    success = WriteDword(L"Tidy", L"OnlyCurrentMonitor", settings.onlyCurrentMonitor ? 1 : 0) && success;
    success = WriteDword(L"Move", L"Mode", static_cast<DWORD>(settings.moveMode)) && success;
    for (size_t i = 0; i < kActionCount; ++i) {
        success = WriteDword(L"Hotkeys", kHotkeyValues[i], wt::PackHotkey(settings.shortcuts[i])) && success;
    }
    return success;
}

std::wstring ExecutablePath() {
    std::wstring buffer(260, L'\0');
    for (;;) {
        const DWORD n = GetModuleFileNameW(nullptr, buffer.data(), static_cast<DWORD>(buffer.size()));
        if (n == 0) return {};
        if (static_cast<size_t>(n) < buffer.size() - 1) {
            buffer.resize(n);
            return buffer;
        }
        if (buffer.size() >= 32768) return {};
        buffer.resize(buffer.size() * 2);
    }
}

bool StartupEnabled() {
    DWORD bytes = 0;
    constexpr DWORD typeFlags = RRF_RT_REG_SZ | RRF_RT_REG_EXPAND_SZ | RRF_NOEXPAND;
    if (RegGetValueW(HKEY_CURRENT_USER, kRunKey, kRunValue, typeFlags,
                     nullptr, nullptr, &bytes) != ERROR_SUCCESS ||
        bytes < sizeof(wchar_t) || bytes > 65536) return false;

    std::vector<wchar_t> value(bytes / sizeof(wchar_t) + 1, L'\0');
    return RegGetValueW(HKEY_CURRENT_USER, kRunKey, kRunValue, typeFlags,
                        nullptr, value.data(), &bytes) == ERROR_SUCCESS &&
           value.front() != L'\0';
}

bool SetStartup(bool enabled) {
    HKEY key{};
    if (RegCreateKeyExW(HKEY_CURRENT_USER, kRunKey, 0, nullptr, 0,
                        KEY_SET_VALUE, nullptr, &key, nullptr) != ERROR_SUCCESS) return false;
    LONG rc = ERROR_SUCCESS;
    if (enabled) {
        const std::wstring path = ExecutablePath();
        if (path.empty()) {
            RegCloseKey(key);
            return false;
        }
        const std::wstring command = L"\"" + path + L"\"";
        rc = RegSetValueExW(key, kRunValue, 0, REG_SZ,
                            reinterpret_cast<const BYTE*>(command.c_str()),
                            static_cast<DWORD>((command.size() + 1) * sizeof(wchar_t)));
    } else {
        rc = RegDeleteValueW(key, kRunValue);
        if (rc == ERROR_FILE_NOT_FOUND) rc = ERROR_SUCCESS;
    }
    RegCloseKey(key);
    return rc == ERROR_SUCCESS;
}

bool IsShellWindow(HWND window) {
    wchar_t name[96]{};
    return GetClassNameW(window, name, static_cast<int>(std::size(name))) == 0 ||
           wt::IsShellClass(name);
}

bool IsCloaked(HWND window) {
    DWORD cloaked = 0;
    return SUCCEEDED(DwmGetWindowAttribute(window, DWMWA_CLOAKED, &cloaked, sizeof(cloaked))) &&
           cloaked != 0;
}

bool EligibleWindow(HWND window) {
    if (!window || !IsWindow(window) || !IsWindowVisible(window) ||
        window == g_window || IsShellWindow(window) || IsCloaked(window)) return false;
    DWORD pid = 0;
    GetWindowThreadProcessId(window, &pid);
    return pid != 0 && pid != GetCurrentProcessId();
}

HWND ActiveWindow() {
    HWND window = GetForegroundWindow();
    if (!EligibleWindow(window)) return nullptr;
    return window;
}

bool RegisterHotkeys(const Settings& settings, size_t* badIndex = nullptr, bool keepSuccessful = false) {
    bool allRegistered = true;
    for (size_t i = 0; i < kActionCount; ++i) {
        g_hotkeyActive[i] = false;
        g_hotkeyErrors[i] = ERROR_SUCCESS;
        if (settings.shortcuts[i].vk == 0) continue;
        const auto hotkey = settings.shortcuts[i];
        if (RegisterHotKey(g_window, kHotkeyBaseId + static_cast<int>(i),
                           hotkey.mods | MOD_NOREPEAT, hotkey.vk)) {
            g_hotkeyActive[i] = true;
        } else {
            g_hotkeyErrors[i] = GetLastError();
            if (allRegistered && badIndex) *badIndex = i;
            allRegistered = false;
        }
    }
    if (!allRegistered && !keepSuccessful) {
        // Leave all shortcuts unregistered while the settings dialog remains open.
        // Preserve each Win32 error for the warning instead of obscuring later failures.
        for (size_t i = 0; i < kActionCount; ++i) {
            if (g_hotkeyActive[i]) UnregisterHotKey(g_window, kHotkeyBaseId + static_cast<int>(i));
            g_hotkeyActive[i] = false;
        }
    }
    return allRegistered;
}

void UnregisterHotkeys() {
    for (size_t i = 0; i < kActionCount; ++i) {
        if (g_hotkeyActive[i]) UnregisterHotKey(g_window, kHotkeyBaseId + static_cast<int>(i));
        g_hotkeyActive[i] = false;
        g_hotkeyErrors[i] = 0;
    }
}

void ReportHotkeyFailure(size_t badIndex, HWND owner = nullptr) {
    std::wstring report = Tr(wt::TextId::WarnRegistration);
    report += Tr(kHotkeyNames[badIndex]);
    if (g_hotkeyErrors[badIndex] != ERROR_SUCCESS) {
        report += L" (Win32: " + std::to_wstring(g_hotkeyErrors[badIndex]) + L")";
    }
    report += Tr(wt::TextId::WarnRegistrationDetail);
    Warn(report, owner);
}

BOOL CALLBACK EnumerateTidy(HWND window, LPARAM param) {
    const auto* context = reinterpret_cast<const TidyContext*>(param);
    if (!EligibleWindow(window) || IsIconic(window)) return TRUE;
    const LONG_PTR style = GetWindowLongPtrW(window, GWL_STYLE);
    const LONG_PTR extended = GetWindowLongPtrW(window, GWL_EXSTYLE);
    if (wt::ClassifyTidyStyle(style, extended, GetWindow(window, GW_OWNER) != nullptr,
                              context->settings.skipTopmost) != wt::TidyStyleReason::Eligible) return TRUE;
    if (GetAncestor(window, GA_ROOTOWNER) == context->exemptRoot) return TRUE;
    if (context->settings.onlyCurrentMonitor &&
        MonitorFromWindow(window, MONITOR_DEFAULTTONEAREST) != context->currentMonitor) return TRUE;

    const auto identity = wt::CaptureWindowIdentity(window);
    if (!identity.hwnd) return TRUE;
    const bool wasMaximized = IsZoomed(window) != FALSE;
    ShowWindow(window, SW_MINIMIZE);
    // A closed/reused HWND must not become a restore target or a failure record.
    if (!wt::MatchesWindowIdentity(identity)) return TRUE;
    if (IsIconic(window)) {
        g_tidied.push_back(TidyEntry{identity, wasMaximized});
    } else if (g_lastTidyFailures.size() < kMaxDiagnosticsWindows) {
        g_lastTidyFailures.push_back(identity);
    }
    return TRUE;
}

void DoTidy(HWND target) {
    if (!EligibleWindow(target)) return;
    const HWND root = GetAncestor(target, GA_ROOTOWNER);
    g_tidied.clear();
    g_lastTidyFailures.clear();
    const TidyContext context{root, MonitorFromWindow(target, MONITOR_DEFAULTTONEAREST), g_settings};
    EnumWindows(EnumerateTidy, reinterpret_cast<LPARAM>(&context));
}

void UndoTidy() {
    for (const auto& entry : g_tidied) {
        if (!wt::MatchesWindowIdentity(entry.identity) || !IsIconic(entry.identity.hwnd)) continue;
        ShowWindow(entry.identity.hwnd, entry.maximized ? SW_SHOWMAXIMIZED : SW_RESTORE);
    }
    g_tidied.clear();
}

void ToggleTidy(HWND target) {
    if (!g_tidied.empty()) UndoTidy();
    else DoTidy(target);
}

void ToggleTopmost(HWND target) {
    if (!EligibleWindow(target)) return;
    const bool isTopmost = (GetWindowLongPtrW(target, GWL_EXSTYLE) & WS_EX_TOPMOST) != 0;
    SetWindowPos(target, isTopmost ? HWND_NOTOPMOST : HWND_TOPMOST, 0, 0, 0, 0,
                 SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE | SWP_NOOWNERZORDER);
}

void MinimizeWindow(HWND target) {
    g_lastMinimizeFailure = {};
    if (!EligibleWindow(target) || IsIconic(target)) return;
    if ((GetWindowLongPtrW(target, GWL_STYLE) & WS_MINIMIZEBOX) == 0) return;
    ShowWindow(target, SW_MINIMIZE);
    if (!IsIconic(target)) g_lastMinimizeFailure = wt::CaptureWindowIdentity(target);
}

BOOL CALLBACK EnumerateMonitors(HMONITOR handle, HDC, LPRECT, LPARAM param) {
    auto* result = reinterpret_cast<std::vector<Monitor>*>(param);
    MONITORINFO info{};
    info.cbSize = sizeof(info);
    if (GetMonitorInfoW(handle, &info)) {
        result->push_back(Monitor{handle, info.rcMonitor, info.rcWork});
    }
    return TRUE;
}

void MoveWindowToMonitor(HWND target, int direction) {
    g_lastMoveFailure = {};
    if (!EligibleWindow(target) || IsIconic(target)) return;
    const LONG_PTR style = GetWindowLongPtrW(target, GWL_STYLE);
    if (!wt::CanMoveByStyle(style)) return;
    std::vector<Monitor> monitors;
    EnumDisplayMonitors(nullptr, nullptr, EnumerateMonitors, reinterpret_cast<LPARAM>(&monitors));
    if (monitors.size() < 2) return;
    std::sort(monitors.begin(), monitors.end(), [](const Monitor& a, const Monitor& b) {
        if (a.screen.left != b.screen.left) return a.screen.left < b.screen.left;
        return a.screen.top < b.screen.top;
    });

    const HMONITOR source = MonitorFromWindow(target, MONITOR_DEFAULTTONEAREST);
    const auto it = std::find_if(monitors.begin(), monitors.end(),
                                 [source](const Monitor& m) { return m.handle == source; });
    if (it == monitors.end()) return;
    const size_t from = static_cast<size_t>(it - monitors.begin());
    const size_t to = direction > 0 ? (from + 1) % monitors.size() :
                                 (from + monitors.size() - 1) % monitors.size();

    const bool maximized = IsZoomed(target) != FALSE;
    // Restoring before GetWindowRect returns screen coordinates even for maximized windows.
    // GetWindowPlacement::rcNormalPosition is in workspace coordinates and cannot safely be mixed with monitor rects.
    if (maximized) ShowWindow(target, SW_RESTORE);
    RECT current{};
    if (!GetWindowRect(target, &current) || wt::Width(current) <= 0 || wt::Height(current) <= 0) {
        if (maximized) ShowWindow(target, SW_MAXIMIZE);
        return;
    }
    RECT desired{};
    switch (g_settings.moveMode) {
    case MoveMode::Center:
    case MoveMode::Maximize:
        desired = wt::CenterInWork(monitors[to].work, current);
        break;
    case MoveMode::Preserve:
    default:
        desired = wt::PreserveRelative(monitors[from].work, monitors[to].work, current);
        break;
    }
    const BOOL moved = SetWindowPos(target, nullptr, desired.left, desired.top,
                                    wt::Width(desired), wt::Height(desired),
                                    SWP_NOZORDER | SWP_NOACTIVATE | SWP_NOOWNERZORDER);
    const DWORD error = moved ? ERROR_SUCCESS : GetLastError();
    if (maximized || (moved && g_settings.moveMode == MoveMode::Maximize)) {
        ShowWindow(target, SW_MAXIMIZE);
    }
    if (!moved || MonitorFromWindow(target, MONITOR_DEFAULTTONEAREST) != monitors[to].handle) {
        g_lastMoveFailure = MoveFailure{
            wt::CaptureWindowIdentity(target), error, moved != FALSE
        };
    }
}

bool AddTray() {
    g_tray = {};
    g_tray.cbSize = sizeof(g_tray);
    g_tray.hWnd = g_window;
    g_tray.uID = 1;
    g_tray.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
    g_tray.uCallbackMessage = WM_TRAYICON;
    g_tray.hIcon = g_smallIcon;
    wcscpy_s(g_tray.szTip, L"Window Tidy");
    g_trayAdded = Shell_NotifyIconW(NIM_ADD, &g_tray) != FALSE ||
                  Shell_NotifyIconW(NIM_MODIFY, &g_tray) != FALSE;
    return g_trayAdded;
}

void EnsureTray() {
    if (AddTray()) KillTimer(g_window, kTrayRetryTimer);
    else SetTimer(g_window, kTrayRetryTimer, 2000, nullptr);
}

void RemoveTray() {
    KillTimer(g_window, kTrayRetryTimer);
    if (g_trayAdded) Shell_NotifyIconW(NIM_DELETE, &g_tray);
    g_trayAdded = false;
}

void ShowSettings();
void DispatchAction(HotkeyIndex action, HWND target);

BOOL CALLBACK CollectTrayTargets(HWND window, LPARAM context) {
    auto& targets = *reinterpret_cast<std::vector<TrayTarget>*>(context);
    if (!EligibleWindow(window) || IsIconic(window)) return TRUE;
    wchar_t name[160]{};
    const int length = GetWindowTextW(window, name, static_cast<int>(std::size(name)));
    if (length <= 0) return TRUE;
    const auto identity = wt::CaptureWindowIdentity(window);
    if (!identity.hwnd) return TRUE;
    targets.push_back(TrayTarget{identity, std::wstring(name, static_cast<size_t>(length))});
    return targets.size() < kMaxTrayTargets;
}

std::wstring HotkeyName(wt::Hotkey shortcut) {
    std::wstring result;
    if (shortcut.mods & MOD_CONTROL) result += L"Ctrl+";
    if (shortcut.mods & MOD_ALT) result += L"Alt+";
    if (shortcut.mods & MOD_SHIFT) result += L"Shift+";
    if (shortcut.vk >= 'A' && shortcut.vk <= 'Z') {
        result += static_cast<wchar_t>(shortcut.vk);
    } else if (shortcut.vk == VK_LEFT) {
        result += L"Left";
    } else if (shortcut.vk == VK_RIGHT) {
        result += L"Right";
    } else if (shortcut.vk == VK_UP) {
        result += L"Up";
    } else if (shortcut.vk == VK_DOWN) {
        result += L"Down";
    } else if (shortcut.vk >= VK_NUMPAD0 && shortcut.vk <= VK_NUMPAD9) {
        result += L"Num " + std::to_wstring(shortcut.vk - VK_NUMPAD0);
    } else {
        wchar_t name[64]{};
        const UINT scan = MapVirtualKeyW(shortcut.vk, MAPVK_VK_TO_VSC_EX);
        const LONG keyData = static_cast<LONG>(((scan & 0xffu) << 16u) |
                                               ((scan & 0xff00u) ? (1u << 24u) : 0u));
        if (GetKeyNameTextW(keyData, name, static_cast<int>(std::size(name))) > 0) {
            result += name;
        } else {
            result += L"VK " + std::to_wstring(shortcut.vk);
        }
    }
    return result;
}

void ShowHotkeyDiagnostics() {
    std::wstring report = Tr(wt::TextId::DiagnosticIntro);
    for (size_t i = 0; i < kActionCount; ++i) {
        const auto shortcut = g_settings.shortcuts[i];
        report += L"\n";
        report += Tr(kHotkeyNames[i]);
        report += L": ";
        if (shortcut.vk == 0) {
            report += Tr(wt::TextId::DiagnosticDisabled);
        } else {
            report += HotkeyName(shortcut);
            report += L" (";
            report += Tr(wt::TextId::DiagnosticVirtualKey);
            report += L"=" + std::to_wstring(shortcut.vk) + L") - ";
            const wt::TextId state = g_settingsDialogOpen ? wt::TextId::DiagnosticSuspended :
                                     g_hotkeyActive[i] ? wt::TextId::DiagnosticActive :
                                     g_hotkeyErrors[i] != ERROR_SUCCESS ? wt::TextId::DiagnosticUnavailable :
                                     wt::TextId::DiagnosticNotRegistered;
            report += Tr(state);
            if (g_hotkeyErrors[i] != ERROR_SUCCESS && !g_hotkeyActive[i]) {
                report += L" (Win32: " + std::to_wstring(g_hotkeyErrors[i]) + L")";
            }
            report += L" [";
            report += Tr(wt::TextId::DiagnosticMessages);
            report += L" " + std::to_wstring(g_hotkeyMessages[i]);
            if (g_hotkeyMessages[i] != 0) {
                report += L", ";
                report += Tr(g_hotkeyLastTargetValid[i] ? wt::TextId::DiagnosticTargetValid :
                             wt::TextId::DiagnosticTargetInvalid);
            }
            report += L"]";
        }
    }
    report += L"\n";
    report += Tr(wt::TextId::DiagnosticAdvice);
    MessageBoxW(g_window, report.c_str(), Tr(wt::TextId::DiagnosticTitle), MB_OK | MB_ICONINFORMATION);
}

const wchar_t* TidyStyleExplanation(wt::TidyStyleReason reason) {
    switch (reason) {
    case wt::TidyStyleReason::NoMinimizeButton: return Tr(wt::TextId::WindowDiagNoMinButton);
    case wt::TidyStyleReason::ToolWindow: return Tr(wt::TextId::WindowDiagToolWindow);
    case wt::TidyStyleReason::OwnedWindow: return Tr(wt::TextId::WindowDiagOwnedWindow);
    case wt::TidyStyleReason::ExcludedTopmost: return Tr(wt::TextId::WindowDiagTopmostExcluded);
    case wt::TidyStyleReason::Eligible: return Tr(wt::TextId::WindowDiagReady);
    }
    return Tr(wt::TextId::WindowDiagReady);
}

struct WindowDiagnosticScan {
    std::wstring report;
    size_t listed{};
    bool truncated{};
    bool redactTitles{};
};

BOOL CALLBACK EnumerateWindowDiagnostics(HWND window, LPARAM param) {
    auto& scan = *reinterpret_cast<WindowDiagnosticScan*>(param);
    if (!EligibleWindow(window)) return TRUE;
    wchar_t name[160]{};
    const int length = GetWindowTextW(window, name, static_cast<int>(std::size(name)));
    if (length <= 0) return TRUE;
    if (scan.listed == kMaxDiagnosticsWindows) {
        scan.truncated = true;
        return FALSE;
    }

    const LONG_PTR style = GetWindowLongPtrW(window, GWL_STYLE);
    const LONG_PTR extended = GetWindowLongPtrW(window, GWL_EXSTYLE);
    const bool minimized = IsIconic(window) != FALSE;
    wchar_t className[128]{};
    const bool classKnown = GetClassNameW(window, className, static_cast<int>(std::size(className))) > 0;
    const auto reason = wt::ClassifyTidyStyle(style, extended,
                          GetWindow(window, GW_OWNER) != nullptr, g_settings.skipTopmost);
    const bool attemptedAndFailed = std::any_of(g_lastTidyFailures.begin(), g_lastTidyFailures.end(),
        [window](const wt::WindowIdentity& id) {
            return id.hwnd == window && wt::MatchesWindowIdentity(id);
        });

    scan.report += std::to_wstring(++scan.listed) + L". ";
    if (scan.redactTitles) {
        scan.report += Tr(wt::TextId::WindowDiagHiddenTitle);
    } else {
        std::wstring title(name, static_cast<size_t>(length));
        for (auto& character : title) {
            if (character < L' ') character = L' ';
        }
        scan.report += title;
    }
    scan.report += L"\r\n";
    scan.report += Tr(wt::TextId::WindowDiagTidy);
    scan.report += L": ";
    scan.report += attemptedAndFailed ? Tr(wt::TextId::WindowDiagAttemptFailed) :
                   minimized ? Tr(wt::TextId::WindowDiagAlreadyMinimized) :
                   TidyStyleExplanation(reason);
    scan.report += L"; ";
    scan.report += Tr(wt::TextId::WindowDiagMove);
    scan.report += L": ";
    scan.report += minimized ? Tr(wt::TextId::WindowDiagAlreadyMinimized) :
                   wt::CanMoveByStyle(style) ? Tr(wt::TextId::WindowDiagReady) :
                   Tr(wt::TextId::WindowDiagMoveNoCaption);
    scan.report += L"\r\n";
    scan.report += Tr(wt::TextId::WindowDiagClass);
    scan.report += L": ";
    scan.report += classKnown ? className : L"?";
    if (g_lastMinimizeFailure.hwnd == window &&
        wt::MatchesWindowIdentity(g_lastMinimizeFailure) && !minimized) {
        scan.report += L"\r\n";
        scan.report += Tr(wt::TextId::WindowDiagMinimizeFailed);
    }
    if (g_lastMoveFailure.target.hwnd == window &&
        wt::MatchesWindowIdentity(g_lastMoveFailure.target)) {
        scan.report += L"\r\n";
        scan.report += Tr(g_lastMoveFailure.targetMonitorNotReached
            ? wt::TextId::WindowDiagMoveDidNotReach : wt::TextId::WindowDiagMoveRejected);
        if (g_lastMoveFailure.error != ERROR_SUCCESS) {
            scan.report += L" (Win32: " + std::to_wstring(g_lastMoveFailure.error) + L")";
            if (g_lastMoveFailure.error == ERROR_ACCESS_DENIED) {
                scan.report += L" - ";
                scan.report += Tr(wt::TextId::WindowDiagAccessDenied);
            }
        }
    }
    scan.report += L"\r\n\r\n";
    return TRUE;
}

std::wstring BuildWindowDiagnostics(bool redactTitles = false) {
    WindowDiagnosticScan scan;
    scan.redactTitles = redactTitles;
    scan.report = Tr(wt::TextId::WindowDiagIntro);
    EnumWindows(EnumerateWindowDiagnostics, reinterpret_cast<LPARAM>(&scan));
    if (!scan.listed) scan.report += Tr(wt::TextId::WindowDiagNoWindows);
    if (scan.truncated) scan.report += Tr(wt::TextId::WindowDiagTruncated);
    scan.report += Tr(wt::TextId::WindowDiagFooter);
    return scan.report;
}

bool CopyPrivateWindowReport(HWND owner) {
    // Never put window titles on the clipboard through the dedicated Copy button.
    const std::wstring report = BuildWindowDiagnostics(true);
    if (!OpenClipboard(owner)) return false;
    const SIZE_T bytes = (report.size() + 1) * sizeof(wchar_t);
    HGLOBAL memory = GlobalAlloc(GMEM_MOVEABLE, bytes);
    bool copied = false;
    if (memory) {
        auto* destination = static_cast<wchar_t*>(GlobalLock(memory));
        if (destination) {
            std::copy(report.begin(), report.end(), destination);
            destination[report.size()] = 0;
            GlobalUnlock(memory);
            if (EmptyClipboard() && SetClipboardData(CF_UNICODETEXT, memory)) {
                copied = true;
                memory = nullptr; // Clipboard now owns this movable allocation.
            }
        }
        if (memory) GlobalFree(memory);
    }
    CloseClipboard();
    return copied;
}

INT_PTR CALLBACK WindowDiagnosticsProc(HWND dialog, UINT message, WPARAM wparam, LPARAM) {
    if (message == WM_INITDIALOG) {
        SetWindowTextW(dialog, Tr(wt::TextId::WindowDiagTitle));
        SetDlgItemTextW(dialog, IDC_WINDOW_REPORT, BuildWindowDiagnostics().c_str());
        SetDlgItemTextW(dialog, IDOK, Tr(wt::TextId::WindowDiagClose));
        SetDlgItemTextW(dialog, IDC_COPY_REPORT, Tr(wt::TextId::WindowDiagCopy));
        return TRUE;
    }
    if (message == WM_COMMAND) {
        if (LOWORD(wparam) == IDC_COPY_REPORT) {
            if (CopyPrivateWindowReport(dialog)) {
                SetDlgItemTextW(dialog, IDC_COPY_REPORT, Tr(wt::TextId::WindowDiagCopied));
            } else {
                Warn(Tr(wt::TextId::WindowDiagCopyFailed), dialog);
            }
            return TRUE;
        }
        if (LOWORD(wparam) == IDOK || LOWORD(wparam) == IDCANCEL) {
            EndDialog(dialog, LOWORD(wparam));
            return TRUE;
        }
    }
    return FALSE;
}

void ShowWindowDiagnostics() {
    DialogBoxParamW(g_instance, MAKEINTRESOURCEW(IDD_WINDOW_DIAGNOSTICS),
                    g_window, WindowDiagnosticsProc, 0);
}

void TrayMenu() {
    // Snapshot the foreground before our menu receives activation. Hotkeys use live foreground.
    g_trayTarget = wt::CaptureWindowIdentity(ActiveWindow());
    HMENU menu = CreatePopupMenu();
    if (!menu) return;
    AppendMenuW(menu, MF_STRING, ID_TRAY_TIDY, Tr(wt::TextId::TrayTidy));
    AppendMenuW(menu, MF_STRING, ID_TRAY_UNDO, Tr(wt::TextId::TrayUndo));
    AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(menu, MF_STRING, ID_TRAY_TOPMOST, Tr(wt::TextId::TrayTopmost));
    AppendMenuW(menu, MF_STRING, ID_TRAY_NEXT, Tr(wt::TextId::TrayNext));
    AppendMenuW(menu, MF_STRING, ID_TRAY_PREV, Tr(wt::TextId::TrayPrevious));
    AppendMenuW(menu, MF_STRING, ID_TRAY_MINIMIZE, Tr(wt::TextId::TrayMinimize));
    // An explicit choice avoids guessing the foreground window after Explorer
    // takes focus while opening the notification-area menu.
    std::vector<TrayTarget> targets;
    EnumWindows(CollectTrayTargets, reinterpret_cast<LPARAM>(&targets));
    HMENU chooseWindow = CreatePopupMenu();
    if (chooseWindow) {
        constexpr std::array<wt::TextId, kActionCount> actions{{
            wt::TextId::TrayTidy, wt::TextId::TrayTopmost, wt::TextId::TrayNext,
            wt::TextId::TrayPrevious, wt::TextId::TrayMinimize
        }};
        for (size_t i = 0; i < targets.size(); ++i) {
            HMENU actionsMenu = CreatePopupMenu();
            if (!actionsMenu) continue;
            for (size_t action = 0; action < actions.size(); ++action) {
                const UINT id = ID_TRAY_TARGET_BASE + static_cast<UINT>(i * kActionCount + action);
                AppendMenuW(actionsMenu, MF_STRING, id, Tr(actions[action]));
            }
            const std::wstring label = wt::SafeMenuLabel(targets[i].title);
            if (!AppendMenuW(chooseWindow, MF_POPUP,
                             reinterpret_cast<UINT_PTR>(actionsMenu), label.c_str())) {
                DestroyMenu(actionsMenu);
            }
        }
        if (GetMenuItemCount(chooseWindow) == 0) {
            AppendMenuW(chooseWindow, MF_STRING | MF_GRAYED, 0, Tr(wt::TextId::TrayNoTargets));
        }
        if (!AppendMenuW(menu, MF_POPUP, reinterpret_cast<UINT_PTR>(chooseWindow),
                         Tr(wt::TextId::TrayChooseTarget))) {
            DestroyMenu(chooseWindow);
        }
    }
    AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(menu, MF_STRING, ID_TRAY_SETTINGS, Tr(wt::TextId::TraySettings));
    AppendMenuW(menu, MF_STRING, ID_TRAY_DIAGNOSTICS, Tr(wt::TextId::TrayDiagnostics));
    AppendMenuW(menu, MF_STRING, ID_TRAY_WINDOW_DIAGNOSTICS, Tr(wt::TextId::TrayWindowDiagnostics));
    AppendMenuW(menu, MF_STRING | (StartupEnabled() ? MF_CHECKED : 0),
                ID_TRAY_STARTUP, Tr(wt::TextId::TrayStartup));
    AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(menu, MF_STRING, ID_TRAY_EXIT, Tr(wt::TextId::TrayExit));
    if (g_tidied.empty()) EnableMenuItem(menu, ID_TRAY_UNDO, MF_BYCOMMAND | MF_GRAYED);
    if (!g_trayTarget.hwnd) {
        for (const UINT command : {ID_TRAY_TIDY, ID_TRAY_TOPMOST, ID_TRAY_NEXT,
                                   ID_TRAY_PREV, ID_TRAY_MINIMIZE}) {
            EnableMenuItem(menu, command, MF_BYCOMMAND | MF_GRAYED);
        }
    }
    POINT cursor{};
    if (GetCursorPos(&cursor)) {
        SetForegroundWindow(g_window);
        const UINT chosen = TrackPopupMenu(menu, TPM_RETURNCMD | TPM_RIGHTBUTTON,
                                            cursor.x, cursor.y, 0, g_window, nullptr);
        const UINT targetEnd = ID_TRAY_TARGET_BASE + static_cast<UINT>(targets.size() * kActionCount);
        if (chosen >= ID_TRAY_TARGET_BASE && chosen < targetEnd) {
            const UINT offset = chosen - ID_TRAY_TARGET_BASE;
            const auto& selected = targets[offset / kActionCount];
            if (wt::MatchesWindowIdentity(selected.identity) && EligibleWindow(selected.identity.hwnd)) {
                DispatchAction(static_cast<HotkeyIndex>(offset % kActionCount), selected.identity.hwnd);
            }
        } else if (chosen != 0) {
            SendMessageW(g_window, WM_COMMAND, chosen, 0);
        }
        PostMessageW(g_window, WM_NULL, 0, 0);
    }
    DestroyMenu(menu);
    g_trayTarget = {};
}

HWND VerifiedTrayTarget() {
    return wt::MatchesWindowIdentity(g_trayTarget) && EligibleWindow(g_trayTarget.hwnd)
        ? g_trayTarget.hwnd : nullptr;
}

void FillHotkeyControl(HWND dialog, int id, wt::Hotkey hotkey) {
    const UINT flags = ((hotkey.mods & MOD_ALT) ? HOTKEYF_ALT : 0u) |
                       ((hotkey.mods & MOD_CONTROL) ? HOTKEYF_CONTROL : 0u) |
                       ((hotkey.mods & MOD_SHIFT) ? HOTKEYF_SHIFT : 0u);
    const WORD value = MAKEWORD(static_cast<BYTE>(hotkey.vk), static_cast<BYTE>(flags));
    SendDlgItemMessageW(dialog, id, HKM_SETHOTKEY, value, 0);
}

wt::Hotkey ReadHotkeyControl(HWND dialog, int id) {
    const WORD value = static_cast<WORD>(SendDlgItemMessageW(dialog, id, HKM_GETHOTKEY, 0, 0));
    const UINT flags = HIBYTE(value);
    const UINT mods = ((flags & HOTKEYF_ALT) ? MOD_ALT : 0u) |
                      ((flags & HOTKEYF_CONTROL) ? MOD_CONTROL : 0u) |
                      ((flags & HOTKEYF_SHIFT) ? MOD_SHIFT : 0u);
    return {mods, LOBYTE(value)};
}

INT_PTR CALLBACK SettingsProc(HWND dialog, UINT message, WPARAM wparam, LPARAM) {
    switch (message) {
    case WM_INITDIALOG: {
        SetWindowTextW(dialog, Tr(wt::TextId::SettingsTitle));
        constexpr std::array<std::pair<int, wt::TextId>, 14> labels{{
            {IDC_TXT_GLOBAL, wt::TextId::GlobalHotkeys},
            {IDC_TXT_TIDY, wt::TextId::TidyLabel},
            {IDC_TXT_TOPMOST, wt::TextId::TopmostLabel},
            {IDC_TXT_NEXT, wt::TextId::NextLabel},
            {IDC_TXT_PREV, wt::TextId::PreviousLabel},
            {IDC_TXT_MINIMIZE, wt::TextId::MinimizeLabel},
            {IDC_TXT_BEHAVIOR, wt::TextId::Behavior},
            {IDC_SKIP_TOPMOST, wt::TextId::SkipTopmost},
            {IDC_ONLY_CURRENT, wt::TextId::OnlyCurrent},
            {IDC_TXT_MOVE_MODE, wt::TextId::MoveModeLabel},
            {IDC_RUN_AT_STARTUP, wt::TextId::RunAtStartup},
            {IDC_HOTKEY_HELP, wt::TextId::HotkeyHelp},
            {IDOK, wt::TextId::Save},
            {IDCANCEL, wt::TextId::Cancel}
        }};
        for (const auto& [control, text] : labels) {
            SetDlgItemTextW(dialog, control, Tr(text));
        }
        CheckDlgButton(dialog, IDC_SKIP_TOPMOST, g_settings.skipTopmost ? BST_CHECKED : BST_UNCHECKED);
        CheckDlgButton(dialog, IDC_ONLY_CURRENT, g_settings.onlyCurrentMonitor ? BST_CHECKED : BST_UNCHECKED);
        HWND combo = GetDlgItem(dialog, IDC_MOVE_MODE);
        SendMessageW(combo, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(Tr(wt::TextId::Preserve)));
        SendMessageW(combo, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(Tr(wt::TextId::Center)));
        SendMessageW(combo, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(Tr(wt::TextId::Maximize)));
        SendMessageW(combo, CB_SETCURSEL, static_cast<WPARAM>(g_settings.moveMode), 0);
        CheckDlgButton(dialog, IDC_RUN_AT_STARTUP, StartupEnabled() ? BST_CHECKED : BST_UNCHECKED);
        for (size_t i = 0; i < kActionCount; ++i) {
            FillHotkeyControl(dialog, kHotkeyControls[i], g_settings.shortcuts[i]);
        }
        return TRUE;
    }
    case WM_COMMAND: {
        const int id = LOWORD(wparam);
        constexpr std::array<int, kActionCount> clearButtons{
            IDC_CLEAR_TIDY, IDC_CLEAR_TOPMOST, IDC_CLEAR_NEXT, IDC_CLEAR_PREV, IDC_CLEAR_MINIMIZE
        };
        const auto clear = std::find(clearButtons.begin(), clearButtons.end(), id);
        if (clear != clearButtons.end()) {
            FillHotkeyControl(dialog, kHotkeyControls[static_cast<size_t>(clear - clearButtons.begin())], {});
            return TRUE;
        }
        if (id == IDCANCEL) {
            EndDialog(dialog, IDCANCEL);
            return TRUE;
        }
        if (id == IDOK) {
            Settings draft = g_settings;
            draft.skipTopmost = IsDlgButtonChecked(dialog, IDC_SKIP_TOPMOST) == BST_CHECKED;
            draft.onlyCurrentMonitor = IsDlgButtonChecked(dialog, IDC_ONLY_CURRENT) == BST_CHECKED;
            const LRESULT mode = SendDlgItemMessageW(dialog, IDC_MOVE_MODE, CB_GETCURSEL, 0, 0);
            if (mode >= 0 && mode <= 2) draft.moveMode = static_cast<MoveMode>(mode);
            for (size_t i = 0; i < kActionCount; ++i) {
                draft.shortcuts[i] = ReadHotkeyControl(dialog, kHotkeyControls[i]);
            }
            if (!wt::UniqueHotkeys(draft.shortcuts)) {
                Warn(Tr(wt::TextId::WarnBadHotkey), dialog);
                return TRUE;
            }
            size_t failure = 0;
            if (!RegisterHotkeys(draft, &failure)) {
                ReportHotkeyFailure(failure, dialog);
                return TRUE;
            }
            if (!SaveSettings(draft)) {
                UnregisterHotkeys();
                Warn(Tr(wt::TextId::WarnCannotSave), dialog);
                return TRUE;
            }
            const bool autostart = IsDlgButtonChecked(dialog, IDC_RUN_AT_STARTUP) == BST_CHECKED;
            if (autostart != StartupEnabled() && !SetStartup(autostart)) {
                UnregisterHotkeys();
                Warn(Tr(wt::TextId::WarnCannotAutostart), dialog);
                return TRUE;
            }
            g_settings = draft;
            EndDialog(dialog, IDOK);
            return TRUE;
        }
        break;
    }
    default: break;
    }
    return FALSE;
}

void ShowSettings() {
    if (g_settingsDialogOpen) return;
    g_settingsDialogOpen = true;
    UnregisterHotkeys();
    const INT_PTR result = DialogBoxParamW(g_instance, MAKEINTRESOURCEW(IDD_SETTINGS),
                                           g_window, SettingsProc, 0);
    // Successful Save has already registered the new bindings. Re-register only
    // after Cancel or dialog creation failure; otherwise Windows can retain stale IDs.
    if (result != IDOK) {
        size_t failure = 0;
        if (!RegisterHotkeys(g_settings, &failure, true)) ReportHotkeyFailure(failure);
    }
    g_settingsDialogOpen = false;
}

void DispatchAction(HotkeyIndex action, HWND target) {
    switch (action) {
    case Tidy: ToggleTidy(target); break;
    case Topmost: ToggleTopmost(target); break;
    case Next: MoveWindowToMonitor(target, +1); break;
    case Previous: MoveWindowToMonitor(target, -1); break;
    case Minimize: MinimizeWindow(target); break;
    }
}

LRESULT CALLBACK WindowProc(HWND window, UINT message, WPARAM wparam, LPARAM lparam) {
    if (g_taskbarCreated != 0 && message == g_taskbarCreated) {
        g_trayAdded = false; // explorer.exe restarted, old icon has been discarded
        EnsureTray();
        return 0;
    }
    switch (message) {
    case WM_TIMER:
        if (wparam == kTrayRetryTimer && !g_trayAdded) EnsureTray();
        return 0;
    case WM_TRAYICON:
        if (lparam == WM_RBUTTONUP || lparam == WM_CONTEXTMENU) TrayMenu();
        else if (lparam == WM_LBUTTONDBLCLK) ShowSettings();
        return 0;
    case WM_HOTKEY: {
        const int index = static_cast<int>(wparam) - kHotkeyBaseId;
        if (index >= 0 && index < static_cast<int>(kActionCount)) {
            const HWND target = ActiveWindow();
            ++g_hotkeyMessages[static_cast<size_t>(index)];
            g_hotkeyLastTargetValid[static_cast<size_t>(index)] = target != nullptr;
            DispatchAction(static_cast<HotkeyIndex>(index), target);
        }
        return 0;
    }
    case WM_SHOWSETTINGS:
        ShowSettings();
        return 0;
    case WM_COMMAND:
        switch (LOWORD(wparam)) {
        case ID_TRAY_TIDY:
            if (const HWND target = VerifiedTrayTarget()) DispatchAction(Tidy, target);
            break;
        case ID_TRAY_UNDO: UndoTidy(); break;
        case ID_TRAY_TOPMOST: DispatchAction(Topmost, VerifiedTrayTarget()); break;
        case ID_TRAY_NEXT: DispatchAction(Next, VerifiedTrayTarget()); break;
        case ID_TRAY_PREV: DispatchAction(Previous, VerifiedTrayTarget()); break;
        case ID_TRAY_MINIMIZE: DispatchAction(Minimize, VerifiedTrayTarget()); break;
        case ID_TRAY_SETTINGS: ShowSettings(); break;
        case ID_TRAY_DIAGNOSTICS: ShowHotkeyDiagnostics(); break;
        case ID_TRAY_WINDOW_DIAGNOSTICS: ShowWindowDiagnostics(); break;
        case ID_TRAY_STARTUP:
            if (!SetStartup(!StartupEnabled())) Warn(Tr(wt::TextId::WarnCannotStartup));
            break;
        case ID_TRAY_EXIT: DestroyWindow(window); break;
        default: break;
        }
        return 0;
    case WM_DESTROY:
        UnregisterHotkeys();
        RemoveTray();
        PostQuitMessage(0);
        return 0;
    case WM_CLOSE:
        DestroyWindow(window);
        return 0;
    default: break;
    }
    return DefWindowProcW(window, message, wparam, lparam);
}
} // namespace

int WINAPI wWinMain(_In_ HINSTANCE instance, _In_opt_ HINSTANCE, _In_ PWSTR, _In_ int) {
    g_instance = instance;
    g_language = wt::LanguageForUiLang(GetUserDefaultUILanguage());
    g_mutex = CreateMutexW(nullptr, FALSE, kMutexName);
    if (!g_mutex) return 1;
    if (GetLastError() == ERROR_ALREADY_EXISTS) {
        const HWND first = FindWindowW(kWindowClass, nullptr);
        if (first) PostMessageW(first, WM_SHOWSETTINGS, 0, 0);
        CloseHandle(g_mutex);
        return 0;
    }

    INITCOMMONCONTROLSEX controls{sizeof(controls), ICC_HOTKEY_CLASS | ICC_STANDARD_CLASSES};
    if (!InitCommonControlsEx(&controls)) {
        CloseHandle(g_mutex);
        return 1;
    }

    const HICON largeIcon = static_cast<HICON>(LoadImageW(instance, MAKEINTRESOURCEW(IDI_WINDOWTIDY),
        IMAGE_ICON, GetSystemMetrics(SM_CXICON), GetSystemMetrics(SM_CYICON), LR_SHARED));
    g_smallIcon = static_cast<HICON>(LoadImageW(instance, MAKEINTRESOURCEW(IDI_WINDOWTIDY),
        IMAGE_ICON, GetSystemMetrics(SM_CXSMICON), GetSystemMetrics(SM_CYSMICON), LR_SHARED));
    if (!largeIcon || !g_smallIcon) {
        CloseHandle(g_mutex);
        return 1;
    }

    WNDCLASSEXW cls{};
    cls.cbSize = sizeof(cls);
    cls.hInstance = instance;
    cls.lpfnWndProc = WindowProc;
    cls.lpszClassName = kWindowClass;
    cls.hIcon = largeIcon;
    cls.hIconSm = g_smallIcon;
    cls.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    if (!RegisterClassExW(&cls)) {
        CloseHandle(g_mutex);
        return 1;
    }
    g_settings = LoadSettings();
    g_taskbarCreated = RegisterWindowMessageW(L"TaskbarCreated");
    g_window = CreateWindowExW(WS_EX_TOOLWINDOW, kWindowClass, kAppName,
                               WS_OVERLAPPED, 0, 0, 0, 0,
                               nullptr, nullptr, instance, nullptr);
    if (!g_window) {
        CloseHandle(g_mutex);
        return 1;
    }
    EnsureTray();
    size_t failure = 0;
    if (!RegisterHotkeys(g_settings, &failure, true)) ReportHotkeyFailure(failure);
    MSG message{};
    while (GetMessageW(&message, nullptr, 0, 0) > 0) {
        TranslateMessage(&message);
        DispatchMessageW(&message);
    }
    if (IsWindow(g_window)) DestroyWindow(g_window);
    CloseHandle(g_mutex);
    return 0;
}
