# Window Tidy

A lightweight, portable Windows desktop window manager written in native C++20 and Win32 API. No runtime frameworks, installers, third-party libraries, network access or telemetry.

## Features

- **Tidy / Undo:** minimize eligible windows while keeping the active application available; restore only windows minimized by the last Tidy operation.
- **Always on top:** toggle topmost status of the focused window.
- **Move monitor:** move a window to the next or previous display, preserving relative placement, centering or maximizing it.
- **Minimize:** minimize the active window.
- **Global hotkeys:** customize all five shortcuts in Settings.
- **Tray application:** the approved Window Tidy icon in the executable, notification area and native Settings dialog.
- **Language:** Russian UI when the current Windows display language is Russian (including regional variants); English for all other Windows display languages.
- **Hotkey diagnostics:** the tray menu reports the registration status of every shortcut and the corresponding Win32 error code when registration fails.
- **Windows Registry only:** no INI, JSON or local settings files.

### Default hotkeys

| Action | Shortcut |
| --- | --- |
| Tidy / Undo | Ctrl+Alt+Z |
| Always on top | Ctrl+Alt+T |
| Next monitor | Ctrl+Alt+Right |
| Previous monitor | Ctrl+Alt+Left |
| Minimize active | Alt+H |

Shortcuts are registered with Windows. If Windows or another application prevents registration, Window Tidy keeps all other available shortcuts working. Open **Hotkey diagnostics...** in the tray menu to see each current shortcut, whether it is registered, unregistered, unavailable (with a Win32 failure code) or disabled, the virtual-key number (VK), and how often the app received its `WM_HOTKEY` event. A shortcut is temporarily unregistered while **Settings...** is open, then restored on Cancel; successfully saved shortcuts remain registered without a second registration cycle. Reassign an unavailable shortcut in Settings. `Right/Left` and `Num 6/Num 4` are different virtual keys; check the VK number when setting up monitor moves. A registered shortcut does not imply that Windows permits the target window operation. Window Tidy does not intercept or inject keyboard input.

If Windows has already taken focus when you open the tray menu, window-specific actions are disabled rather than risking a change to an unrelated window. Use a global shortcut while the intended window is active.

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

The application icon is embedded into the executable and notification-area icon. The canonical unmodified artwork is `res/WindowTidy.source.png`; `res/WindowTidy.ico` contains its raster-only Windows-sized derivatives (16, 20, 24, 32, 40, 48, 64, 96, 128 and 256 px). No external assets are needed at runtime.

## Interface language

On startup Window Tidy reads the **Windows user interface/display language**, not the keyboard layout or regional date/number format. Russian Windows UI selects Russian; every other Windows UI language selects English. Restart the application after changing the Windows display language. There is no language setting to maintain and no external translation file.

## Configuration

`HKCU\Software\WindowTidy` stores editable hotkeys and Tidy/Move options as `REG_DWORD`. Autostart is determined exclusively from a non-empty `REG_SZ` or `REG_EXPAND_SZ` value at `HKCU\Software\Microsoft\Windows\CurrentVersion\Run\WindowTidy`; there is no duplicate autostart setting.

Hotkeys and settings are per-user. Window Tidy does not require administrator privileges, but Windows may restrict manipulating higher-integrity, protected, or otherwise special windows.

## Safety and limitations

- Tidy ignores desktop/taskbar, hidden, cloaked, minimized, non-minimizable and tool windows; it does not force-close anything.
- Undo is an in-memory, single-session operation and will not reopen closed applications.
- Operations target an eligible foreground top-level window; some privileged, unusual or application-managed windows may reject movement or changes.
- Multi-monitor placement uses the working area (excluding taskbars). Windows may enforce additional window-size limits.
- Settings use the standard Windows hotkey control (Ctrl, Alt and Shift combinations); global Win-key shortcuts are not exposed by this control.
- UI behavior on real multi-monitor desktops still needs interactive acceptance testing; a successful CI build is not proof of those scenarios.

## Versioning and releases

Only numerical SemVer: `MAJOR.MINOR.PATCH` (Git tags: `v0.1.0`, `v0.1.1`, ...). No prerelease suffixes.

GitHub Releases provide the portable executable and a SHA256SUMS.txt file. The guarded release workflow supports manual dispatch on main and an explicit version request through `.github/release-request.txt` committed to main (two lines: `version=N.N.N` and `acceptance=limited|full`). A matching `docs/releases/vN.N.N.md` must document owner verification and the acceptance scope. The workflow rebuilds and verifies the exact release commit before publishing. A limited-acceptance release does **not** mean complete Windows 10 plus Windows 11 desktop acceptance; full A01-A21 coverage remains tracked in issue #2.

## Development

See [ROADMAP.md](ROADMAP.md) for the current scope. Contributions should keep the executable native, dependency-free and portable.

**License:** no license granted yet. The owner has not selected an open-source license.
