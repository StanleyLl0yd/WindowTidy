# Window Tidy

A lightweight, portable Windows desktop window manager written in native C++20 and Win32 API. No runtime frameworks, installers, third-party libraries, network access or telemetry.

## Features

- **Tidy / Undo:** minimize eligible windows while keeping the active application available; restore only windows minimized by the last Tidy operation.
- **Always on top:** toggle topmost status of the focused window.
- **Move monitor:** move a window to the next or previous display, preserving relative placement, centering or maximizing it.
- **Minimize:** minimize the active window.
- **Global hotkeys:** customize all five shortcuts in Settings.
- **Tray application:** Windows notification area icon and native Settings dialog.
- **Windows Registry only:** no INI, JSON or local settings files.

### Default hotkeys

| Action | Shortcut |
| --- | --- |
| Tidy / Undo | Ctrl+Alt+Z |
| Always on top | Ctrl+Alt+T |
| Next monitor | Ctrl+Alt+Right |
| Previous monitor | Ctrl+Alt+Left |
| Minimize active | Alt+H |

Shortcuts are registered with Windows. If a shortcut is already owned by another app, Window Tidy reports the collision instead of intercepting or injecting keyboard input.

## Requirements

Windows 10 or Windows 11, 64-bit.

## Build

No Visual Studio solution/project file is required. The build uses the MSVC command-line compiler and Windows SDK tools, available in Visual Studio Build Tools.

From an **x64 Native Tools Command Prompt**:

```cmd
scripts\build.cmd
```

Output: `build\x64\WindowTidy.exe`. The build script also creates and executes the native logic tests when called with `test`:

```cmd
scripts\build.cmd test
```

GitHub Actions builds and tests on a Windows runner; successful runs publish a downloadable unsigned executable artifact. Do not treat an artifact as a released or digitally signed build.

## Configuration

`HKCU\Software\WindowTidy` stores editable hotkeys and Tidy/Move options as `REG_DWORD`. Autostart is determined exclusively from the non-empty `HKCU\Software\Microsoft\Windows\CurrentVersion\Run\WindowTidy` registry value; there is no duplicate autostart setting.

Hotkeys and settings are per-user. Window Tidy does not require administrator privileges, but Windows may restrict manipulating higher-integrity, protected, or otherwise special windows.

## Safety and limitations

- Tidy ignores desktop/taskbar, hidden, cloaked, minimized, non-minimizable and tool windows; it does not force-close anything.
- Undo is an in-memory, single-session operation and will not reopen closed applications.
- Operations target an eligible foreground top-level window; some privileged, unusual or application-managed windows may reject movement or changes.
- Multi-monitor placement uses the working area (excluding taskbars). Windows may enforce additional window-size limits.
- Settings use the standard Windows hotkey control (Ctrl, Alt and Shift combinations); global Win-key shortcuts are not exposed by this control.
- UI behavior on real multi-monitor desktops still needs interactive acceptance testing; a successful CI build is not proof of those scenarios.

## Versioning

Only numerical SemVer: `MAJOR.MINOR.PATCH` (Git tags: `v0.1.0`, `v0.1.1`, ...). No prerelease suffixes.

## Development

See [ROADMAP.md](ROADMAP.md) for the current scope. Contributions should keep the executable native, dependency-free and portable.

**License:** no license granted yet. The owner has not selected an open-source license.
