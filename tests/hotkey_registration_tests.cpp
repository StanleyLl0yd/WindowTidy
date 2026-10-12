#include <windows.h>
#include <array>
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
    std::puts("Hotkey collision regressions passed in both directions");
    return 0;
}
