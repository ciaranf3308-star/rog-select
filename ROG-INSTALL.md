# ROG-INSTALL — on-device steps for Codex (ROG Xbox Ally X)

ROG Select v1.0.4 is a four-button checkpoint: boot, choose Windows / Emulation / Xbox / Steam, play. **This test must verify the Windows card, scaling, controller, touch, audio, ES-DE, Steam, and Xbox before attempting AnyFSE integration.** Do not attempt Step 3 (AnyFSE) until every item in Step 2 passes. Test the standalone EXE first, before changing Xbox Mode at all.

## Hard rules — do not break these to make a test pass

- Never replace Explorer as the Windows shell, never edit the shell registry, never add a Defender exclusion, never disable antivirus.
- Never disable ASUS Optimization, Armoury Crate, or ASUS Command Center. Never remap ASUS buttons. Never install AnyFSE's ACSE Filter service (it injects a DLL into ASUS Optimization).
- Never run ROG Select or games as administrator by default. Elevation is only for installing a package / AnyFSE, never for running the selector.
- Never claim Xbox Mode optimisations are preserved until the cold-boot comparison in Step 4 passes with numbers.
- If anything in Steps 1–3 fails, stop, report it, and leave the device on stock Xbox home (Step 6). Do not try a shell hack, a background watcher, or a service as a workaround.

## What is unverified until this device test passes

Xbox Mode background-process suppression and RAM benefit with ROG Select as home app; MSIX Gaming Home registration; AnyFSE bridge behaviour on this device; Game Bar Home return after `exit_on_launch=on`; sleep/resume landing spot; Xbox PC app vs original Xbox home behaviour. The v1.0.4 Windows-card switch, scaling, controller, and touch fixes are themselves unverified until Step 2 passes — they were built from the v1.0.3 device findings, not tested on the device yet. Everything else below is code you can check in the repo or in `rog-select.log`.

## Step 0 — Record the stock baseline (before touching anything)

1. Note Windows build (`winver`), Xbox app version, and Settings > Gaming > Xbox mode settings: home app, "Enter Xbox mode on startup" on/off.
2. Full shutdown, power on, sign in, wait 2 minutes idle in the stock Xbox dashboard.
3. Record: free/available RAM (Task Manager > Performance > Memory), top background processes by RAM, and whether `explorer.exe` is running. This is the **stock baseline** — every later comparison uses it, under the same idle conditions.
4. Note Armoury Crate / Command Center behaviour and the current performance mode.

## Step 1 — Get v1.0.4 and configure it

