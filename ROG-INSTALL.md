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
- [ ] Each launch, one at a time, in this safe order. Return is now via Game Bar Home / Task View / relaunch — there is NO watcher that auto-returns when an app exits (v1.0.1+):
  1. **Emulation** — frontend opens; selector hides. Return via Game Bar Home (Win+G, then Home) or Task View (Win+Tab) or relaunching `rog-select.exe` (single-instance shows the hidden window). Note which route worked.
  2. **Steam** — Big Picture opens; selector hides. Return the same way. Note whether closing Big Picture alone, exiting Steam fully, or neither was needed before a Game Bar / Task View return worked.
  3. **Xbox** — Xbox app opens; selector hides. Note: opening the Xbox PC app is NOT the same as restoring the original Xbox home experience — record both behaviours separately in §6.
  4. **Windows LAST, and only if Xbox Mode is inactive in this windowed test** — selector quits and the normal desktop appears. If Xbox Mode is active, the Windows button must instead show the Win+F11 / Game Bar / Task View instruction and must NOT launch explorer or quit. Relaunch `rog-select.exe --windowed` to continue testing.
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
- [ ] Repeat the Windows-button test last, with the §6 distinction in mind: if FSE is inactive, desktop must appear cleanly, with no black screen. If FSE is active, the button must show the official exit instruction, not quit to a fake desktop. If the screen goes black, check whether `explorer.exe` is running in Task Manager and report it — do not change the shell to fix it.

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

## 6. Xbox Mode / Gaming Home integration — research findings, then device tests

Work through this only after §4 passes. Nothing in this section is claimed as working until the checklist in §6.5 passes on this device and is logged.

### 6.1 What Microsoft actually says (verified from the sources, not from this app)

- **Xbox mode / FSE is a session posture.** Microsoft Support (KB 5070297, 20 Oct 2025): "When using Xbox mode, Windows will start your gaming home app to use as a launcher" and, with "Enter Xbox mode on startup" on, "Windows limits background processes to improve performance while gaming." On handhelds, the home app is chosen in Settings > Gaming > Xbox mode, under "Choose home app"; selecting "None" turns Xbox mode off; Game Bar and Task View gain home-app entry points.
  Source: https://support.microsoft.com/en-us/servicing/os/windows/docs/2025/10/windows-gaming-full-screen-experience
- **What FSE suppresses.** Microsoft GDK handheld guidance: when a default full-screen app is configured, "Many standard Windows experiences are suppressed, such as the taskbar, some startup apps, and other services. This suppression results in more memory being available to the games." On sleep/hibernate resume, the player returns to the game/app, or to the default gaming app if games/apps were closed. Only one window is displayed at a time in Xbox mode, apps are shown fullscreen when possible, and some desktop shortcuts are disabled.
  Source: https://learn.microsoft.com/en-us/gaming/gdk/docs/gdk-dev/pc-dev/handheld/handheld-launchers
- **How to enter and exit.** Enter: Game Bar (Win+G or Xbox button) > Settings, Task View (Win+Tab), or Win+F11; on handhelds it starts automatically at sign-in if "Enter Xbox mode on startup" is on. Exit to desktop: Game Bar settings, Task View, or Win+F11; on touch, swipe up for Task View. Microsoft explicitly warns: on handhelds with startup mode on, "you'll be asked if you want to restart your device to take advantage of these performance optimizations when you re-enter Xbox mode after using the Windows desktop."
- **Detection API.** GDK guidance: call `IsGamingFullScreenExperienceActive()` from `<gamingexperience.h>`, linked via `windowsapp.lib`, guarded by `IsApiSetImplemented("api-ms-win-gaming-experience-l1-1-0")`. Change notifications use `RegisterGamingFullScreenExperienceChangeNotification` / `Unregister...`. SDK required: 10.0.26100.3916 or later (April 2025). ROG Select v1.0.1+ loads this API set dynamically and logs ACTIVE / INACTIVE / UNAVAILABLE at startup and on the Windows button. It does **not** link `windowsapp.lib` (classic Win32 exe), and it does **not** register for change notifications yet — that is a possible small follow-up, not a running service.
- **Launcher guidance that ROG Select already follows.** GDK: minimise launcher resources when not in the foreground, consider terminating the launcher after a successful game launch to give the game more resources, avoid the task tray (not available in FSE), and ensure an easy way to close the launcher. ROG Select hides (not minimises to tray), paints only on change, and has no tray icon, no service, and no watcher thread.

