# Window actions: M2 / M3 technical decisions

Status: design only, agreed 2026-10-13. No new functions are implemented by this document.

## M2 user experience

- Choose target window from the existing tray menu. Add **Arrange / Разместить** with Center, Left half, Right half, Upper left/right and Lower left/right. Place Maximize/Restore and Undo placement beside the arrangement submenu.
- Do not create five new default hotkeys. Existing five shortcuts, Settings, Undo Tidy and the per-user registry schema remain unchanged.
- All commands are available in English and Russian. The action menu must remain keyboard traversable and bounded for crowded desktops.
- Maximize/Restore is a two-state toggle for a normal vs maximized window. Center keeps the current size if possible. Snap places the *selected* window in the target monitor's work area, without rearranging other apps. Do not claim native Windows Snap Groups integration.
- Undo placement means **one action backwards**, only for a successful Window Tidy geometry action. Capture state immediately before each placement and its expected result after. The next successful placement replaces the previous snapshot; using Undo consumes it. An external/manual move after the action invalidates the snapshot. This is not Undo Tidy and cannot unminimize arbitrary windows.
- Hidden, minimized, protected, modal, shell and exceptional windows should remain conservatively ineligible until specifically tested.

## M2 technical approach and dependencies

- Native C++20 / Win32 only; no added runtime/library. Reuse `geometry.hpp`, existing monitor Move logic, shell eligibility filtering and `WindowIdentity` checks (HWND/PID/thread).
- `MONITORINFO.rcWork` defines usable desktop bounds, not the raw monitor rectangle. Divide odd widths/heights deterministically; test left-of-primary negative coordinates, taskbar edges, portrait and mixed DPI.
- `SetWindowPos` does resizing and positioning without unintended activation/Z-order changes. Restore a maximized window before geometry changes. Do not override application minimum sizing or attempt to bypass UIPI/Win32 error 5.
- `GetWindowPlacement`/`SetWindowPlacement` handle saved show/normal state. Their placement coordinates cannot be fed directly into screen-space `SetWindowPos`; unit tests must keep these coordinate systems distinct.
- `ShowWindow` handles max/restore. Its BOOL result indicates *prior* visibility, so validate the resulting state rather than interpreting that BOOL as success.
- Extend menu command IDs independently of `kActionCount=5` hotkeys and existing 24-window bounded chooser. Do not mutate the original hotkey default binding list.
- Keep only bounded per-window placement snapshots in process memory, without saving titles. Invalid HWND/process/thread or incompatible current geometry must never restore an unrelated window.

## M3 hide/show and mandatory recovery gate

- Hide only an eligible, explicitly selected ordinary top-level window. Do not hide taskbar, shell, owned/modal, cloaked or protected windows, nor broadly force-hide every frameless Qt window.
- Add an always-accessible **Show hidden windows... / Показать скрытые окна...** tray submenu; hidden windows are omitted from the existing visible-window chooser. Track only successfully hidden windows and validate identity again on show.
- Normal Exit must restore owned hidden windows. Recovery after crash, forced termination and restart must be specified *before* enabling Hide, including stale-handle and PID-reuse safety. An in-memory map alone is insufficient; investigate a limited HKCU recovery journal with process-lifetime validation and no window names. Do not ship the feature if a safe recovery path cannot be shown.
- Showing should restore previous visibility, normal/maximized state and preferably avoid activating the window. Hide state must not interfere with existing Tidy/Undo Tidy or Undo placement.

## Test gates before milestone completion

- **M2-P01** max/restore including maximized prior state; **M2-P02** center; **M2-P03** six snap zones including odd pixels; **M2-P04** one-step undo, stale HWND and manual move; **M2-P05** app minimum size, mixed DPI and portrait; **M2-P06** shell/protected/other-user denials and no unrelated window changes.
- **M3-H01** hide/show and separate recovery submenu; **M3-H02** normal exit auto-show; **M3-H03** crash/kill/restart recovery; **M3-H04** stale HWND and shell/UIPI safety.
- Run MSVC build, `/analyze`, native logic/integration tests, PE/manifest/icon/smoke. Record real Windows 10 and Windows 11 test outcomes separately as PASS, FAIL or BLOCKED. Green CI does not constitute desktop acceptance.
- Gate M2 behind the remaining M1 acceptance and M3 Hide behind verified abnormal-exit recovery.

Microsoft Win32 references: [SetWindowPos](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-setwindowpos), [ShowWindow](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-showwindow), [GetWindowPlacement](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-getwindowplacement), [WINDOWPLACEMENT](https://learn.microsoft.com/en-us/windows/win32/api/winuser/ns-winuser-windowplacement).
