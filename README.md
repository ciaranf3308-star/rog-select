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
- **Idle cost is effectively zero.** The message loop blocks (`GetMessage`). The only wake-up is an 80 ms XInput poll, and painting happens only on focus change, resize, launch, or return.

## Xbox Mode — read this first, honestly

**It is NOT verified that this selector preserves Xbox Mode's Windows background-process optimisations, and this README does not claim that it does.**

Xbox Full Screen Experience / Gaming Home integration goes through Microsoft's Gaming Home mechanism (AnyFSE is the community route people point at first). Whether ROG Select can be registered that way, whether it keeps the same background-process behaviour as the stock Xbox dashboard, and whether Armoury Crate / ASUS power modes behave identically under it, can only be established by testing on the actual Ally X.

Until that test is done and logged:

- Treat ROG Select as a **fullscreen selector app you launch (optionally at startup)**, not as a proven Gaming Home replacement.
- Do not present it to anyone else as "Xbox Mode, but with a better menu" until the on-device checks in `ROG-INSTALL.md` pass.
- If Gaming Home registration fails, the safe fallback is the stock Xbox Full Screen Experience (see Rollback below) — not a shell hack.

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
| Each card's `return_process` | Process name to watch for return-to-selector (see below). |

**Emulation:** leave `path` blank and the button will show an error and log it — it will never guess a frontend or launch the desktop instead. Set it to your actual frontend exe.

**Xbox:** the default AUMID is `Microsoft.GamingApp_8wekyb3d8bbwe!Microsoft.Xbox.App`. If launching Xbox fails on your device, find the real one in PowerShell:
```powershell
Get-StartApps | Where-Object { $_.Name -match 'Xbox' }
```
and put `shell:AppsFolder\<that AUMID>` in `selector.ini`.

**Steam:** leave `path` blank to auto-detect `C:\Program Files (x86)\Steam\steam.exe` / `C:\Program Files\Steam\steam.exe`, or set it explicitly.

**Sounds:** navigate = Gentle swoosh (menu moving), select = Brief metallic clang (selection). If they feel reversed on the device, swap the two `sound_*` lines in `selector.ini` — no rebuild needed.

## Controls

- **Controller (XInput):** D-Pad or left stick to move, A to launch. B and Guide do nothing, deliberately — no accidental exit to desktop.
- **Keyboard:** Left/Right (or Up/Down) to move, Enter/Space to launch, 1–4 to focus a card. Esc does nothing, deliberately.
- **Mouse / touchscreen:** hover or tap to focus, click/tap to launch.
- **No auto-launch, no timeout.** Nothing launches until you confirm. Windows is never opened by accident.

## Returning to the selector

- **Direct exe launches (Emulation, Steam when not already running):** the selector hides, waits for the launched process to exit, then reappears fullscreen.
- **Steam when already running:** the selector watches for `steam.exe` to exit. Closing Big Picture alone while Steam stays in the tray does **not** yet bring the selector back — exit Steam fully, or relaunch `rog-select.exe` (single-instance handling brings the hidden window back).
- **Xbox (store app):** the selector watches for `GamingApp.exe`. If that process never appears (the URI forwards elsewhere), the selector stays hidden rather than popping over the Xbox app. Relaunching `rog-select.exe` brings it back.
- **Windows:** the selector quits. That *is* the switch to desktop — there is nothing to return from, by design. Relaunch the exe to get the selector back.

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
That produces `rog-select.exe` in the repo folder. No packages to download — it links only Windows system libraries (`d2d1`, `windowscodecs`, `dwrite`, `Xinput`, `winmm`, `user32`, `gdi32`, `shell32`).

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
```
