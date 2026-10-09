#include <windows.h>
#include <cstdio>
#include <string>

namespace {
constexpr int kReservedAltH = 700;
constexpr int kProbeTidy = 701;

bool ProbeTidyAvailable() {
    if (!RegisterHotKey(nullptr, kProbeTidy, MOD_CONTROL | MOD_ALT, 'Z')) return false;
    UnregisterHotKey(nullptr, kProbeTidy);
    return true;
}

void StopChild(PROCESS_INFORMATION& process) {
    if (process.hProcess) {
        TerminateProcess(process.hProcess, 0);
        WaitForSingleObject(process.hProcess, 5000);
        CloseHandle(process.hProcess);
        CloseHandle(process.hThread);
    }
}
} // namespace

int wmain(int argc, wchar_t** argv) {
    if (argc != 2) {
        std::fputs("Usage: hotkey_registration_tests.exe WindowTidy.exe\n", stderr);
        return 1;
    }
    if (!ProbeTidyAvailable()) {
        std::fputs("Ctrl+Alt+Z is already reserved in the test desktop\n", stderr);
        return 1;
    }
    if (!RegisterHotKey(nullptr, kReservedAltH, MOD_ALT, 'H')) {
        std::fputs("Alt+H is already reserved in the test desktop\n", stderr);
        return 1;
    }

    wchar_t path[32768]{};
    const DWORD length = GetFullPathNameW(argv[1], static_cast<DWORD>(std::size(path)), path, nullptr);
    if (!length || length >= std::size(path)) {
        UnregisterHotKey(nullptr, kReservedAltH);
        std::fputs("Cannot resolve executable path\n", stderr);
        return 1;
    }

    std::wstring command = L"\"" + std::wstring(path) + L"\"";
    STARTUPINFOW startup{};
    startup.cb = sizeof(startup);
    PROCESS_INFORMATION process{};

    const BOOL created = CreateProcessW(path, command.data(), nullptr, nullptr, FALSE, 0,
                                        nullptr, nullptr, &startup, &process);
    if (!created) {
        UnregisterHotKey(nullptr, kReservedAltH);
        std::fputs("Cannot launch Window Tidy\n", stderr);
        return 1;
    }

    Sleep(2500);
    const bool running = WaitForSingleObject(process.hProcess, 0) == WAIT_TIMEOUT;
    const bool tidyReserved = !ProbeTidyAvailable();

    StopChild(process);
    UnregisterHotKey(nullptr, kReservedAltH);

    const bool releasedOnExit = ProbeTidyAvailable();
    if (!running || !tidyReserved || !releasedOnExit) {
        std::fprintf(stderr, "Hotkey conflict regression FAILED: running=%d, tidyReserved=%d, released=%d\n",
                     running, tidyReserved, releasedOnExit);
        return 1;
    }
    std::puts("Hotkey conflict regression passed: Alt+H conflict preserves Ctrl+Alt+Z");
    return 0;
}
