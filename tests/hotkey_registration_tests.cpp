#include <windows.h>
#include <oleacc.h>
#include <cwchar>
#include <array>
#include "../res/resource.h"
#include <cstdio>
#include <string>

namespace {
constexpr int kReserved = 700;
constexpr int kProbe = 701;
struct Shortcut { UINT mods; UINT key; };

bool Available(Shortcut shortcut) {
    if (!RegisterHotKey(nullptr, kProbe, shortcut.mods, shortcut.key)) return false;
    UnregisterHotKey(nullptr, kProbe);
    return true;
}

void StopChild(PROCESS_INFORMATION& process) {
    TerminateProcess(process.hProcess, 0);
    WaitForSingleObject(process.hProcess, 5000);
    CloseHandle(process.hProcess);
    CloseHandle(process.hThread);
}

bool RunCollision(const wchar_t* path, Shortcut reserved, Shortcut expectedToWork) {
    if (!Available(reserved) || !Available(expectedToWork)) {
        std::fputs("Required hotkey is unavailable before launch\n", stderr);
        return false;
    }
    if (!RegisterHotKey(nullptr, kReserved, reserved.mods, reserved.key)) return false;

    std::wstring command = L"\"" + std::wstring(path) + L"\"";
    STARTUPINFOW startup{};
    startup.cb = sizeof(startup);
    PROCESS_INFORMATION process{};
    const BOOL created = CreateProcessW(path, command.data(), nullptr, nullptr, FALSE, 0,
                                        nullptr, nullptr, &startup, &process);
    if (!created) {
        UnregisterHotKey(nullptr, kReserved);
        return false;
    }
    Sleep(2500);
    const bool running = WaitForSingleObject(process.hProcess, 0) == WAIT_TIMEOUT;
    const bool independentShortcutWorks = !Available(expectedToWork);
    StopChild(process);
    UnregisterHotKey(nullptr, kReserved);
    const bool released = Available(expectedToWork);
    return running && independentShortcutWorks && released;
}
struct DialogQuery {
    DWORD pid;
    HWND dialog{};
};

BOOL CALLBACK FindSettingsDialog(HWND window, LPARAM context) {
    auto& query = *reinterpret_cast<DialogQuery*>(context);
    DWORD pid{};
    GetWindowThreadProcessId(window, &pid);
    if (pid == query.pid && GetDlgItem(window, IDC_HK_TIDY) != nullptr) {
        query.dialog = window;
        return FALSE;
    }
    return TRUE;
}

bool WaitForShortcut(Shortcut shortcut, bool shouldBeAvailable, unsigned timeoutMs) {
    for (unsigned elapsed = 0; elapsed < timeoutMs; elapsed += 50) {
        if (Available(shortcut) == shouldBeAvailable) return true;
        Sleep(50);
    }
    return false;
}

HWND WaitForSettings(DWORD pid) {
    for (unsigned elapsed = 0; elapsed < 5000; elapsed += 50) {
        DialogQuery query{pid};
        EnumWindows(FindSettingsDialog, reinterpret_cast<LPARAM>(&query));
        if (query.dialog) return query.dialog;
        Sleep(50);
    }
    return nullptr;
}

bool HasAccessibleClearButtonNames(HWND dialog) {
    const HRESULT initialized = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    if (FAILED(initialized)) return false;
    constexpr std::array<int, 5> ids{
        IDC_CLEAR_TIDY, IDC_CLEAR_TOPMOST, IDC_CLEAR_NEXT,
        IDC_CLEAR_PREV, IDC_CLEAR_MINIMIZE
    };
    bool allNamed = true;
    for (int id : ids) {
        const HWND button = GetDlgItem(dialog, id);
        IAccessible* accessible{};
        if (!button || FAILED(AccessibleObjectFromWindow(
                button, static_cast<DWORD>(OBJID_CLIENT), __uuidof(IAccessible),
                reinterpret_cast<void**>(&accessible))) || !accessible) {
            allNamed = false;
            break;
        }
        VARIANT self{};
        self.vt = VT_I4;
        self.lVal = CHILDID_SELF;
        BSTR name{};
        const HRESULT result = accessible->get_accName(self, &name);
        const bool named = SUCCEEDED(result) && name != nullptr &&
            (std::wcsstr(name, L"Clear ") == name ||
             std::wcsstr(name, L"Очистить ") == name);
        if (!named) std::fprintf(stderr, "Missing accessible name for clear button %d\n", id);
        if (name) SysFreeString(name);
        accessible->Release();
        if (!named) { allNamed = false; break; }
    }
    CoUninitialize();
    return allNamed;
}

bool RunSettingsCycle(const wchar_t* path, Shortcut expected, bool save) {
    if (!Available(expected)) {
        std::fputs("Settings test: shortcut unavailable before launch\n", stderr);
        return false;
    }
    std::wstring command = L"\"" + std::wstring(path) + L"\"";
    STARTUPINFOW startup{};
    startup.cb = sizeof(startup);
    PROCESS_INFORMATION process{};
    if (!CreateProcessW(path, command.data(), nullptr, nullptr, FALSE, 0,
                        nullptr, nullptr, &startup, &process)) return false;
    const bool initiallyReserved = WaitForShortcut(expected, false, 5000);
    HWND mainWindow{};
    for (unsigned elapsed = 0; elapsed < 5000 && !mainWindow; elapsed += 50) {
        mainWindow = FindWindowW(L"WindowTidyWindow_38132E6B", nullptr);
        if (!mainWindow) Sleep(50);
    }
    DWORD ownerPid{};
    if (mainWindow) GetWindowThreadProcessId(mainWindow, &ownerPid);
    const bool opened = ownerPid == process.dwProcessId &&
                        PostMessageW(mainWindow, WM_SHOWSETTINGS, 0, 0);
    const HWND dialog = opened ? WaitForSettings(process.dwProcessId) : nullptr;
    const bool suspended = dialog && WaitForShortcut(expected, true, 5000);
    const bool accessibleNames = dialog && HasAccessibleClearButtonNames(dialog);
    const bool commandPosted = dialog &&
        PostMessageW(dialog, WM_COMMAND, MAKEWPARAM(save ? IDOK : IDCANCEL, BN_CLICKED), 0);
    // Wait for the Save/Cancel handler to finish before checking restored bindings.
    bool dialogClosed = false;
    if (commandPosted) {
        for (unsigned elapsed = 0; elapsed < 5000; elapsed += 50) {
            DialogQuery query{process.dwProcessId};
            EnumWindows(FindSettingsDialog, reinterpret_cast<LPARAM>(&query));
            if (!query.dialog) { dialogClosed = true; break; }
            Sleep(50);
        }
    }
    const bool restored = dialogClosed && WaitForShortcut(expected, false, 5000);
    StopChild(process);
    const bool released = WaitForShortcut(expected, true, 2000);
    if (!(initiallyReserved && opened && dialog && suspended && accessibleNames &&
          commandPosted && dialogClosed && restored && released)) {
        std::fprintf(stderr,
            "Settings %s: initiallyReserved=%d ownerPidMatches=%d opened=%d dialog=%d suspended=%d "
            "accessibleNames=%d commandPosted=%d dialogClosed=%d restored=%d released=%d\n",
            save ? "Save" : "Cancel", initiallyReserved, ownerPid == process.dwProcessId,
            opened, dialog != nullptr, suspended, accessibleNames,
            commandPosted, dialogClosed, restored, released);
    }
    return initiallyReserved && opened && dialog && suspended && accessibleNames &&
           commandPosted && dialogClosed && restored && released;
}

} // namespace

