# ROG-INSTALL.md — for Codex running locally on the Ally X

This file is written for the coding agent on the device itself. Follow it in order. Do not skip the windowed test. Do not claim Xbox Mode behaviour is preserved until the checks in §6 are actually done and logged.

## Hard rules for this device

- **Do not** replace Explorer as the system shell. Do not edit `HKLM\...\Winlogon\Shell` or any shell registry value.
- **Do not** disable Defender, SmartScreen, or any security protection.
- **Do not** install a controller remapping service, a DLL-injection helper, or a background service.
- **Do not** modify Windows system files.
- **Do not** change the user's ROM library, saves, BIOS, or frontend installs. ROG Select only *launches* the frontend named in `selector.ini`; it never scans or writes to a library.
- Keep every change inside the ROG Select folder, except: (a) the optional Startup shortcut in §5, and (b) `selector.ini` edits. If a step would touch anything else, stop and ask the user first.
- ROG Select is personal-use, single-device software. The card art is the user's supplied art.

## 0. Get the code

```powershell
# Pick a folder you control, e.g.:
cd C:\Users\ciara\Documents
git clone https://github.com/ciaranf3308-star/rog-select.git
cd rog-select
git log --oneline -3
git status
```

If the repo is not there yet, or the tag build has not finished, say so plainly and use the local folder as handed over — do not invent a download link.

## 1. Verify the delivered files

```powershell
dir
dir assets
# Expected:
#   rog-select.exe (only after a build, §2)
#   selector.ini, README.md, ROG-INSTALL.md, build.bat, src\main.cpp
#   assets\background.png, windows.png, emulation.png, xbox.png, steam.png
#   assets\navigate.mp3, assets\select.mp3
```

Open `selector.ini` and confirm the four card images and the background/sound paths point at files that exist. If any asset is missing, the app draws a fallback card — note it in your report, do not silently substitute other art.

## 2. Build (if no exe was delivered)

In a Developer Command Prompt for VS 2022 (x64), from the repo folder:

```
build.bat
```

Expected output ends with `BUILD OK: rog-select.exe`. If `cl` is not found, open the Developer Command Prompt from the Start menu (search "Developer Command Prompt") and run `build.bat` again — do not install a new toolchain without asking.

## 3. Configure — this is the step that makes it real

Open `selector.ini` in a text editor and set:

1. **Emulation `path`** — the exact frontend exe on this device. Do not guess. Check what the user actually uses (ES-DE, Playnite, etc.) by looking at their existing shortcuts/installs, and confirm the path exists before saving:
   ```powershell
   Test-Path 'C:\path\to\frontend.exe'
   ```
   If you cannot find it, stop and ask the user — do not leave a wrong path in place and do not pick a random exe.
2. **Steam `path`** — leave blank if Steam is in a standard location (auto-detect), otherwise set the real `steam.exe` path and verify with `Test-Path`.
3. **Xbox `path`** — try the default AUMID first. If Xbox fails to launch, find the real one:
   ```powershell
   Get-StartApps | Where-Object { $_.Name -match 'Xbox' }
   ```
   Replace the `path` line with `shell:AppsFolder\<the AUMID shown>`, save, and retest.
4. **Sounds** — if navigate/select feel reversed, swap the two `sound_*` lines. Note the swap in your report.
5. Leave `default_focus=1` (Emulation, matching the hero) unless the user asks otherwise.

## 4. Test in this exact order

### 4a. Windowed first (safe)

```
rog-select.exe --windowed
```

Check, in order, and note pass/fail for each:

- [ ] Window opens 1280×720, background + four cards visible, header `ROG SELECT`, title `Choose your destination`.
- [ ] Keyboard: Left/Right move focus (Emulation starts focused, grows with green glow), Enter launches, Esc does nothing.
- [ ] Mouse: hover moves focus, click launches.
- [ ] Touch: tap a card to launch (if the device is in handheld/tablet posture).
- [ ] Controller: D-Pad / left stick move, A launches, B does nothing.
- [ ] Sounds: a swoosh on move, a clang on select. If silent, check `assets\*.mp3` exist and `sound` is not `off` in `selector.ini`.
- [ ] Each launch, one at a time, in this safe order:
  1. **Emulation** — frontend opens; close the frontend; selector must reappear.
  2. **Steam** — Big Picture opens; note whether closing Big Picture alone returns to the selector, or only exiting Steam fully does (see README limitations).
  3. **Xbox** — Xbox app opens; note whether return works, and what `GamingApp.exe` did in Task Manager.
  4. **Windows LAST** — selector quits and the normal desktop appears. Relaunch `rog-select.exe --windowed` to continue testing.
