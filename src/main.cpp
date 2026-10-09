#include <windows.h>
#include <shellapi.h>
#include <commctrl.h>
#include <dwmapi.h>

#include <algorithm>
#include <array>
#include <cwchar>
#include <string>
#include <vector>

#include "windowtidy/geometry.hpp"
#include "windowtidy/hotkeys.hpp"
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
    HWND hwnd{};
    DWORD pid{};
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

HINSTANCE g_instance{};
HWND g_window{};
HANDLE g_mutex{};
UINT g_taskbarCreated{};
NOTIFYICONDATAW g_tray{};
bool g_trayAdded = false;
bool g_settingsDialogOpen = false;
HWND g_trayTarget{};
Settings g_settings{};
std::vector<TidyEntry> g_tidied;

constexpr std::array<int, kActionCount> kHotkeyControls{
    IDC_HK_TIDY, IDC_HK_TOPMOST, IDC_HK_NEXT, IDC_HK_PREV, IDC_HK_MINIMIZE
};
constexpr std::array<const wchar_t*, kActionCount> kHotkeyNames{
    L"Tidy", L"Always on top", L"Next monitor", L"Previous monitor", L"Minimize"
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
    if (RegGetValueW(HKEY_CURRENT_USER, kRunKey, kRunValue, RRF_RT_REG_SZ,
                     nullptr, nullptr, &bytes) != ERROR_SUCCESS ||
        bytes < sizeof(wchar_t) || bytes > 65536) return false;

    std::vector<wchar_t> value(bytes / sizeof(wchar_t) + 1, L'\0');
    return RegGetValueW(HKEY_CURRENT_USER, kRunKey, kRunValue, RRF_RT_REG_SZ,
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
    if (!GetClassNameW(window, name, static_cast<int>(std::size(name)))) return true;
    return _wcsicmp(name, L"Shell_TrayWnd") == 0 ||
           _wcsicmp(name, L"Shell_SecondaryTrayWnd") == 0 ||
           _wcsicmp(name, L"Progman") == 0 ||
           _wcsicmp(name, L"WorkerW") == 0 ||
           _wcsicmp(name, L"NotifyIconOverflowWindow") == 0;
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
        if (settings.shortcuts[i].vk == 0) continue;
        const auto hotkey = settings.shortcuts[i];
        if (!RegisterHotKey(g_window, kHotkeyBaseId + static_cast<int>(i),
                            hotkey.mods | MOD_NOREPEAT, hotkey.vk)) {
            if (allRegistered && badIndex) *badIndex = i;
            allRegistered = false;
            if (!keepSuccessful) {
                for (size_t j = 0; j < i; ++j) {
                    UnregisterHotKey(g_window, kHotkeyBaseId + static_cast<int>(j));
                }
                return false;
            }
        }
    }
    return allRegistered;
}

void UnregisterHotkeys() {
    for (size_t i = 0; i < kActionCount; ++i) {
        UnregisterHotKey(g_window, kHotkeyBaseId + static_cast<int>(i));
    }
}

void ReportHotkeyFailure(size_t badIndex, HWND owner = nullptr) {
    Warn(std::wstring(L"Cannot register global shortcut: ") + kHotkeyNames[badIndex] +
         L".\n\nAnother application or Windows may already use this combination.", owner);
}

BOOL CALLBACK EnumerateTidy(HWND window, LPARAM param) {
    const auto* context = reinterpret_cast<const TidyContext*>(param);
    if (!EligibleWindow(window) || IsIconic(window)) return TRUE;
    const LONG_PTR style = GetWindowLongPtrW(window, GWL_STYLE);
    const LONG_PTR extended = GetWindowLongPtrW(window, GWL_EXSTYLE);
    if ((style & WS_MINIMIZEBOX) == 0 || (extended & WS_EX_TOOLWINDOW) != 0 ||
        GetWindow(window, GW_OWNER) != nullptr) return TRUE;
    if (context->settings.skipTopmost && (extended & WS_EX_TOPMOST) != 0) return TRUE;
    if (GetAncestor(window, GA_ROOTOWNER) == context->exemptRoot) return TRUE;
    if (context->settings.onlyCurrentMonitor &&
        MonitorFromWindow(window, MONITOR_DEFAULTTONEAREST) != context->currentMonitor) return TRUE;

    DWORD pid = 0;
    GetWindowThreadProcessId(window, &pid);
    const bool wasMaximized = IsZoomed(window) != FALSE;
    ShowWindow(window, SW_MINIMIZE);
    if (IsIconic(window)) g_tidied.push_back(TidyEntry{window, pid, wasMaximized});
    return TRUE;
}

void DoTidy(HWND target) {
    if (!EligibleWindow(target)) return;
    const HWND root = GetAncestor(target, GA_ROOTOWNER);
    g_tidied.clear();
    const TidyContext context{root, MonitorFromWindow(target, MONITOR_DEFAULTTONEAREST), g_settings};
    EnumWindows(EnumerateTidy, reinterpret_cast<LPARAM>(&context));
}