int wmain(int argc, wchar_t** argv) {
    if (argc != 2) {
        std::fputs("Usage: hotkey_registration_tests.exe WindowTidy.exe\n", stderr);
        return 1;
    }
    wchar_t path[32768]{};
    const DWORD length = GetFullPathNameW(argv[1], static_cast<DWORD>(std::size(path)), path, nullptr);
    if (!length || length >= std::size(path)) {
        std::fputs("Cannot resolve executable path\n", stderr);
        return 1;
    }
    constexpr Shortcut tidy{MOD_CONTROL | MOD_ALT, 'Z'};
    constexpr Shortcut minimize{MOD_ALT, 'H'};
    if (!RunCollision(path, minimize, tidy)) {
        std::fputs("FAILED: Alt+H collision must preserve Ctrl+Alt+Z\n", stderr);
        return 1;
    }
    if (!RunCollision(path, tidy, minimize)) {
        std::fputs("FAILED: Ctrl+Alt+Z collision must preserve Alt+H\n", stderr);
        return 1;
    }
    if (!RunSettingsCycle(path, tidy, false)) {
        std::fputs("FAILED: shortcut must be restored after Settings Cancel\n", stderr);
        return 1;
    }
    // Save writes per-user registry settings; restrict this path to ephemeral CI desktops.
    if (GetEnvironmentVariableW(L"CI", nullptr, 0) != 0 &&
        !RunSettingsCycle(path, tidy, true)) {
        std::fputs("FAILED: shortcut must remain registered after Settings Save\n", stderr);
        return 1;
    }
    std::puts("Hotkey conflicts and Settings Cancel/Save lifecycle checks passed");
    return 0;
}