1. Download the v1.0.4 portable ZIP from the GitHub release (see README) and unzip it to a folder, e.g. `C:\Users\<you>\ROG-Select\`.
2. Check the folder contains: `rog-select.exe`, `selector.ini`, `README.md`, `ROG-INSTALL.md`, and `assets/` with `background.png`, `windows.png`, `emulation.png`, `xbox.png`, `steam.png`, `navigate.mp3`, `select.mp3`.
3. Edit `selector.ini` (Notepad is fine):
   - `[emulation] path=` — put the exact path to ES-DE (the emulation frontend for this test). Leave it blank and the button will show an error instead of guessing — that is by design.
   - `[steam] path=` — leave blank to auto-detect Steam, or put the exact path. Arguments stay `-bigpicture`.
   - `[xbox] path=` — verify with `Get-StartApps` in PowerShell; the default AUMID is `shell:AppsFolder\Microsoft.GamingApp_8wekyb3d8bbwe!Microsoft.Xbox.App`.
   - `[general] default_focus=1` (Emulation), `sound=on`, and **`exit_on_launch=off`** — leave it off for the first install. It is a later test only (Step 5).
4. Do not add a Startup shortcut yet, do not change Xbox Mode settings yet.

## Step 2 — Standalone test first (no Xbox Mode change)

Do every item below, in order, before AnyFSE. If any item fails, stop and report it — do not move to Step 3.

1. Run `rog-select.exe --windowed` from the folder. Check: hero background, single row of 4 cards, Emulation focused/enlarged, title "Choose your destination", window title in Task View / Alt-Tab reads exactly "ROG Select" (not garbled).
2. **Scaling — primary test at the device's current 175% (no scaling change required for normal use).** Run fullscreen (no `--windowed`) at 175%: all four cards fully visible, card aspect ratio not stretched, title, subtitle, footer "(A) Launch | (+) Move", and status bar fully visible, nothing clipped on the right or bottom. Tap/hover hit-testing must match the visuals (tapping a card launches that card, tapping between cards does nothing). Optional, only if you choose: repeat at 100% and 150% in Settings, then restore 175%. The app must work at 175% without the user changing scaling.
3. **Controller:** D-Pad and left stick move focus (swoosh on move), A selects (clang on select), B and Esc do nothing. Test a controller that is NOT slot 0 if one is available (e.g. a second pad, or the Ally's own controller after another pad was connected first) — navigation must work without assuming slot 0. **Reconnection:** disconnect / switch off the controller, reconnect / switch it on, and navigate again without restarting ROG Select. No phantom selection on reconnect.
4. **Touch:** in windowed mode first (this is where the old screen/client bug showed), tap each card and confirm the tapped card launches — not the card to its right/below. Repeat fullscreen. Mouse hover/click must still work.
5. **Audio:** swoosh on every focus move, clang on every selection, nothing else. If `sound=off` is set, silence. If a sound file is missing, the selector must still work (log only).
6. One at a time, in this order:
   - **ES-DE (Emulation)** — ES-DE opens, selector hides. Navigate ES-DE with the controller, exit ES-DE, then return to ROG Select via Game Bar Home (Win+G, then Home) or Task View (Win+Tab / long-press Xbox / swipe up) or by relaunching `rog-select.exe` (single-instance shows the hidden window). Record which route worked. There is no auto-return on app exit — that is by design, no watcher.
   - **Steam** — Big Picture opens, selector hides. Return the same way. Note if Steam opens in desktop mode instead, and whether the controller works in Big Picture.
   - **Xbox** — Xbox app opens, selector hides. Note separately: this is the Xbox PC app, not the original Xbox home experience.
   - **Windows — the v1.0.4 test, do this carefully, once:** check the log says FSE ACTIVE first. Press Windows once and wait up to 10 seconds. Expected: status shows "Switching to Windows desktop...", FSE becomes INACTIVE, and the desktop appears cleanly (selector quits). Time it. Then press Windows a second time quickly in a separate run and confirm it does NOT toggle back into Xbox Mode — only one Win+F11 is sent, and double-pressing while switching is ignored. If the switch does not complete in 10 s, the selector must show the manual message (*"Press Win+F11, or Task View > Windows Desktop > Continue."*) and must NOT retry on its own. If FSE status is UNAVAILABLE, the selector must show the manual message and must NOT send Win+F11 blindly.
   - **Windows when already inactive:** relaunch ROG Select on the desktop (FSE inactive), press Windows, and confirm the selector quits straight to the desktop (existing behaviour, no Win+F11 needed).
7. Check `rog-select.log` beside the exe: it must show `FSE state (startup)` and, after the Windows test, `FSE state (Windows button)` as ACTIVE / INACTIVE / UNAVAILABLE, plus a "Win+F11 sent once" line and either a "transition succeeded" line (with milliseconds) or a "transition failed" line. Paste those lines in the report. If it says UNAVAILABLE, record the Windows build — do not guess FSE was active.
8. Idle check: with the selector hidden after a launch, Task Manager should show `rog-select.exe` at 0% CPU. Its controller polling timer is stopped while hidden and resumed on show — no timer wake-ups, no service, no extra process. (RAM while hidden is not zero; that is what `exit_on_launch` tests in Step 5.)
9. Failure checks: blank Emulation path must show an on-screen error and keep the selector up. Missing asset must show a labelled fallback card, never a blank screen.
10. Only if items 1–9 pass: run `rog-select.exe` fullscreen (no `--windowed`) once, repeat the four buttons, and confirm the same behaviour at 1080p. Then, and only then, move to Step 3.

## Step 3 — First Xbox Mode test: AnyFSE custom executable (not MSIX)

Only after Step 2 passes in full. MSIX is research only — do not build, sign, or test the MSIX package in `packaging/msix/`. That template has no real .SCCD and is deferred, not part of v1.0.4. The first Xbox Mode test uses AnyFSE, if appropriate, because it is the reversible bridge with the least new code.

1. Download AnyFSE only from its GitHub releases / Microsoft Store listing. Record its version.
2. Run its installer as administrator (its installer requires admin; ROG Select itself does not). Note: AnyFSE's installer enables Developer Mode for the gamingHome capability, installs a temp certificate to Trusted People, then removes the cert and turns Developer Mode off. Record whether that happened here. Any Defender flag on AnyFSE must be reported verbatim.
3. In AnyFSE settings (Start menu > AnyFSE > Configure), choose the custom-executable option and browse to the real `rog-select.exe` from Step 1. Do NOT tick "run as administrator".
4. **Button remapping OFF, explicitly.** Do not enable AnyFSE remapping, do not install/start the ACSE Filter service, do not disable Armoury Crate or ASUS Optimization. If remapping is already on from an earlier setup, turn it off and confirm no ACSE service remains.
5. Settings > Gaming > Full Screen Experience / Xbox mode > Choose home app > AnyFSE. Enable "Enter Xbox mode on startup" only if the user asks for it. Reboot.
6. Cold boot check: full shutdown, power on, sign in. Does the device go straight to ROG Select (not stock dashboard, not desktop)? Time it. Note any Xbox app flash or desktop flash first.

## Step 4 — Cold-boot RAM and background-process comparison (the Xbox Mode verdict)

Do this twice, same conditions, idle 2 minutes after sign-in:

- **(a) Stock:** home app = Xbox, cold restart, record free/available RAM and the top background processes by RAM, and whether `explorer.exe` is running.
- **(b) ROG Select via AnyFSE:** home app = AnyFSE → ROG Select, cold restart, record the same numbers.

Report both sets of numbers and the difference. Do not write "optimisations preserved" from one screenshot — the numbers and the process lists are the evidence. If (b) is materially worse than (a), or if processes that were suppressed in (a) are running in (b), the verdict is NOT VERIFIED, even if ROG Select itself looks fine.

Then, with ROG Select as home app, pass/fail each of these:

- [ ] Controller, D-pad, touch, and audio work in FSE (swoosh on move, clang on select).
- [ ] Steam Big Picture and the configured emulator launch fullscreen from ROG Select, and Game Bar Home / Task View returns to ROG Select.
- [ ] Xbox PC app behaviour, recorded separately from restoring the original Xbox home.
- [ ] Game Bar Home (Win+G / Xbox button > Home) returns to ROG Select; Task View (Win+Tab / long-press Xbox / swipe up) also works. Record which routes worked.
- [ ] Sleep and resume: with ROG Select in foreground, sleep then resume — where do you land? With a game/app in foreground, sleep then resume — where do you land? With all apps closed, sleep then resume — does it return to ROG Select?
- [ ] Windows switch in FSE: press Windows once. It must send Win+F11 once, show "Switching to Windows desktop...", reach INACTIVE within 10 s, and land on the desktop. Then re-enter Xbox mode: are you prompted to restart to regain optimisations? If yes, restart and re-run Step 4. Record the prompt verbatim.
- [ ] ASUS Command Center / Armoury Crate and performance modes work unchanged, with ASUS Optimization untouched.

## Step 5 — Optional test only: `exit_on_launch`

Only after Steps 2–4 pass. This tests whether terminating the selector entirely is reliable, per Microsoft's GDK guidance to terminate a launcher after a successful launch to give the game more resources.

1. Set `exit_on_launch=on` in `selector.ini`, launch Steam, confirm `rog-select.exe` is gone from Task Manager (0 CPU, 0 RAM).
2. Try Game Bar Home and Task View: does ROG Select relaunch as the home app, or are you stuck in Steam with no selector? If it does NOT relaunch reliably, set `exit_on_launch=off` again and report that terminating is NOT reliable on this device — the hidden-with-timer-stopped behaviour in v1.0.4 stays the default.
3. If it DOES relaunch reliably, report that too, so a later update can make it the default. Do not change the default yourself on this test.

## Step 6 — Rollback (must pass even if everything else failed)

1. Settings > Gaming > Full Screen Experience / Xbox mode > Choose home app > Xbox. Reboot. Confirm the device boots to the stock Xbox dashboard again, matching Step 0.
2. If AnyFSE was used: uninstall it via its uninstaller. If broken, only as documented in AnyFSE's own README: `Get-AppxPackage *AnyFSE* | Remove-AppxPackage`, delete `%ProgramFiles%\AnyFSE`, and confirm no ACSE service remains.
3. Delete the ROG Select folder (and any Startup shortcut, if one was ever added). Reboot. Nothing remains: no service, no shell change, no driver change, no filter service.

## Step 7 — Report back (copy this shape)

- Standalone EXE: pass/fail, with `rog-select.log` FSE lines and Win+F11 transition lines.
- Windows card: pass/fail, transition time (ms), double-press behaviour, inactive-path behaviour.
- Scaling: pass/fail at 175% (and 100%/150% if tested), with any clipping/stretching noted.
- Controller: pass/fail, slot tested, reconnection behaviour.
- Touch: pass/fail, windowed and fullscreen, tapped card vs launched card.
- Audio: pass/fail (swoosh/clang).
- ES-DE / Steam / Xbox: pass/fail each.
- AnyFSE bridge (only if Step 2 passed): pass/fail, AnyFSE version, remapping OFF confirmed.
- Cold-boot comparison: stock RAM/processes vs ROG Select RAM/processes, with numbers.
- Sleep/resume, ASUS modes, Windows switch, Game Bar return: pass/fail each.
- `exit_on_launch` test (if run): pass/fail, and whether Game Bar Home relaunched the selector.
- Xbox Mode optimisations: VERIFIED (with evidence) or NOT VERIFIED.
- Rollback: confirmed, or what is left behind.
