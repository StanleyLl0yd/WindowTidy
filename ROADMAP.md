# Window Tidy roadmap

## Product invariants

- Native Win32 / C++20, x64. No UI frameworks, external runtime dependencies, installer, background service, telemetry or network requests.
- Registry-only configuration under HKCU. Autostart truth comes solely from the HKCU Run value.
- All five default hotkeys can be changed or disabled in the native Settings UI.
- SemVer consists of numeric components only, with tags like `v0.1.0`. Do not create prerelease suffixes.
- Never claim device testing on real monitors based solely on a CI result.
- No public license has been selected.

## M0: Foundation / v0.1.0

- [x] Create repository and initial documentation.
- [x] Native tray application, single instance and safe lifecycle.
- [x] Tidy/Undo, Topmost, Move between monitors, Minimize active.
- [x] Native Settings, hotkey collision handling, HKCU persistence and autostart.
- [x] Command-line MSVC build and Windows CI logic tests.
- [x] CI build passing with downloadable executable, embedded manifest and portable-runtime checks.
- [x] Publish portable v0.1.0 with approved icon and automated verification; the user-device acceptance was limited.
- [ ] Complete the full Windows 10 and Windows 11 GUI acceptance matrix ([#2](https://github.com/StanleyLl0yd/WindowTidy/issues/2)), independently of limited releases.

## Maintenance release 0.1.1

- [x] Correct hotkey re-registration after Settings Save, retain bindings after Cancel, and add CI regression tests.
- [x] Add automatic Russian/English UI localization and native per-shortcut diagnostics.
- [x] Obtain positive owner verification of all app actions on a Windows desktop ([#10](https://github.com/StanleyLl0yd/WindowTidy/issues/10)).
- [x] Publish 0.1.1 as an explicitly limited-acceptance numbered release, while preserving issue #2 for full OS coverage.

## M1: Reliability

- [ ] Real-desktop tests for mixed scaling, multiple taskbar positions, portrait displays and hot-plug.
- [x] Add explicit on-demand window selection to tray operations; no focus guessing, keyboard hooks or polling ([#3](https://github.com/StanleyLl0yd/WindowTidy/issues/3)).
- [ ] Verify that picker against real Explorer/taskbar focus scenarios, protected and nonstandard windows, and accessibility before closing #3.
- [x] Add unique localized Settings keyboard accelerators, with native tests.
- [ ] Verify keyboard-only Settings workflow and accessible X-button names on actual Windows desktops (A28).
- [x] Implement retry-based recovery of the notification icon after Explorer restarts (interactive acceptance remains in #2).
- [ ] Verify elevated / protected / UWP window behavior.
- [x] Add on-demand, read-only window-style diagnostics and memory-only results for unsuccessful Tidy attempts; no local log files (desktop confirmation still required).

## M2: Quality of life

- [ ] Optional per-application exclusion rules in the registry.
- [ ] Optional per-monitor Tidy filtering improvements.
- [x] Approved native app icon embedded into the executable and tray (desktop visual acceptance remains open).
- [ ] Other application polish.
- [ ] Documentation for supported OS versions, permissions and troubleshooting.

No additional features will be introduced into M0 at the expense of basic reliability.
