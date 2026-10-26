@echo off
REM ROG Select - local Windows build (run in a Developer Command Prompt,
REM or this script will try to load vcvarsall itself).
REM Output: rog-select.exe in this folder. No installer, no registry writes.
setlocal
if not defined VCINSTALLDIR (
  if exist "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvarsall.bat" (
    call "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvarsall.bat" x64
  ) else if exist "C:\Program Files\Microsoft Visual Studio\2022\Professional\VC\Auxiliary\Build\vcvarsall.bat" (
    call "C:\Program Files\Microsoft Visual Studio\2022\Professional\VC\Auxiliary\Build\vcvarsall.bat" x64
  )
)
cl /nologo /O2 /EHsc /std:c++17 /DUNICODE /D_UNICODE src\main.cpp /link d2d1.lib windowscodecs.lib dwrite.lib Xinput.lib winmm.lib user32.lib gdi32.lib shell32.lib /OUT:rog-select.exe
if errorlevel 1 (
  echo BUILD FAILED - see the errors above.
  exit /b 1
)
echo.
echo BUILD OK: rog-select.exe
echo Test windowed first: rog-select.exe --windowed
endlocal
