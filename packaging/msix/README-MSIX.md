# MSIX Gaming Home packaging — UNVERIFIED template

This folder is a starting point for the preferred native route, not a finished installer. v1.0.3 defers it: the first device test uses the AnyFSE bridge in ROG-INSTALL Step 3. Do not build or test this package in the first test.

## What is here

- `Package.appxmanifest` — template only. Identity Publisher is a placeholder and must match the signing certificate exactly. The `windows.gamingApp` extension children must be copied from a working Gaming Home example on device and validated with MakeAppx.
- No real `.SCCD` file is shipped. A valid Custom Capability Descriptor for `Microsoft.appCategory.gamingHome_8wekyb3d8bbwe` is required for the capability to be granted outside Developer Mode. Do not fabricate one.

## Signing, in outline (device steps, not done here)

1. On the Ally X only, create a self-signed developer certificate for the Publisher name in the manifest.
2. Trust that certificate (Trusted People, Local Machine). This step needs elevation; the selector itself never needs to run as administrator.
3. Turn on Developer Mode (Settings > System > Advanced / For developers) to sideload a package with a custom capability.
4. Build the package with MakeAppx, sign it with SignTool using that certificate, install with `Add-AppxPackage`.
5. Check Settings > Gaming > Full Screen Experience (Xbox mode) — ROG Select must appear as a selectable home app. If it does not, stop and report that result. Do not fall back to a shell registry change.
6. After install, test whether Developer Mode can be turned back off (the Playnite example says yes for its package; verify, don't assume, for this one).
7. Uninstall normally: Start menu > Uninstall, or `Get-AppxPackage *ROGSelect* | Remove-AppxPackage`. Then re-select Xbox as home app (see ROG-INSTALL.md §6 rollback).

## Security rules that still apply

- No Defender exclusion, no antivirus disabling.
- No DLL injection, no ASUS button remapping, no filter service.
- No disabling ASUS Optimization or Armoury Crate.
- No running ROG Select or games as administrator by default.
- No Windows system-file modification, no Explorer shell replacement.

## If MSIX proves too complex

Use the AnyFSE bridge documented in `ROG-INSTALL.md §6` instead: AnyFSE as home app, with a custom executable pointing at `rog-select.exe`, button remapping OFF, and no ACSE Filter service. ROG Select itself must not depend on AnyFSE staying resident — AnyFSE's own flow is to start the launcher, wait for its main window, close its splash and exit.