- [ ] Bad-path test: temporarily set Emulation `path` to a nonexistent file, relaunch, press Emulation — an on-screen error must appear and the selector must stay up. Restore the real path afterwards.
- [ ] Check `rog-select.log` in the same folder after each test. It must show starts, launches, and any failures. Paste relevant log lines into your report — do not summarise failures away.

Close the windowed app with Alt+F4 or the window close button when done with this stage.

### 4b. Fullscreen

```
rog-select.exe
```

- [ ] Fullscreen covers the display at the device's native resolution, no taskbar, no window chrome.
- [ ] Cards are in one row, readable at handheld distance, focused card clearly larger.
- [ ] Repeat the controller test in §4a. If the built-in controller does not move focus, record the exact behaviour (does XInput see it as controller 0? does another app need to be closed first?) — do not install a remapper to work around it.
- [ ] Repeat the Windows-button test last: desktop must appear cleanly, with no black screen. If the screen goes black, check whether `explorer.exe` is running in Task Manager and report it — do not change the shell to fix it.

### 4c. Performance / interference spot-check

- [ ] With the selector idle fullscreen for 2 minutes, Task Manager shows near-zero CPU for `rog-select.exe` and modest RAM (tens of MB, not hundreds).
- [ ] Armoury Crate still opens and still changes power modes. Note any mode that stops working while the selector is running.
- [ ] No new service, scheduled task, or startup entry was created by the app itself (it creates none). If you add a Startup shortcut in §5, that is the only one, and it must be listed in your report.

## 5. Optional startup (only if the user asks)

1. Right-click `rog-select.exe` → Create shortcut.
2. `Win+R` → `shell:startup` → move the shortcut there.
3. Reboot, confirm the selector appears, confirm the Windows button still exits to desktop.
4. To undo: delete that shortcut. That is the whole uninstall for startup.

Do **not** set ROG Select as a shell, a logon app via Group Policy, or a Gaming Home replacement in this step. Those are §6, and §6 is unverified.

## 6. Xbox Mode / Gaming Home — UNVERIFIED, test separately, report honestly

This is the part that must not be oversold. Work through it only after §4 passes, and report exactly what happened:

1. Note the current state first: is Xbox Full Screen Experience on in Settings → Gaming, and what does the stock dashboard do on boot today?
2. Try the AnyFSE route the user mentioned (community Gaming Home tooling) **in a reversible way**: document the exact tool, version, and setting you changed, before you change it. Take a screenshot or note of the before-state.
3. Test: does the device boot / return to ROG Select instead of the stock dashboard? Do background processes behave as they did under stock Xbox Mode, or does something visibly change (fan, power mode, Armoury Crate, Xbox app behaviour)?
4. If AnyFSE / Gaming Home registration does **not** work, or you cannot verify background-process behaviour: **stop there**. Report "Xbox Mode preservation: NOT VERIFIED" and leave the device on the stock dashboard + optional Startup shortcut from §5. Do not try a shell replacement, a registry shell hack, or a service as a workaround.
5. If it does work: document the exact registration steps, the tool version, and the before/after behaviour, so the main README can be updated with evidence — not with a claim.

Until §6 is completed with evidence, every report, README line, and message about this project must keep the wording: **"Xbox Mode optimisations: unverified on this hardware."**

## 7. Report back (required format)

Your hand-back must include, with no hand-waving:

- What was implemented / changed (files, `selector.ini` values you set).
- What was tested (§4a, §4b, §4c checklists, pass/fail each).
- What is functioning, what remains incomplete, and any compromises.
- `rog-select.log` excerpts for any failure.
- Xbox Mode status in one line: VERIFIED (with evidence) or NOT VERIFIED.
- Rollback confirmation: how to undo everything you did (§5 shortcut deletion + folder deletion + any §6 setting restored).

## 8. Rollback (if anything goes wrong)

1. Windows button (or Alt+F4 in windowed mode) to exit to desktop.
2. If stuck fullscreen: Ctrl+Alt+Del → Task Manager → end `rog-select.exe`.
3. Delete the Startup shortcut if you added one (`shell:startup`).
4. Delete the ROG Select folder.
5. Restore any §6 Gaming Home / Xbox Full Screen Experience setting you changed, to its before-state noted in §6 step 2.
6. Reboot. Nothing else remains: no service, no shell change, no driver change.
