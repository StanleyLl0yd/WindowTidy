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
- [ ] Complete interactive Windows GUI acceptance ([#2](https://github.com/StanleyLl0yd/WindowTidy/issues/2)) and address its findings before tagging a release.

## M1: Reliability

- [ ] Real-desktop tests for mixed scaling, multiple taskbar positions, portrait displays and hot-plug.
- [ ] Check taskbar/tray focus edge cases and owned/modal windows ([#3](https://github.com/StanleyLl0yd/WindowTidy/issues/3)).
- [ ] Accessibility and keyboard-only Settings workflow.
- [x] Implement retry-based recovery of the notification icon after Explorer restarts (interactive acceptance remains in #2).
- [ ] Verify elevated / protected / UWP window behavior.
- [ ] Improve diagnostic feedback without writing local log files.

## M2: Quality of life

- [ ] Optional per-application exclusion rules in the registry.
- [ ] Optional per-monitor Tidy filtering improvements.
- [ ] Native application icon and polish.
- [ ] Documentation for supported OS versions, permissions and troubleshooting.

No additional features will be introduced into M0 at the expense of basic reliability.
