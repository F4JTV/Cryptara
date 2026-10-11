@echo off
rem ===========================================================================
rem  CRYPTARA - complete Windows build
rem
rem  Configures, compiles, deploys the Qt runtime, bundles the Visual C++
rem  runtime and builds the installer, in one go.
rem
rem  Run it from the project root, in a "x64 Native Tools Command Prompt
rem  for VS 2022" (or 2026). Needs, installed once:
rem    - Visual Studio with "Desktop development with C++"
rem    - Qt 6 for MSVC 64-bit, with Qt Quick and Qt Quick Controls (both
rem      come with every Qt installation)
rem    - Inno Setup 6 or 7, for the installer
rem  Nothing else: the cryptography is Windows' own (CNG, bcrypt.dll).
rem
rem  Options:
rem    /clean        wipe the build directory first
rem    /nobuild      skip configure and compile, deploy and package only
rem    /noinstaller  stop after staging, do not run Inno Setup
rem    /test         run the unit tests after compiling
rem  build_all.bat /? prints this list.
rem ===========================================================================

setlocal enabledelayedexpansion
title CRYPTARA - complete build

rem ------------------------------------------------------- paths to adjust
set "QT_DIR=C:\Qt\6.11.2\msvc2022_64"

set "BUILD_DIR=build"
set "DIST=installer\dist"

rem ------------------------------------------------------------- arguments
set DO_CLEAN=0
set DO_BUILD=1
set DO_INSTALLER=1
set DO_TEST=0

:parse
if "%~1"=="" goto parsed
if /i "%~1"=="/clean"       set DO_CLEAN=1&      shift & goto parse
if /i "%~1"=="/nobuild"     set DO_BUILD=0&      shift & goto parse
if /i "%~1"=="/noinstaller" set DO_INSTALLER=0&  shift & goto parse
if /i "%~1"=="/test"        set DO_TEST=1&       shift & goto parse
if /i "%~1"=="/?"           goto usage
echo Unknown option: %~1
goto usage
:parsed

if not exist "CMakeLists.txt" (
    echo [X] Run this script from the project root, next to CMakeLists.txt.
    goto fail
)

rem The application version, read from CMakeLists.txt and passed to Inno
rem Setup. This line stays at top level: the searched string holds a
rem parenthesis, which would break a parenthesised block.
set "APP_VERSION="
for /f "tokens=1-3" %%A in ('findstr /b /c:"project(CRYPTARA" CMakeLists.txt') do set "APP_VERSION=%%C"
if not defined APP_VERSION set "APP_VERSION=0.0.0"

rem ---------------------------------------------------------- prerequisites
echo.
echo === Checking prerequisites ===

where cmake >nul 2>&1
if errorlevel 1 (
    echo [X] cmake not found in PATH. Open a "x64 Native Tools Command Prompt".
    goto fail
)
where ninja >nul 2>&1
if errorlevel 1 (
    echo [X] ninja not found in PATH. It comes with the "C++ CMake tools for
    echo     Windows" component of Visual Studio; open a "x64 Native Tools
    echo     Command Prompt".
    goto fail
)
where cl >nul 2>&1
if errorlevel 1 (
    echo [X] cl.exe not found. This must run in a "x64 Native Tools Command Prompt".
    goto fail
)
echo [ok] cl

if not exist "%QT_DIR%\bin\windeployqt.exe" (
    echo [X] Qt not found at %QT_DIR%
    echo     Edit QT_DIR at the top of this script.
    goto fail
)
echo [ok] Qt          %QT_DIR%
rem The tests run from the build folder and need the Qt DLLs.
set "PATH=%QT_DIR%\bin;%PATH%"

rem Inno Setup, looked up in the usual places then in PATH.
set "ISCC="
for %%P in (
    "%ProgramFiles(x86)%\Inno Setup 6\ISCC.exe"
    "%ProgramFiles%\Inno Setup 6\ISCC.exe"
    "%ProgramFiles(x86)%\Inno Setup 7\ISCC.exe"
    "%ProgramFiles%\Inno Setup 7\ISCC.exe"
) do if not defined ISCC if exist %%P set "ISCC=%%~P"
if not defined ISCC for /f "delims=" %%P in ('where ISCC 2^>nul') do if not defined ISCC set "ISCC=%%P"

if "%DO_INSTALLER%"=="1" (
    if defined ISCC (
        echo [ok] Inno Setup  !ISCC!
    ) else (
        echo [--] Inno Setup not found, the installer step will be skipped.
        set DO_INSTALLER=0
    )
)
echo [ok] Version     %APP_VERSION%

rem ------------------------------------------------------------------ clean
if "%DO_CLEAN%"=="1" (
    echo.
    echo === Cleaning ===
    if exist "%BUILD_DIR%" rmdir /s /q "%BUILD_DIR%"
    if exist "%DIST%"      rmdir /s /q "%DIST%"
    if exist "installer\output" rmdir /s /q "installer\output"
)

rem ------------------------------------------------------------ timestamps
rem A project file dated in the future (an archive unpacked with another
rem clock or time zone) makes Ninja re-run CMake endlessly before compiling:
rem such files are brought back to the present time first.
if "%DO_BUILD%"=="1" powershell -NoProfile -ExecutionPolicy Bypass -Command "$now = Get-Date; $late = Get-ChildItem -Recurse -File | Where-Object { $_.LastWriteTime -gt $now.AddMinutes(1) -and $_.FullName -notlike '*\build\*' -and $_.FullName -notlike '*\installer\dist\*' }; foreach ($f in $late) { Write-Host ('[--] dated ' + $f.LastWriteTime + ', in the future: reset ' + $f.FullName); $f.LastWriteTime = $now }"

