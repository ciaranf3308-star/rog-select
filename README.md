# ROG Select

A lightweight, native Windows boot selector for the **ASUS ROG Xbox Ally X (Ryzen AI Z2 Extreme)**.

Four large, controller-friendly buttons, full-screen at 1080p:

1. **Windows** — switch to the normal Windows desktop.
2. **Emulation** — launch your configurable emulation frontend.
3. **Xbox** — launch the Xbox PC experience.
4. **Steam** — launch Steam Big Picture.

Design target: the supplied hero image (`ROG SELECT — Choose your destination`, single row, Emulation focused). The clean background plate and the four card images are the user's own supplied assets, in `assets/`.

## What this is, and what it is not

- **Native C++ only.** One Win32 + Direct2D source file (`src/main.cpp`). No Electron, browser engine, Python runtime, or web server.
- **No background service, no telemetry, no library scanning, no automatic updates.** The app does one thing: show four buttons, launch one thing, and come back when that thing exits (see limitations below).
- **No system changes.** It does not modify Windows system files, does not replace Explorer as the system shell, does not disable Defender or any security protection, and does not install controller remapping or DLL-injection services.
- **Idle cost is effectively zero, including when hidden.** The message loop blocks (`GetMessage`). While visible, the only wake-up is an 80 ms XInput poll, and painting happens only on focus change, resize, launch, or return. In v1.0.3, that timer is stopped when the selector hides after a launch and resumed when it is shown again — a hidden selector has no timer wake-ups at all. There is no watcher thread, service, or extra process. An optional `exit_on_launch=on` test flag (selector.ini, off by default) exits the selector entirely after a launch for 0 CPU / 0 RAM; see ROG-INSTALL for the device test that must pass before it becomes the default.
- **Display scaling (v1.0.4).** The app is now Per-Monitor V2 DPI-aware (with a system-DPI fallback), the Direct2D render target is fixed at 96 DPI so 1 D2D unit = 1 physical pixel, and `WM_DPICHANGED` resizes to Windows' suggested rect. Layout is always recomputed from the physical client rect, so at 1080p the layout is identical at 100%, 150% and 175% scaling — no clipping, no stretched cards, no missing text, and no need to change Windows scaling. `tools/layout_check.py` mirrors the card math and passes for 1920×1080, 1280×720 windowed, and 2560×1440.
- **Window title and Unicode (v1.0.4).** Window class, title, mutex, and message box are now Unicode (W) APIs with a wide title literal, replacing the mixed ANSI/Unicode handling that garbled the title. Exe path, command line, config, log, sound, and launch paths also go through W APIs (internal strings stay UTF-8, matching `ToWide`/`WideToUtf8`), so a non-ASCII user path no longer garbles.
- **Controller and touch (v1.0.4).** `WM_POINTERDOWN` coordinates are now converted from screen to client coordinates before hit-testing (mouse messages were already client coordinates — that mismatch was the touch bug).
- **Controller (v1.0.6, fixes the v1.0.5 acceptance failure).** All four XInput slots are polled every 80 ms with per-slot state, so whichever controller is actively used drives navigation — an idle virtual pad on slot 0 can no longer block the built-in pad on a later slot. A newly connected/reconnected slot records its state and does not act that poll (no phantom press); disconnection clears its state. The timer start was also fixed: the old `WM_CREATE` start read `g_hwnd` before WinMain assigned it, so it was dead code and polling silently depended on `WM_SHOWWINDOW`. The timer now takes its HWND explicitly, checks `SetTimer`'s return, and logs success or failure. Diagnostic logging (timer start, controller scan, connect/disconnect, D-pad/A activity per slot) is concise and never per-poll, so `rog-select.log` shows which failure mode occurred: timer never started, no controller seen, or controller seen but no activity. Polling still stops while hidden. No GameInput fallback: the built-in Ally X pad is an Xbox/XInput pad, and GameInput would not fix first-slot blocking, a dead timer, or Armoury Crate Desktop mode, while adding a WinRT dependency.
- **ASUS control modes (no remapping, no system change by the app).** Armoury Crate's Auto mode switches Gamepad/Desktop automatically for supported apps; a plain Win32 exe like ROG Select may be treated as a desktop app, in which case the pad emulates mouse/keyboard (right stick moves the cursor, A = Enter, RB = left-click) instead of XInput — ROG Select already handles those mouse/keyboard paths (hover moves focus, Enter launches, click launches). On Xbox Ally X models, ASUS says the system default is Gamepad mode per Xbox requirements, with Auto switching to Desktop only for certain Xbox-designated apps. If XInput navigation does not work in Auto, the controller-only test in ROG-INSTALL tells Codex to note the Control Mode shown in Command Center and, if needed, manually set Gamepad mode there for the test — that is a user action in Armoury Crate, not a change made by ROG Select, and no button remapping is involved.

