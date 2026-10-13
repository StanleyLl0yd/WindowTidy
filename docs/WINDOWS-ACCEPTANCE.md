# Interactive Windows 10/11 acceptance

Tracking issue: [#2](https://github.com/StanleyLl0yd/WindowTidy/issues/2). This procedure records **hands-on** behavior, not CI results. Perform it separately on Windows 10 x64 and Windows 11 x64. The complete A01-A21 acceptance gate requires hands-on checks on both Windows 10 and Windows 11. A release may be issued earlier only with a version-specific, documented owner-approved **limited** acceptance that explicitly discloses the missing test coverage. Keep issue #2 open until the full matrix is complete.

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

## Additional checks for the localized hotkey diagnostics build

| ID | Actions | Expected outcome |
| --- | --- | --- |
| A20 | Run on a Windows account with Russian display language; reopen the EXE on English, German or another non-Russian display-language account. | Tray and settings are in Russian only for Russian Windows UI. Otherwise English; no mixed untranslated labels, broken Cyrillic or clipped text. Changing input keyboard layout alone does not change UI language. |
| A21 | Use tray **Hotkey diagnostics...**, press every shortcut in a normal app, and compare effects and registration statuses. Arrange a conflict for a shortcut, restart and repeat. | Each active binding reports registered; conflicts report unavailable with Win32 code; disabled bindings report disabled. Other independent hotkeys still respond. If all keys register but do nothing, record the target app, exact shortcut, Windows display/input languages and whether it also fails in Notepad. |

## Explicit tray target picker (M1)

| ID | Actions | Expected outcome |
| --- | --- | --- |
| A22 | Open an ordinary titled window, right-click Window Tidy in the tray when Explorer has focus, open **Choose target window...**, then choose the intended window and an action. Repeat after closing or hiding that target, and test with two similarly titled windows. | Targeted operation affects only the explicitly selected still-eligible window. If its HWND/PID/thread changes or it is no longer eligible, the operation is safely ignored. No application other than the selected target is manipulated. The original direct commands remain safely unavailable when no foreground target can be proven. |
| A23 | Inspect the picker with many open windows, a modal dialogue, Windows Terminal, an elevated window, and a window with no title. Also test keyboard-only menu navigation. | The picker lists a bounded, on-demand set of titled visible windows without stale entries; nonstandard or inaccessible windows do not hang/crash the tray or redirect operations. Document any omitted or unsupported window type instead of marking it PASS automatically. |

## M1 window diagnostics retest (A24)

After the owner's 2026-10-13 GUI feedback on main `2778956c`, the window picker, topmost toggle, normal minimize and monitor moves, duplicate titles, large menu, keyboard menu navigation and regression checks all passed on one desktop. Tidy missed an ADUC snap-in launched under another user and a Telegram mini-app; Task Manager did not minimize or move. Closing a target while the tray menu is open was not tested. These are **partial** desktop observations with the OS version still unspecified, not evidence of full A22/A23 or #2 acceptance.

| ID | Actions | Expected outcome |
| --- | --- | --- |
| A24 | Using the new **Window diagnostics...** tray command, inspect ADUC under another account, Telegram's mini-app window, and Task Manager. First run a Tidy pass with Notepad as the chosen foreground window, then inspect the read-only diagnostics before Undo. Compare reported style-based exclusions and any last-Tidy failed attempts with the actual state of each window. Record which windows appear, whether they have no minimize control, are tool/owned/topmost, or are style-eligible but still do not respond. | Diagnostics are displayed only when explicitly requested, in RU/EN, without log files, foreground polling, crashes or UI privilege escalation. 'Eligible by style' is not a promise an elevated or other-user process will allow actions. A skipped popup need not be force-minimized. Record OS edition/build and privilege context, and mark unsupported cases BLOCKED/FAIL rather than PASS. |

## M1 A25: window-class and action outcome follow-up

Owner's A24 observations on 2026-10-13: Telegram mini-app lacks minimize and caption styles (thus skipped by the current policy); Task Manager and ADUC from another user pass the Tidy style filter but remain unminimized after Tidy. The notification-area overflow shell surface unexpectedly appears among eligible titled windows. No personal titles, usernames, mail addresses or paths are copied from the user's report.

| ID | Actions | Expected outcome |
| --- | --- | --- |
| A25 | Open new **Window diagnostics...** for Telegram mini-app, Task Manager, ADUC and notification overflow, and record only each window's **Class** (not private titles). Run direct Minimize and Next Monitor on Task Manager, reopen diagnostics and record any failure lines, with Win32 error codes where available. | Window classes distinguish genuine applications from shell popups. Minimize/Move outcomes are reported only for live matching HWND/PID/thread, with no logs or background queries. Failures are not silently reclassified as successful; no elevated rights or blanket force-hide behavior is introduced. |

## M1 A25 owner observations (2026-10-13)

The owner tested `5da1bb1` on an actual Windows desktop. Store only classes and technical results, not private filenames, user names, e-mail addresses, paths or document titles:

- Explorer overflow: `TopLevelWindowForOverflowXamlIsland`, incorrectly included in the diagnostic list before the fix.
- Telegram mini-app: `Qt51519QWindow`, with no standard minimize button or caption, skipped by the current Tidy/Move policy. Telegram's main window is a different Qt class and remains unaffected.
- Task Manager: `TaskManagerWindow`; direct minimize did not take effect, and the monitor move failed with **Win32 error 5, access denied**. Window Tidy is an ordinary user process; permissions are not bypassed.
- ADUC snap-in: `MMCMainFrame`; the earlier owner run saw a failed Tidy attempt while launched under another user's credentials. Whether elevation/user isolation is the exact cause is not yet established. Its style checks are normal.

A25 diagnostic behavior is confirmed for these scenarios; the *underlying exceptional-window actions are not fixed*. Keep #3 open.

| ID | Actions | Expected outcome |
| --- | --- | --- |
| A26 | With the overflow panel open and multiple ordinary app windows, start the newest build and inspect **Choose target window...** and **Window diagnostics...**. Attempt Tidy and explicit actions for ordinary windows. In Task Manager's diagnostic failure, check the localized message for `Win32: 5`. | `TopLevelWindowForOverflowXamlIsland` is absent from all target lists. Ordinary application actions remain functional. An access-denied message is explained without elevation or access-control bypass. If an unsupported app is skipped, it is not mistaken for a successful action. |

## M1 A27: Tidy restore identity guard

The user accepted A26 on 2026-10-13 with three PASS results on `7d93bc46`: XAML notification overflow excluded from chooser and diagnostics, localized Win32 access-denied explanation confirmed on Task Manager, and ordinary minimize/restore/move actions still working. The A26 report does not establish Windows 10 and 11 coverage.

| ID | Actions | Expected outcome |
| --- | --- | --- |
| A27 | Open Notepad A and B, use Tidy to minimize B, close B from the taskbar while minimized, then create a new window (including another Notepad window) and invoke Undo. Repeat with a minimized normal and a maximized window, without closing them. | Undo never restores or acts on an unrelated newly opened window. Remaining matching windows restore, preserving prior maximized state. The identity guard checks HWND, PID, and thread ID, though Win32 handles can theoretically be reused within the same thread; no absolute atomicity is claimed. |

## M1 A28: Settings keyboard and accessibility

Native Settings now uses unique translated Alt+letter mnemonics for all five hotkey labels, the two Tidy options, Move mode and autostart. Native logic tests verify marker presence/uniqueness, but **interactive keyboard navigation and assistive-technology results still require actual Windows desktop testing**.

| ID | Actions | Expected outcome |
| --- | --- | --- |
| A28 | Open Settings with the keyboard. Use the displayed Alt+underlined-letter access keys on each hotkey label, both Tidy options, Move mode and autostart (use the matching input language where needed). Navigate using Tab/Shift+Tab, press Space on X to clear a hotkey, Enter to save, Esc to cancel, and repeat with a screen reader if available. | Each label focuses its adjacent hotkey control; checkboxes toggle; Move mode receives focus; saving persists, cancelling discards edits, and no conflicting mnemonics exist. Screen-reader naming of X clear buttons is separately pending and is **not** proven by CI. |

## M1 A29: share diagnostics without private window titles

The on-demand Window diagnostics dialog retains the normal local view, but adds **Copy without titles / Копировать без названий**. Only a direct user click invokes the Windows clipboard. The copied report retains class names, action eligibility and non-sensitive error codes, and replaces each window title with a constant placeholder. The Windows clipboard is accessible to other processes in the session; inspect the pasted report before sharing.

| ID | Actions | Expected outcome |
| --- | --- | --- |
| A29 | Open Window diagnostics with a few windows whose titles include deliberately recognizable **non-sensitive dummy** document names. Click **Copy without titles**, paste into Notepad and inspect. Also test repeated copying and a locked clipboard if possible, on Russian and English UI. | Pasted report contains no window titles, dummy document names, usernames, e-mail addresses or file paths. It preserves class/status/error information, displays the copied confirmation, and reports clipboard errors without crashing or writing log files. The normal diagnostic view remains unchanged. |

## Post-test and release decision

- Restore the original hotkeys, Tidy settings and exact autostart Run value, or delete test settings when using a disposable account. Exit Window Tidy.
- For a failure, file a dedicated issue with test ID, tested commit/hash, OS build, display layout, reproduction steps, expected/observed result and sanitized evidence. Retest the affected case after the fix, then rerun a general smoke pass.
- [#3](https://github.com/StanleyLl0yd/WindowTidy/issues/3) is the follow-up for improving tray target selection and exceptional windows. The safe fallback in A14 is not proof of a complete UX solution.
- [#4](https://github.com/StanleyLl0yd/WindowTidy/issues/4) is the owner's source-license choice. Do not infer a license from public visibility.
- Full acceptance requires issue #2 to contain positive Windows 10 **and** Windows 11 evidence with identified defects resolved. Version-specific releases with explicitly documented limited device verification may be published before this full gate, provided the release notes state that the complete matrix is still open. Verify exact-main Windows CI and every release-build check before publishing.

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