void UndoTidy() {
    for (const auto& entry : g_tidied) {
        if (!IsWindow(entry.hwnd) || !IsIconic(entry.hwnd)) continue;
        DWORD currentPid = 0;
        GetWindowThreadProcessId(entry.hwnd, &currentPid);
        if (currentPid != entry.pid) continue;
        ShowWindow(entry.hwnd, entry.maximized ? SW_SHOWMAXIMIZED : SW_RESTORE);
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
    if (!EligibleWindow(target) || IsIconic(target)) return;
    if ((GetWindowLongPtrW(target, GWL_STYLE) & WS_MINIMIZEBOX) == 0) return;
    ShowWindow(target, SW_MINIMIZE);
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
    if (!EligibleWindow(target) || IsIconic(target)) return;
    const LONG_PTR style = GetWindowLongPtrW(target, GWL_STYLE);
    if ((style & WS_CAPTION) == 0) return;
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
    if (maximized || (moved && g_settings.moveMode == MoveMode::Maximize)) {
        ShowWindow(target, SW_MAXIMIZE);
    }
}

bool AddTray() {
    g_tray = {};
    g_tray.cbSize = sizeof(g_tray);
    g_tray.hWnd = g_window;
    g_tray.uID = 1;
    g_tray.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
    g_tray.uCallbackMessage = WM_TRAYICON;
    g_tray.hIcon = LoadIconW(nullptr, IDI_APPLICATION);
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

void TrayMenu() {
    // Snapshot the foreground before our menu receives activation. Hotkeys use live foreground.
    g_trayTarget = ActiveWindow();
    HMENU menu = CreatePopupMenu();
    if (!menu) return;
    AppendMenuW(menu, MF_STRING, ID_TRAY_TIDY, L"Tidy / Undo");
    AppendMenuW(menu, MF_STRING, ID_TRAY_UNDO, L"Undo Tidy");
    AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(menu, MF_STRING, ID_TRAY_TOPMOST, L"Toggle always on top");
    AppendMenuW(menu, MF_STRING, ID_TRAY_NEXT, L"Move to next monitor");
    AppendMenuW(menu, MF_STRING, ID_TRAY_PREV, L"Move to previous monitor");
    AppendMenuW(menu, MF_STRING, ID_TRAY_MINIMIZE, L"Minimize active window");
    AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(menu, MF_STRING, ID_TRAY_SETTINGS, L"Settings...");
    AppendMenuW(menu, MF_STRING | (StartupEnabled() ? MF_CHECKED : 0),
                ID_TRAY_STARTUP, L"Start with Windows");
    AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(menu, MF_STRING, ID_TRAY_EXIT, L"Exit");
    if (g_tidied.empty()) EnableMenuItem(menu, ID_TRAY_UNDO, MF_BYCOMMAND | MF_GRAYED);
    if (!g_trayTarget) {
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
        if (chosen != 0) SendMessageW(g_window, WM_COMMAND, chosen, 0);
        PostMessageW(g_window, WM_NULL, 0, 0);
    }
    DestroyMenu(menu);
    g_trayTarget = nullptr;
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
        CheckDlgButton(dialog, IDC_SKIP_TOPMOST, g_settings.skipTopmost ? BST_CHECKED : BST_UNCHECKED);
        CheckDlgButton(dialog, IDC_ONLY_CURRENT, g_settings.onlyCurrentMonitor ? BST_CHECKED : BST_UNCHECKED);
        HWND combo = GetDlgItem(dialog, IDC_MOVE_MODE);
        SendMessageW(combo, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"Preserve relative position"));
        SendMessageW(combo, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"Center on destination"));
        SendMessageW(combo, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"Move and maximize"));
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
                Warn(L"Shortcuts must be unique. Each enabled shortcut must include Ctrl, Alt or Shift.", dialog);
                return TRUE;
            }
            size_t failure = 0;
            if (!RegisterHotkeys(draft, &failure)) {
                ReportHotkeyFailure(failure, dialog);
                return TRUE;
            }
            UnregisterHotkeys();
            if (!SaveSettings(draft)) {
                Warn(L"Could not save all settings to the current user's registry.", dialog);
                return TRUE;
            }
            const bool autostart = IsDlgButtonChecked(dialog, IDC_RUN_AT_STARTUP) == BST_CHECKED;
            if (autostart != StartupEnabled() && !SetStartup(autostart)) {
                Warn(L"Could not update the Windows startup registry value.", dialog);
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
    DialogBoxParamW(g_instance, MAKEINTRESOURCEW(IDD_SETTINGS), g_window, SettingsProc, 0);
    size_t failure = 0;
    if (!RegisterHotkeys(g_settings, &failure)) ReportHotkeyFailure(failure);
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
            DispatchAction(static_cast<HotkeyIndex>(index), ActiveWindow());
        }
        return 0;
    }
    case WM_SHOWSETTINGS:
        ShowSettings();
        return 0;
    case WM_COMMAND:
        switch (LOWORD(wparam)) {
        case ID_TRAY_TIDY: DispatchAction(Tidy, g_trayTarget); break;
        case ID_TRAY_UNDO: UndoTidy(); break;
        case ID_TRAY_TOPMOST: DispatchAction(Topmost, g_trayTarget); break;
        case ID_TRAY_NEXT: DispatchAction(Next, g_trayTarget); break;
        case ID_TRAY_PREV: DispatchAction(Previous, g_trayTarget); break;
        case ID_TRAY_MINIMIZE: DispatchAction(Minimize, g_trayTarget); break;
        case ID_TRAY_SETTINGS: ShowSettings(); break;
        case ID_TRAY_STARTUP:
            if (!SetStartup(!StartupEnabled())) Warn(L"Could not update Windows startup.");
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

    WNDCLASSEXW cls{};
    cls.cbSize = sizeof(cls);
    cls.hInstance = instance;
    cls.lpfnWndProc = WindowProc;
    cls.lpszClassName = kWindowClass;
    cls.hIcon = LoadIconW(nullptr, IDI_APPLICATION);
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