## Xbox Mode — read this first, honestly

**It is NOT verified that this selector preserves Xbox Mode's Windows background-process optimisations, and this README does not claim that it does.**

Xbox Mode (Full Screen Experience, FSE) is a Windows session posture, not a theme: when a supported gaming home app is chosen in Settings > Gaming > Xbox mode / Full Screen Experience, Windows starts that home app, suppresses the taskbar, some startup apps and other components, and — with "Enter Xbox mode on startup" — limits background processes to improve gaming performance. Microsoft's own docs put the exit routes as Game Bar settings, Task View, or Win+F11, and warn that re-entering Xbox mode after a desktop visit prompts for a restart to regain the startup optimisations.

ROG Select's integration path is therefore, in order:

1. **Preferred: native Gaming Home registration via MSIX** — a package using the `windows.gamingApp` extension and the `Microsoft.appCategory.gamingHome_8wekyb3d8bbwe` custom capability (with a valid `.SCCD`), as demonstrated by Playnite-FSE-Launcher and FullScreenExperienceShell. A template manifest is in `packaging/msix/` — it is **unverified** until it is built, signed, installed on the Ally X, and actually appears in the home-app dropdown.
2. **Fallback bridge: AnyFSE** — AnyFSE as home app, with its custom-executable feature pointing at `rog-select.exe`, button remapping OFF. AnyFSE's own flow starts the launcher, waits for its main window, then closes its splash and exits, so ROG Select does not depend on a permanently running bridge. Do not enable AnyFSE's ASUS button remapping / ACSE Filter service for ROG Select — that is DLL injection into ASUS Optimization, and it is out of scope for this project.
3. **If neither works:** a plain fullscreen app launched optionally at startup. That is *not* Xbox Mode, and must not be described as such.

Until the on-device checks in `ROG-INSTALL.md §6` pass and are logged:

- Treat ROG Select as a **fullscreen selector app you launch (optionally at startup)**, not as a proven Gaming Home replacement.
- Do not present it to anyone else as "Xbox Mode, but with a better menu".
- If Gaming Home registration fails, the safe fallback is the stock Xbox Full Screen Experience (see Rollback below) — not a shell hack, not a registry shell change, not a service.

The app itself now detects FSE at startup and on the Windows button via `IsGamingFullScreenExperienceActive()` (SDK 10.0.26100.3916+, loaded dynamically; if the API set is absent, the log says UNAVAILABLE, never guessed). FSE being active does **not** prove every optimisation is still in effect after a desktop visit — that is a device test, not a return value.

## Installation (portable, no installer)

