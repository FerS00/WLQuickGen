@echo off
REM Builds WLQuickGen for x86 and x64 with MSVC.
REM Output: WLQuickGen-x86.exe and WLQuickGen-x64.exe
REM (CMake users: see "Building" in README.md.)
setlocal
set "VCVARS=C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvarsall.bat"
if not exist "%VCVARS%" (
    echo vcvarsall.bat not found at "%VCVARS%".
    echo Edit build.bat and point VCVARS at your Visual Studio installation.
    exit /b 1
)

cd /d "%~dp0"
set "SOURCES=src\WLQuickGen.cpp src\i18n.cpp src\theme.cpp"
set "CFLAGS=/nologo /std:c++17 /EHsc /O2 /W4 /permissive- /utf-8 /MT /DUNICODE /D_UNICODE /I."
set "LIBS=user32.lib gdi32.lib kernel32.lib comctl32.lib shell32.lib"

for %%A in (x86 x64) do (
    echo === Building %%A ===
    call "%VCVARS%" %%A >nul
    rc /nologo /fo WLQuickGen-%%A.res WLQuickGen.rc
    if errorlevel 1 (echo Resource compilation failed & exit /b 1)
    if not exist build-%%A mkdir build-%%A
    cl %CFLAGS% %SOURCES% WLQuickGen-%%A.res /Fe:WLQuickGen-%%A.exe /Fo:build-%%A\ ^
       /link /SUBSYSTEM:WINDOWS /MANIFEST:NO %LIBS%
    if errorlevel 1 (echo %%A build failed & exit /b 1)
)

rmdir /s /q build-x86 build-x64 2>nul
del /q WLQuickGen-x86.res WLQuickGen-x64.res 2>nul
echo.
echo === Done: WLQuickGen-x86.exe and WLQuickGen-x64.exe ===
endlocal
