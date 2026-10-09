# M0 interactive Windows acceptance (v0.1.0 gate)

Tracking issue: [#2](https://github.com/StanleyLl0yd/WindowTidy/issues/2). This procedure records **hands-on** behavior, not CI results. Perform it separately on Windows 10 x64 and Windows 11 x64. Do not tag a release until all mandatory checks pass, or a documented owner-approved exception exists.

## Prepare a reproducible test

1. Use a regular, non-administrator Windows account, preferably a disposable test account. Do not run the app elevated. Close any previously running Window Tidy process.
2. Download `WindowTidy-Windows-x64` from the successful Windows x64 Actions run for the exact tested commit. Unzip the unsigned executable and `SHA256SUMS.txt` to a normal user-writable path **containing spaces**, e.g. `%LOCALAPPDATA%\Window Tidy Test\`.
3. In PowerShell, run `Get-FileHash -Algorithm SHA256 .\WindowTidy.exe` and compare its value with the hash inside `SHA256SUMS.txt`. Record the commit, run ID and executable hash. Do not confuse the artifact-ZIP digest with the EXE digest.
4. Record the Windows edition/build (`winver`), Explorer/taskbar layout, monitors, their resolutions/relative coordinates/orientations, per-display scaling, and whether the session is local or remote. Start with two or more ordinary windows open, such as Notepad and File Explorer.
5. Keep existing user registry settings safe. Prefer a disposable account. If using an established account, export `HKCU\Software\WindowTidy` before testing (if present) and separately record the exact value and type of `HKCU\Software\Microsoft\Windows\CurrentVersion\Run\WindowTidy`. Do **not** export or publish the whole Run key; it can contain unrelated personal entries. Restore the original WindowTidy values afterwards.
6. If Windows warns about an unsigned file, evaluate its provenance and the digest. Do not globally disable security controls.

Record each case as **PASS**, **FAIL**, or **BLOCKED** for **each OS**. For FAIL, record actual vs expected, steps, and a screenshot with unrelated application titles and personal information hidden. For BLOCKED, state the missing setup. A CI pass never replaces a hands-on PASS.

## Mandatory test cases

| ID | Actions | Expected outcome |
| --- | --- | --- |
| A01 | Start the app, open the tray menu, double-click the tray icon, reopen Settings, then exit through the menu. | One tray icon, functioning Settings and clean exit; icon disappears on exit. |
| A02 | Launch the EXE twice with the first instance running. | Only one background instance; the second invocation brings up Settings rather than creating another tray process. |
| A03 | With three ordinary windows open, make one foreground and press **Ctrl+Alt+Z** twice. | First press minimizes other eligible windows while preserving the foreground app; second restores only windows minimized by that Tidy operation. |
| A04 | Focus an ordinary window and press **Ctrl+Alt+T** twice. | Always-on-top is enabled then disabled for that window; no unrelated window changes. |
| A05 | On a system with at least two monitors, press **Ctrl+Alt+Right** and **Ctrl+Alt+Left**. | Focused eligible window moves to the next/previous monitor and back; no offscreen/inaccessible window. |
| A06 | Focus an ordinary minimizable window and press **Alt+H**. | Only that window is minimized. |
| A07 | In Settings, reassign **each of the five hotkeys**, save, close, reopen, and use them. Then clear each using its **X**, save and reopen. | Every valid custom shortcut works; each disabled shortcut remains disabled after restart; changing one action does not silently alter another. Restore the defaults after this case. |
| A08 | Assign one enabled shortcut to two actions and try Save. Also try a shortcut that conflicts with another app/Windows where reproducible. | Duplicate/unavailable binding is rejected; previously valid saved bindings continue to function. Note any binding that cannot be tested manually. |
| A09 | Pre-minimize one window, then Tidy and Undo with several normal and maximized windows. | Undo does not restore the pre-minimized window; windows minimized by Tidy return, with maximized state preserved. |
| A10 | Run Tidy with **Skip always-on-top** enabled, using one marked topmost and several regular windows. Repeat with **Tidy only on current monitor** enabled. | Topmost window remains visible when excluded; with current-monitor mode only eligible windows on the foreground window's monitor are tidied. |
| A11 | Test Tidy/Undo with an owned/modal dialog, tool window, desktop/taskbar, and a window that has already closed. | No shell damage, crashes or incorrect restore of an unrelated window; owned/tool and non-eligible windows are skipped as designed. Record app-specific exceptions. |
| A12 | Test the three **Move mode** settings: Preserve, Center and Move and maximize. Repeat from a maximized window. | Placement follows the chosen mode; maximized windows stay maximized; switching back is possible. |
| A13 | Repeat monitor moves with a monitor left of the primary (negative X), differing DPI scales, and portrait orientation; unplug/replug a display if available. | No crash, unusable geometry, or permanent loss of the window outside visible work areas. Mark each missing arrangement BLOCKED, not PASS. |
| A14 | Use tray actions with a normal app active and again after clicking the notification area when Explorer/taskbar owns focus. | Operations affect **only** an established eligible target. If the intended target cannot be established, the target-specific commands should be disabled, not silently act on Explorer or another window. Capture whether tray operation actually remains usable. |
| A15 | Keep the app running and restart Explorer using Task Manager; wait for the notification area to reappear. | Tray icon is restored without spawning a second application. Settings, menu and shortcuts still work. |
| A16 | In Settings toggle **Start with Windows** from off to on and back; inspect only `HKCU\Software\Microsoft\Windows\CurrentVersion\Run\WindowTidy`. Repeat when EXE is under a path with spaces. | Checked state follows the actual Run entry; enabling creates a quoted command for this EXE; disabling removes just the WindowTidy value, without changing other Run entries. |
| A17 | Try elevated/protected app windows, Windows Terminal, a frameless app and UWP windows if available. | Operations are safe and do not hang or target unrelated windows, even when Windows disallows an action. Document individual application outcomes rather than assuming full support. |
| A18 | Inspect process properties and relevant OS tools for unexpected elevation, additional processes, services, drivers and outbound network traffic. | User-level application, no service/driver installation or unexpected network activity. Note that absence of observed traffic during a short test is limited evidence. |
| A19 | Close Settings via Cancel, reopen, make changes and Save, then restart the EXE. | Cancel discards edits; saved settings persist per user under HKCU and survive process restart. |

## Post-test and release decision

- Restore the original hotkeys, Tidy settings and exact autostart Run value, or delete test settings when using a disposable account. Exit Window Tidy.
- For a failure, file a dedicated issue with test ID, tested commit/hash, OS build, display layout, reproduction steps, expected/observed result and sanitized evidence. Retest the affected case after the fix, then rerun a general smoke pass.
- [#3](https://github.com/StanleyLl0yd/WindowTidy/issues/3) is the follow-up for improving tray target selection and exceptional windows. The safe fallback in A14 is not proof of a complete UX solution.
- [#4](https://github.com/StanleyLl0yd/WindowTidy/issues/4) is the owner's source-license choice. Do not infer a license from public visibility.
- Only after issue #2 contains positive Windows 10 **and** Windows 11 evidence and identified defects are resolved should `v0.1.0` be considered. Verify the new final exact-main Windows CI before publishing an unsigned artifact as a release.

## Acceptance result template (copy into #2)

```text
Commit / workflow / EXE SHA-256:
Windows 10: build / desktop type / monitors / DPI:
Windows 11: build / desktop type / monitors / DPI:
Results per OS: A01 ... A19 = PASS / FAIL / BLOCKED
Failures (test ID, steps, expected, observed, evidence link):
Blocked environments/tests and why:
Fix PRs and retest results:
Reviewer and date:
Release gate: PASS / NOT READY
```