### 6.2 Preferred route: native Gaming Home registration via MSIX (expected, NOT verified)

This is the route to try first, because it is the one that makes Windows itself treat ROG Select as the home app, rather than a startup app sitting on top of Xbox Mode.

- **Mechanism, from community examples.** Playnite-FSE-Launcher: "By correctly implementing the `windows.gamingApp` extension, Windows natively recognizes the package as a selectable dashboard option" in Settings > Gaming > Full Screen Experience, and "the package includes a Custom Capability Descriptor (.SCCD) file. This officially declares the `Microsoft.appCategory.gamingHome_8wekyb3d8bbwe` capability. By declaring this capability via the package manifest, the OS treats the launcher as a native FSE environment." Its launcher then simply triggers the target app and exits, so the target inherits foreground focus "with zero background overhead."
  Source: https://github.com/leeson865/Playnite-FSE-Launcher
- FullScreenExperienceShell (driver1998) is the native proof-of-concept AnyFSE credits as inspiration: a custom shell selectable as FSE Home in Settings > Gaming > Full Screen Experience, using the same `windows.gamingApp` AppExtension contract plus a custom capability.
  Source: https://github.com/driver1998/FullScreenExperienceShell
- **What is in this repo.** `packaging/msix/Package.appxmanifest` (template) + `packaging/msix/README-MSIX.md`. It declares the `windows.gamingApp` extension and the `uap4:CustomCapability Microsoft.appCategory.gamingHome_8wekyb3d8bbwe`, with the native `rog-select.exe` as a full-trust entry point. It ships **no real .SCCD** — that file must be authorised by the capability owner (Microsoft) and cannot be fabricated here.
- **Signing / install steps to test on device (elevation only for install, never for running):**
  1. Record the before-state: current home app, Xbox mode on/off, "Enter Xbox mode on startup" on/off, Windows build (`winver`), Xbox app version.
  2. On device, create a self-signed developer certificate whose subject matches the manifest Publisher exactly. Trust it (Trusted People, Local Machine) — this step needs elevation; ROG Select itself must still run non-admin.
  3. Turn on Developer Mode (Settings > System > Advanced / For developers) to sideload a package with a custom capability.
  4. Copy the exact `windows.gamingApp` extension children from a working example manifest (Playnite-FSE-Launcher or FullScreenExperienceShell) into `Package.appxmanifest` if MakeAppx rejects the template. Do not guess the children and do not hide a rejection.
  5. Build with MakeAppx, sign with SignTool using that certificate, install with `Add-AppxPackage`.
  6. Open Settings > Gaming > Full Screen Experience / Xbox mode. ROG Select must appear as a selectable home app. If it does not, stop and report "MSIX Gaming Home: NOT VERIFIED" — do not work around it with a shell change.
  7. Select ROG Select as home app, enable "Enter Xbox mode on startup" only if the user asks, reboot, and run the §6.5 checklist.
  8. Test whether Developer Mode can be turned back off after install (Playnite's example says yes for its package; verify for this one, don't assume).
  9. Uninstall normally if needed: Start menu > Uninstall, or `Get-AppxPackage *ROGSelect* | Remove-AppxPackage`, then re-select Xbox as home app (see §8).

### 6.3 Fallback bridge: AnyFSE with a custom executable (expected, NOT verified)

Use this only if §6.2 proves too complex or fails on this device. It does not change ROG Select's code, and ROG Select must not depend on AnyFSE staying resident.

- **What AnyFSE is.** "The AnyFSE Home application aims to give users the ability to use their favorite launchers as Home applications for Gaming Full Screen Experience mode." It "can be selected as Home application for full screen experience and will execute users favourite launchers like Playnite, Steam Big Picture mode, LaunchBox, etc. in full screen experience mode (Xbox mode)" and supports "a custom executable or other installed native Gaming Home application."
  Source: https://github.com/ashpynov/AnyFSE
