# Window Tidy roadmap

## Product invariants

- Native Win32 / C++20, x64. No UI frameworks, external runtime dependencies, installer, background service, telemetry or network requests.
- Registry-only configuration under HKCU. Autostart truth comes solely from the HKCU Run value.
- All five default hotkeys can be changed or disabled in the native Settings UI.
- SemVer consists of numeric components only, with tags like `v0.1.0`. Do not create prerelease suffixes.
- Never claim device testing on real monitors based solely on a CI result.
- No public license has been selected.

## Mandatory exit audits for every M stage

The end of **M1 and every following milestone (M2, M3, M4, ...)** requires the mandatory whole-repository **audit + safe refactoring + code review** in [AGENTS.md](AGENTS.md), **after functional/acceptance work but before milestone closure or progression to the next M stage**. Each milestone must have a tracked, non-optional exit-audit issue, with two complete repository passes, concrete justified changes, full final-main CI/security/test verification, separate real-desktop acceptance evidence or explicitly BLOCKED cases, and a written final report. An issue cannot be checked off merely because a previous mid-stage audit was done.

- **M1:** [#22](https://github.com/StanleyLl0yd/WindowTidy/issues/22)
- **M2:** [#23](https://github.com/StanleyLl0yd/WindowTidy/issues/23)
- **M3:** [#24](https://github.com/StanleyLl0yd/WindowTidy/issues/24)
- **M4 and later:** create the corresponding exit-audit issue when adding the stage to this roadmap; use exactly the same mandatory gate.

No calendar date is guessed; each audit is triggered by completing that stage's development and planned testing. Previously published limited-acceptance releases are not retrospectively altered.

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
- [x] Add an explicit redacted Copy button for technical diagnostics (GUI clipboard confirmation A29 pending).
- [ ] **M1 exit gate:** after M1 functional work and acceptance, execute the full two-pass repository audit, refactoring and code review; attach final report, reviewed PRs, final-main CI and GUI evidence to [#22](https://github.com/StanleyLl0yd/WindowTidy/issues/22) before marking M1 complete.

## M2: Quality of life and window arrangement (planned)

Owner-approved planning scope (2026-10-13), **not implemented**. Proposed numeric version target: 0.3.0, not a release commitment.

Win32 dependencies, command UX, one-step Undo semantics and acceptance gates are detailed in [docs/M2-M3-WINDOW-ACTIONS.md](docs/M2-M3-WINDOW-ACTIONS.md).

- [ ] **Maximize / Restore** a selected eligible window using native show-state semantics, without affecting other windows or its topmost status.
- [ ] **Center** a selected window within its current monitor's working area (taskbar-aware), retaining its size when possible.
- [ ] **Snap** to left/right halves or one of four quarters of the target monitor's working area. This is Window Tidy placement, not Windows Snap Groups integration.
- [ ] **Undo placement**: one-step, per-window reversal of the last successful Window Tidy placement action (including existing next/previous monitor Move once integrated). Keep this separate from **Undo Tidy**, which restores minimized windows. Do not undo a user's later manual move unexpectedly.
- [ ] Tray UX: add a compact **Arrange / Разместить** submenu under explicitly chosen windows, plus Maximize/Restore and Undo placement. Preserve the five existing hotkeys and their defaults; do not add new global shortcuts automatically.
- [ ] Reuse Win32 `SetWindowPos`, `ShowWindow`, `GetWindowPlacement` / `SetWindowPlacement`, `MonitorFromWindow` and `MONITORINFO.rcWork`, plus current C++20 geometry and HWND/PID/thread checks. No additional dependencies or elevated access.
- [ ] Keep workspace-coordinate `WINDOWPLACEMENT.rcNormalPosition` separate from screen coordinates passed to `SetWindowPos`. Only save Undo state on verified success; discard stale window identity and stale placements.
- [ ] Add pure geometry/state tests and real Windows 10/11 tests for odd pixels, taskbar edges, portrait, negative coordinates, mixed DPI, maximized/minimized states, non-resizable/app-minimum-size windows, permission denials and menu keyboard navigation.
- [ ] Optional per-application exclusion rules in HKCU Registry (existing M2 feature, independent priority).
- [ ] Optional per-monitor Tidy filtering improvements (existing M2 feature, independent priority).
- [x] Approved native app icon embedded into executable and tray (desktop visual acceptance remains open).
- [ ] Other application polish and documentation for OS compatibility, permissions and troubleshooting.
- [ ] **M2 exit gate:** after M2 functional work and acceptance, complete the full two-pass repository audit, refactoring and code review and attach evidence to [#23](https://github.com/StanleyLl0yd/WindowTidy/issues/23) before marking M2 complete.

## M3: Recoverable visibility and robustness (planned)

Owner-approved **Hide / Show window** action is intentionally separated from M2 because hidden windows disappear from the current visible-only target picker. Proposed numeric version target: 0.4.0, not a release commitment.

- [ ] **Hide selected window** only for explicitly selected, eligible ordinary windows; never hide shell/taskbar/desktop or broadly target owned/modal, protected or unknown popups.
- [ ] **Show hidden windows... / Показать скрытые окна...** in the tray, separate from visible-window selection, listing only windows previously hidden by Window Tidy. Provide a safe bounded recovery path, including a restore-all option if validated.
- [ ] Distinguish Hide/Show state from **Undo Tidy** and **Undo placement**. Preserve prior normal/maximized presentation; avoid forced foreground activation where possible.
- [ ] Prove recovery on normal Exit, restart and abnormal termination before shipping. An in-memory HWND list alone cannot restore hidden windows after a crash; evaluate a small HKCU recovery journal without window titles and validate HWND/PID/thread plus process lifetime before acting. If safe recovery is not feasible, keep Hide/Show deferred rather than strand windows.
- [ ] Test repeated hide/show, closed/recreated windows, stale HWNDs, Explorer restart, elevated/other-user denials, DPI, monitor hot-plug and Windows 10/11 behavior. No new services, processes, injection, hooks, polling, telemetry or external libraries.
- [ ] Finish remaining mixed-DPI, accessibility, hot-plug and cross-OS regression hardening before a stable milestone.
- [ ] **M3 exit gate:** after M3 functional work and acceptance, complete the full two-pass repository audit, refactoring and code review and attach evidence to [#24](https://github.com/StanleyLl0yd/WindowTidy/issues/24) before marking M3 complete.

**Ordering:** close M1 user tests A27-A29 and outstanding #2/#3 acceptance first; deliver/test M2 in small pieces (maximize/center, snap, then undo placement); settle M3 recovery architecture **before** implementing Hide. Each new action needs CI tests and separate real-desktop acceptance. These are tasks to plan, not authorization to implement now.

No additional features will be introduced into M0 at the expense of basic reliability.