rem -------------------------------------------------------------- configure
if "%DO_BUILD%"=="1" (
    echo.
    echo === Configuring ===
    cmake -B "%BUILD_DIR%" -G Ninja ^
        -DCMAKE_BUILD_TYPE=Release ^
        -DCMAKE_CXX_COMPILER=cl ^
        -DCMAKE_PREFIX_PATH="%QT_DIR%"
    if errorlevel 1 (
        echo [X] Configuration failed.
        goto fail
    )

    rem The configuration must be up to date right after it is written. If
    rem Ninja still finds one of its inputs newer or missing, it would re-run
    rem CMake again and again: it names the file here, and the build stops.
    ninja -C "%BUILD_DIR%" -n -d explain build.ninja > "%BUILD_DIR%\manifest-check.txt" 2>&1
    findstr /c:"ninja explain" "%BUILD_DIR%\manifest-check.txt" >nul
    if not errorlevel 1 (
        echo [X] The configuration is out of date as soon as it is written.
        echo     Ninja explains:
        type "%BUILD_DIR%\manifest-check.txt"
        echo     The file named above is dated in the future or missing. Check the
        echo     date, time and time zone of this PC, then run build_all.bat /clean.
        goto fail
    )

    echo.
    echo === Compiling ===
    cmake --build "%BUILD_DIR%"
    if errorlevel 1 (
        echo [X] Compilation failed.
        goto fail
    )
)

if "%DO_TEST%"=="1" (
    echo.
    echo === Tests ===
    set "QT_QPA_PLATFORM=offscreen"
    ctest --test-dir "%BUILD_DIR%" --output-on-failure
    if errorlevel 1 (
        echo [X] A test failed.
        goto fail
    )
    set "QT_QPA_PLATFORM="
)

set "OUT=%BUILD_DIR%"
if not exist "%OUT%\CRYPTARA.exe" (
    echo [X] %OUT%\CRYPTARA.exe missing. Compile without /nobuild first.
    goto fail
)

rem ------------------------------------------------------------------ stage
echo.
echo === Gathering into %DIST% ===
if exist "%DIST%" rmdir /s /q "%DIST%"
mkdir "%DIST%" 2>nul

copy /y "%OUT%\CRYPTARA.exe" "%DIST%\" >nul

rem --qmldir: windeployqt reads the imports of the QML files to know which
rem Qt Quick modules to copy (Controls, Material style, Dialogs, Layouts).
rem The Qt translations stay: they give the file dialogs their French.
echo   Qt runtime...
"%QT_DIR%\bin\windeployqt.exe" --release --qmldir src\qml --no-system-d3d-compiler --no-opengl-sw ^
    "%DIST%\CRYPTARA.exe" >nul
if errorlevel 1 (
    echo [X] windeployqt failed.
    goto fail
)

rem The Visual C++ runtime. A fresh Windows does not have it, and the
rem program then stops at start-up on a missing MSVCP140.dll. windeployqt
rem usually drops the redistributable next to the program; if not, it comes
rem from the Visual Studio this prompt belongs to. The installer runs it.
echo   Visual C++ runtime...
if not exist "%DIST%\vc_redist.x64.exe" (
    if defined VCToolsRedistDir if exist "%VCToolsRedistDir%vc_redist.x64.exe" copy /y "%VCToolsRedistDir%vc_redist.x64.exe" "%DIST%\" >nul
)
if exist "%DIST%\vc_redist.x64.exe" (
    echo [ok] vc_redist.x64.exe bundled
) else (
    echo [--] vc_redist.x64.exe not found: the installer will rely on the runtime
    echo      already being present on the target machine.
)

copy /y "README.md" "%DIST%\README.md" >nul
if exist "LICENSE" copy /y "LICENSE" "%DIST%\LICENSE.txt" >nul

rem -------------------------------------------------------------- installer
if "%DO_INSTALLER%"=="1" (
    echo.
    echo === Building the installer, version %APP_VERSION% ===
    "!ISCC!" /Qp /DAppVersion=%APP_VERSION% "installer\CRYPTARA.iss"
    if errorlevel 1 (
        echo [X] Inno Setup failed.
        goto fail
    )
    echo.
    echo [ok] Installer: installer\output\CRYPTARA-%APP_VERSION%-setup.exe
) else (
    echo.
    echo [ok] Staged in %DIST%; run CRYPTARA.exe from there, or build the
    echo      installer with: "%ProgramFiles(x86)%\Inno Setup 6\ISCC.exe" /DAppVersion=%APP_VERSION% installer\CRYPTARA.iss
)

echo.
echo Done.
endlocal
exit /b 0

:usage
echo.
echo Usage: build_all.bat [/clean] [/nobuild] [/noinstaller] [/test]
echo.
echo   /clean        wipe the build directory first
echo   /nobuild      skip configure and compile; deploy and package only
echo   /noinstaller  stop after staging installer\dist
echo   /test         run the unit tests after compiling
echo.
echo Adjust QT_DIR at the top of the file.
endlocal
exit /b 2

:fail
echo.
echo Build aborted.
endlocal
exit /b 1
