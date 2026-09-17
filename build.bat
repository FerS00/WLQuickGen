@echo off
REM Builds WLQuickGen for x86 and x64 with MSVC.
REM Output: WLQuickGen-x86.exe and WLQuickGen-x64.exe
setlocal
set "VCVARS=C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvarsall.bat"
if not exist "%VCVARS%" (
    echo vcvarsall.bat not found at "%VCVARS%".
    echo Edit build.bat and point VCVARS at your Visual Studio installation.
    exit /b 1
)

cd /d "%~dp0"

echo === Building x86 ===
call "%VCVARS%" x86 >nul
rc /nologo /fo WLQuickGen-x86.res WLQuickGen.rc
if errorlevel 1 (echo Resource compilation failed & exit /b 1)
cl /nologo /std:c++17 /EHsc /O2 /W3 /DUNICODE /D_UNICODE WLQuickGen.cpp WLQuickGen-x86.res ^
   /Fe:WLQuickGen-x86.exe /Fo:build-x86.obj ^
   /link /SUBSYSTEM:WINDOWS user32.lib gdi32.lib kernel32.lib
if errorlevel 1 (echo x86 build failed & exit /b 1)

echo === Building x64 ===
call "%VCVARS%" x64 >nul
rc /nologo /fo WLQuickGen-x64.res WLQuickGen.rc
if errorlevel 1 (echo Resource compilation failed & exit /b 1)
cl /nologo /std:c++17 /EHsc /O2 /W3 /DUNICODE /D_UNICODE WLQuickGen.cpp WLQuickGen-x64.res ^
   /Fe:WLQuickGen-x64.exe /Fo:build-x64.obj ^
   /link /SUBSYSTEM:WINDOWS user32.lib gdi32.lib kernel32.lib
if errorlevel 1 (echo x64 build failed & exit /b 1)

del /q build-x86.obj build-x64.obj WLQuickGen-x86.res WLQuickGen-x64.res 2>nul
echo.
echo === Done: WLQuickGen-x86.exe and WLQuickGen-x64.exe ===
endlocal