- **How it works (and why it is not a permanent bridge).** AnyFSE's own flow: Windows starts AnyFSE as fullscreen home app, AnyFSE reads configuration and starts the selected launcher, shows a splash, waits until the launcher's main window is detected, then "Close splash screen and exit." Same when executed from Game Bar. So the bridge exits after handoff; it is not a monitoring service for ROG Select.
- **Steps to test on device:**
  1. Record the before-state as in §6.2 step 1. Download AnyFSE only from its GitHub releases / Microsoft Store listing — do not use a repackaged copy.
  2. Run `AnyFSE.Installer.exe` as administrator (its installer requires admin; ROG Select itself does not). Note: AnyFSE's installer "Enable[s] developer mode to allow CustomCapability 'Microsoft.appCategory.gamingHome'", "install[s] temp certificate to Trusted People to install package", then "After installation it remove[s] temp certificate and turn[s] off developer mode." Record whether that happened on this device.
  3. In AnyFSE settings (Start menu > AnyFSE > Configure task), choose the custom-executable option and browse to the real `rog-select.exe` path. Do **not** select "run as administrator" for ROG Select.
  4. **Button remapping OFF, explicitly.** AnyFSE can redefine Armoury Crate / Command Center / Library buttons, and its ACSE Filter service "monitors the ASUS Optimization process and injects a small filter" (`AnyFSE.ACSEFilterHook.dll` via VirtualAllocEx / WriteProcessMemory / CreateRemoteThread / LoadLibrary) and is installed as an auto-started service that reinjects on restart. That is DLL injection into ASUS Optimization, and it is the most likely reason Defender flags AnyFSE (`Trojan.Wacatac!ml` false positives, per AnyFSE's own README). For ROG Select: do NOT enable remapping, do NOT install/start the ACSE Filter service, do NOT disable Armoury Crate or ASUS Optimization to make room for it. If remapping is already on from an earlier setup, turn it off and verify the service is stopped/removed (`sc stop ACSEFilterInjector` / `sc delete ACSEFilterInjector` only if it was installed for this test and the user approves removal).
  5. In Settings > Gaming > Full Screen Experience, select AnyFSE as home app, enable "Enter full screen experience on startup" only if the user asks, reboot, and run §6.5.
  6. Uninstall path if needed: AnyFSE uninstaller, plus manual cleanup if broken: `Get-AppxPackage *AnyFSE* | Remove-AppxPackage`, delete `%ProgramFiles%\AnyFSE`, remove the uninstall registry key — only as documented in AnyFSE's own README, and only with user approval. Then re-select Xbox as home app (§8).
- **Security notes for the report.** No Defender exclusion was added for ROG Select. Any Defender flag on AnyFSE itself must be reported verbatim, not dismissed as "just a false positive" without noting AnyFSE's own explanation above. No ASUS button remapping, no filter service, no ASUS Optimization disabling, no admin-by-default running of ROG Select or games.

### 6.4 What is verified, expected, and still requires device testing

| Claim | Status |
|---|---|
| ROG Select source compiles under MSVC (CI, after the v1.0.1 `PROCESSENTRY32W` fix) and produces a portable ZIP | Verified in CI once the v1.0.1 run is green — check the run link in the hand-back, do not assume it |
| Assets are the user's supplied background, four cards, and two sounds, at the sizes in README | Verified here (background 1672×941; cards 1086×1448 RGBA; navigate ~4 s, select ~1 s) |
| App detects FSE via `IsGamingFullScreenExperienceActive()` when the API set is present, logs ACTIVE/INACTIVE/UNAVAILABLE | Implemented in code, requires device run to see a real ACTIVE log line |
| Windows button gates on active FSE and shows Win+F11 / Game Bar / Task View instruction instead of launching explorer | Implemented in code, requires device test to confirm the instruction matches what Windows actually does on this build |
| Return via Game Bar Home / Task View / relaunch, with no watcher thread, no polling, no service | Implemented in code, requires device test (§6.5) |
| MSIX `windows.gamingApp` + `gamingHome` capability makes ROG Select appear in the home-app dropdown | Expected from Playnite / FullScreenExperienceShell examples — REQUIRES device test, template only |
| AnyFSE custom-executable bridge launches ROG Select as home app and then exits | Expected from AnyFSE docs — REQUIRES device test, remapping OFF |
| Xbox Mode background-process suppression, Explorer suppression, and RAM benefit are preserved with ROG Select as home app | REQUIRES device test — do not claim until §6.5 cold-boot + RAM comparison passes |
| FSE optimisations survive a Windows desktop visit (or return after the prompted restart) | REQUIRES device test — Microsoft warns a restart may be prompted on re-entry; test it, don't assume it |
| Xbox PC app launch from ROG Select equals restoring the original Xbox home experience | REQUIRES device test — treat as different until proven otherwise |
| Armoury Crate, ASUS Command Center, and performance modes work unchanged under ROG Select as home app | REQUIRES device test |
| Sleep/resume returns to the right place (game/app, or home app if closed) | REQUIRES device test — GDK says resume returns to the game/app, or to the default gaming app if closed; verify for ROG Select |

Until §6.5 is completed with evidence, every report, README line, and message about this project must keep the wording: **"Xbox Mode optimisations: unverified on this hardware."**

### 6.5 Device validation checklist (Codex — run all of these, pass/fail each, with log lines)

Do §6.2 first; if it fails, do §6.3; if both fail, stop at §6.5 item 12 (revert) and report NOT VERIFIED. Do not try a shell hack in between.

- [ ] **1. Cold boot directly into ROG Select.** Full shutdown, power on, sign in. Does the device go straight to ROG Select (not stock Xbox dashboard, not desktop)? Time it. Note any splash, Xbox app flash, or desktop flash first.
- [ ] **2. Background processes and Explorer suppression.** In FSE (before any desktop visit), open Task Manager (via Game Bar / Task View route if needed, or Ctrl+Shift+Esc after a desktop visit for comparison only) and record: is `explorer.exe` running or suppressed? List the top background processes by RAM. Compare with the stock Xbox home cold-boot baseline you recorded in §6.2 step 1 / §6.3 step 1. Do not claim "suppressed" from a single screenshot — record process names and RAM numbers.
- [ ] **3. Available RAM compared with stock Xbox Mode after a cold restart.** Same measurement as item 2, twice: (a) stock Xbox home, cold restart, idle 2 minutes, record free/available RAM; (b) ROG Select as home, cold restart, idle 2 minutes, record free/available RAM. Report both numbers and the difference. A small difference is not "preserved optimisations" by itself — report the numbers, and whether background-process counts also match.
- [ ] **4. Controller, D-pad, touch and audio operation in FSE.** D-Pad and left stick move focus, A launches, B does nothing, touch tap launches, swoosh on move, clang on select. Note any input that only works after a desktop visit, or any controller that XInput does not see as controller 0 in FSE.
- [ ] **5. Steam Big Picture launching.** From ROG Select in FSE, Steam Big Picture opens fullscreen, controller works in it, and Game Bar Home / Task View returns to ROG Select. Note if Steam opens in desktop mode instead, or if it needs a desktop visit first.
- [ ] **6. Emulator launching.** From ROG Select in FSE, the configured frontend opens fullscreen. Note the exact frontend and path used. Return via Game Bar Home / Task View. Note if the frontend opens windowed, needs a desktop visit, or loses controller focus.
- [ ] **7. Xbox PC app behaviour.** From ROG Select in FSE, press Xbox. Record: does the Xbox PC app open, does it open as a fullscreen app, and — separately — does Game Bar Home return to ROG Select, and does selecting the original Xbox home (if still available) behave differently? Do not conflate "Xbox app opened" with "Xbox home restored" in the report.
- [ ] **8. Game Bar Home returning to the selector.** From a launched app/game, Win+G / Xbox button > Home must return to ROG Select. Also test Task View (Win+Tab, long-press Xbox button, swipe up on touch) > select ROG Select. Record which routes worked and which did not.
- [ ] **9. Sleep and resume.** With ROG Select in foreground, sleep the device, resume — where do you land? With a game/app in foreground, sleep, resume — where do you land? With all apps closed, sleep, resume — does it return to ROG Select (the default gaming app) as GDK describes? Record each case.
- [ ] **10. Switching to Windows desktop.** From ROG Select in FSE, press Windows. The app must show the Win+F11 / Game Bar / Task View instruction (not quit to a fake desktop). Follow that instruction: does the desktop appear cleanly? Then re-enter Xbox mode: are you prompted to restart to regain optimisations? If yes, restart and re-run items 1–3. Record the prompt verbatim and whether optimisations (RAM/processes) returned after restart, or only after a cold boot.
- [ ] **11. ASUS Command Center and performance modes.** Open Armoury Crate / Command Center from its normal button/route while ROG Select is home app. Change performance modes (e.g. Silent / Performance / Turbo, as labelled on this device). Do modes apply, do fans/power change as expected, and does ROG Select stay usable afterwards? If AnyFSE remapping was ever enabled, confirm it is OFF and ASUS Optimization was not disabled for this test.
- [ ] **12. Reverting to the original Xbox home application.** Settings > Gaming > Full Screen Experience / Xbox mode > Choose home app > Xbox. Reboot. Confirm the device boots to the stock Xbox dashboard again, with the same cold-boot behaviour as the baseline. If you installed an MSIX package, uninstall it (`Get-AppxPackage *ROGSelect* | Remove-AppxPackage`); if you used AnyFSE, follow its uninstall path in §6.3 step 6. Confirm no ROG Select startup shortcut, service, or filter service remains. This item must pass even if every other item failed — rollback is not optional.
- [ ] **13. FSE log lines.** Paste `rog-select.log` lines showing `FSE state (startup)` and `FSE state (Windows button)` for at least one FSE-active run and one FSE-inactive run (if you can produce the latter). If the log says UNAVAILABLE, record the Windows build and SDK/API-set reason — do not guess that FSE was active.

### 6.6 If integration fails

Report "Xbox Mode preservation: NOT VERIFIED", leave the device on the stock Xbox home app (+ optional Startup shortcut from §5 only if the user asks), and list exactly which §6.5 items failed and what the failure looked like. Do not try, in order: shell replacement, Winlogon Shell registry change, Group Policy logon app, a background watcher/service to force focus, DLL injection, button remapping, Defender exclusion, or disabling ASUS Optimization / Armoury Crate. Those are out of scope for this project, even if another community guide suggests them.

## 7. Report back (required format)

Your hand-back must include, with no hand-waving:

- What was implemented / changed (files, `selector.ini` values you set).
- What was tested (§4a, §4b, §4c checklists, pass/fail each).
- What is functioning, what remains incomplete, and any compromises.
- `rog-select.log` excerpts for any failure.
- Xbox Mode status in one line: VERIFIED (with evidence) or NOT VERIFIED, plus the §6.5 cold-boot RAM numbers (stock vs ROG Select).
- Rollback confirmation: how to undo everything you did (§5 shortcut deletion + folder deletion + any §6 setting restored + MSIX uninstall and/or AnyFSE uninstall if either was tried).

## 8. Rollback (if anything goes wrong)

1. Windows button (or Alt+F4 in windowed mode) to exit to desktop.
2. If stuck fullscreen: Ctrl+Alt+Del → Task Manager → end `rog-select.exe`.
3. Delete the Startup shortcut if you added one (`shell:startup`).
4. Delete the ROG Select folder.
5. Restore any §6 Gaming Home / Xbox Full Screen Experience setting you changed, to its before-state noted in §6.2 step 1. Re-select Xbox as home app.
6. If you installed the MSIX package: `Get-AppxPackage *ROGSelect* | Remove-AppxPackage`. If you used AnyFSE: follow its uninstall path in §6.3 step 6, and confirm no ACSE Filter service remains.
7. Reboot. Nothing else remains: no service, no shell change, no driver change, no filter service.