1. Unzip `ROG-Select-Windows-x64.zip` anywhere, e.g. `C:\Games\ROG-Select\` or a folder on the device you control. Keep `rog-select.exe`, `selector.ini`, and the `assets\` folder together.
2. Edit `selector.ini` — **at minimum, set the Emulation path.** See Configuration below.
3. Test windowed first, on a normal PC or on the Ally X:
   ```
   rog-select.exe --windowed
   ```
   Check keyboard, mouse, touch, and controller navigation, and that each button behaves as expected (see Testing).
4. Only then run it fullscreen:
   ```
   rog-select.exe
   ```
5. Optional startup (manual, your choice, nothing is added automatically): create a shortcut to `rog-select.exe` and put it in the Startup folder (`Win+R`, type `shell:startup`, paste the shortcut there). Remove that shortcut to stop it launching at boot.

## Configuration (`selector.ini`)

All paths are relative to the folder containing `rog-select.exe`, or absolute. Restart the app after editing.

| Setting | Meaning |
|---|---|
| `background`, `sound_navigate`, `sound_select` | Background plate and the two supplied sounds. |
| `default_focus` | 0 Windows, 1 Emulation, 2 Xbox, 3 Steam. Hero default is 1 (Emulation). |
| `sound` | `off` runs silent. |
| Each card's `image` | Card PNG. If an image is missing, the card still renders as a labelled fallback rectangle — never a blank screen. |
| Each card's `action` | `desktop`, `exe`, or `store_app` (Xbox). |
| Each card's `path` | Executable, URI, or `shell:AppsFolder\<AUMID>` for the Xbox app. |
| Each card's `arguments` | Command-line arguments, e.g. Steam's `-bigpicture`. |
| Each card's `return_process` | Legacy, ignored since v1.0.1. Return is via Game Bar Home / Task View / relaunch — no watcher runs. |

**Emulation:** leave `path` blank and the button will show an error and log it — it will never guess a frontend or launch the desktop instead. Set it to your actual frontend exe.

**Xbox:** the default AUMID is `Microsoft.GamingApp_8wekyb3d8bbwe!Microsoft.Xbox.App`. If launching Xbox fails on your device, find the real one in PowerShell:
```powershell
Get-StartApps | Where-Object { $_.Name -match 'Xbox' }
```
and put `shell:AppsFolder\<that AUMID>` in `selector.ini`.

**Steam:** leave `path` blank to auto-detect `C:\Program Files (x86)\Steam\steam.exe` / `C:\Program Files\Steam\steam.exe`, or set it explicitly.

**Sounds:** navigate = Gentle swoosh (menu moving), select = Brief metallic clang (selection). If they feel reversed on the device, swap the two `sound_*` lines in `selector.ini` — no rebuild needed.

## The Windows button (gated, by design)

- If Xbox Mode is **inactive**, pressing Windows quits the selector and the normal desktop underneath appears. That is a real desktop switch.
- If Xbox Mode is **active**, pressing Windows now sends **Win+F11 once via `SendInput`** (v1.0.4, from physical-device testing: a standalone, non-admin native program did this on the Ally X and FSE went ACTIVE → INACTIVE in ~1 s and ~5 s, no prompt, no physical keyboard). The app then polls FSE every 250 ms for at most 10 s. On INACTIVE it quits like the inactive path, so the desktop is clean. On timeout, on a failed `SendInput`, or if FSE status is unavailable, it shows a safe manual message (*"Press Win+F11, or Task View > Windows Desktop > Continue."*) and never sends a second Win+F11 — a second one would toggle back into FSE. Double-pressing Windows while a transition is underway is ignored. This is device-tested behaviour, not a Microsoft "exit FSE" API (none exists alongside the detection API). No admin, shell replacement, service, or registry change.

## Controls

- **Controller (XInput):** D-Pad or left stick to move, A to launch. B and Guide do nothing, deliberately — no accidental exit to desktop.
- **Keyboard:** Left/Right (or Up/Down) to move, Enter/Space to launch, 1–4 to focus a card. Esc does nothing, deliberately.
- **Mouse / touchscreen:** hover or tap to focus, click/tap to launch.
- **No auto-launch, no timeout.** Nothing launches until you confirm. Windows is never opened by accident.

## Returning to the selector

There is **no background watcher** in v1.0.1+. An earlier version waited for a child process to exit; that polling monitor could pop the selector over a launched app when a launcher forwards to an already-running process, and it is not how FSE is meant to work.

- **After any launch:** the selector hides and stays hidden. Nothing polls, nothing watches.
- **Return via the system's own surfaces:** Game Bar Home (Win+G / Xbox button, then Home), Task View (Win+Tab, long-press Xbox button, or swipe up on touch), or relaunching `rog-select.exe` — single-instance handling shows the hidden window.
- **Xbox:** opening the Xbox PC app is **not** the same as restoring the original Xbox home experience. Test both on device (ROG-INSTALL §6).
- **Windows:** see below — this button is now gated when Xbox Mode is active.

## Logging and fallback

- Log file next to the exe: `rog-select.log` (rotates to `rog-select.old.log` over 1 MB). It records starts, launches, missing files, and failures. No data leaves the device.
- Missing config: built-in defaults are used and the problem is logged.
- Missing card/background: fallback colours and labels are drawn, the app keeps running.
- Missing launch path: an on-screen message appears, the failure is logged, and the selector stays up.
- Direct2D failure: the app shows an error and exits cleanly rather than hanging fullscreen. In that case, check the log and the graphics driver — do not try a shell workaround.

## Testing (what was and was not tested here)

Tested without access to the Ally X:

- All supplied assets verified: background 1672×941 PNG; four cards 1086×1448 RGBA PNG (Windows, Emulation, Xbox, Steam, in that order); navigate ~4 s MP3, select ~1 s MP3.
- Source checked for balanced braces/parens and for the documented control flow; the Windows binary itself cannot be compiled on this Linux sandbox (no MSVC here), so the authoritative build is the GitHub Actions MSVC job (see below).

**Not tested, and only testable on the Ally X:** fullscreen behaviour on the handheld display, XInput on the Ally's built-in controller, Armoury Crate / power modes, Xbox app AUMID, Steam Big Picture return, and every Xbox Mode / Gaming Home claim in the section above. See `ROG-INSTALL.md` for the on-device checklist.

## Building

GitHub Actions (recommended): push a tag `v1.0.0` or run the workflow manually. It compiles with MSVC on `windows-latest`, stages the portable folder, uploads `ROG-Select-Windows-x64.zip` as an artifact, and on tags publishes a GitHub Release with the ZIP.

Locally on Windows (Developer Command Prompt, or just run the script):
```
build.bat
```
That produces `rog-select.exe` in the repo folder. No packages to download — it links only Windows system libraries (`d2d1`, `windowscodecs`, `dwrite`, `Xinput`, `winmm`, `user32`, `gdi32`, `shell32`, `ole32` for COM/WIC).

## Rollback (full, in under a minute)

Because nothing system-level was changed, rollback is deletion, not repair:

1. Close ROG Select (Windows button, or Alt+F4 in windowed test mode).
2. If you added a Startup shortcut, delete it from `shell:startup`.
3. Delete the ROG Select folder.
4. If you changed any Xbox Full Screen Experience setting while testing, turn it back on in Windows Settings → Gaming → Xbox Full Screen Experience (or the equivalent setting on your build).
5. Reboot if you want a clean state. No registry, shell, service, or driver change remains to undo.

If the selector ever appears stuck fullscreen on the device: in windowed test mode use Alt+F4; fullscreen, use the Windows button to exit to desktop, or Ctrl+Alt+Del → Task Manager → end `rog-select.exe`. The log will say what it was doing.

## Project layout

```
src/main.cpp                 Single-file native app
selector.ini                 All configurable paths, sounds, focus
assets/background.png        Clean full-screen background plate
assets/windows.png           Windows card (1086x1448)
assets/emulation.png         Emulation card
assets/xbox.png              Xbox card
assets/steam.png             Steam card
assets/navigate.mp3          Focus-move sound
assets/select.mp3            Confirm sound
build.bat                    Local MSVC build
.github/workflows/build-release.yml   CI release build + ZIP
ROG-INSTALL.md               Precise on-device steps for Codex
packaging/msix/              UNVERIFIED Gaming Home MSIX template + signing notes
```
