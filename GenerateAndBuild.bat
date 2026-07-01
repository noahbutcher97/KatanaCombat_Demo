@rem © 2026 Jshaun Hookumchand. All rights reserved.
@echo off
if /i "%~1" NEQ "__keep" (
  start "Hook's - Generate & Build Tool" cmd /k ""%~f0" __keep"
  exit /b
)
setlocal enabledelayedexpansion

set "THIS_DIR=%~dp0"
pushd "%THIS_DIR%" >nul

REM -----------------------------
REM Find exactly one .uproject
REM -----------------------------
set "UPROJECT="
set /a COUNT=0
for %%F in (*.uproject) do (
    set /a COUNT+=1
    if !COUNT! EQU 1 set "UPROJECT=%%~fF"
)

if %COUNT% EQU 0 (
    echo [ERROR] No .uproject found in: "%THIS_DIR%"
    pause
    popd >nul
    exit /b 1
)

if %COUNT% GTR 1 (
    echo [ERROR] More than one .uproject found in: "%THIS_DIR%"
    dir /b *.uproject
    pause
    popd >nul
    exit /b 1
)

for %%F in ("%UPROJECT%") do set "PROJECT_NAME=%%~nF"

REM -----------------------------
REM Locate UnrealVersionSelector (UVS)
REM -----------------------------
set "UVS_EXE="
for /f "tokens=2,*" %%A in ('reg query "HKCR\Unreal.ProjectFile\shell\rungenproj\command" /ve 2^>nul ^| find /i "REG_SZ"') do (
    set "UVS_CMD=%%B"
)

if defined UVS_CMD (
    for %%P in (!UVS_CMD!) do (
        if exist "%%~fP" (
            set "UVS_EXE=%%~fP"
            goto :found_uvs
        )
    )
)

if exist "%ProgramFiles(x86)%\Epic Games\Launcher\Engine\Binaries\Win64\UnrealVersionSelector.exe" (
    set "UVS_EXE=%ProgramFiles(x86)%\Epic Games\Launcher\Engine\Binaries\Win64\UnrealVersionSelector.exe"
)

:found_uvs
if not defined UVS_EXE (
    echo [ERROR] UnrealVersionSelector.exe not found.
    pause
    popd >nul
    exit /b 1
)
:: ---- SET P4IGNORE ----
setx P4IGNORE .p4ignore
echo [INFO] P4IGNORE set to .p4ignore
echo [INFO] Project : "%UPROJECT%"
echo [INFO] UVS     : "%UVS_EXE%"

REM -----------------------------
REM Preflight: detect VS 2022 + C++ workload + Windows SDK
REM -----------------------------
set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
set "HAS_VS=0"
set "HAS_SDK=0"
set "VS_DISPLAY_NAME="
set "SDK_VERSION="

REM -- Check for VS 2017+ with C++ build tools via vswhere
REM    -products *      : include Build Tools (excluded by default)
REM    -version [15.0,) : VS 2017 or newer (UE 4.22+ needs VS 2017; UE 4.25+ needs VS 2019; UE 5.2+ needs VS 2022)
if exist "%VSWHERE%" (
    for /f "usebackq tokens=*" %%I in (`"%VSWHERE%" -products * -version [15.0^,^) -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property displayName 2^>nul`) do (
        set "HAS_VS=1"
        set "VS_DISPLAY_NAME=%%I"
    )
)

REM -- Fallback: filesystem probe for VS versions vswhere can't detect (e.g. VS 2026)
if "!HAS_VS!"=="0" (
    for %%R in ("%ProgramFiles%\Microsoft Visual Studio" "%ProgramFiles(x86)%\Microsoft Visual Studio") do (
        if "!HAS_VS!"=="0" if exist %%R (
            for /f "delims=" %%V in ('dir /b /ad %%R 2^>nul') do (
                if "!HAS_VS!"=="0" (
                    for %%E in (Community Professional Enterprise BuildTools) do (
                        if "!HAS_VS!"=="0" if exist "%%~R\%%V\%%E\VC\Tools\MSVC\" (
                            set "HAS_VS=1"
                            set "VS_DISPLAY_NAME=Visual Studio %%V %%E [filesystem probe]"
                        )
                    )
                )
            )
        )
    )
)

REM -- Check for Windows 10/11 SDK via registry (robust: strip trailing backslash, no version filter)
set "KITS_ROOT="
for /f "tokens=2,*" %%A in ('reg query "HKLM\SOFTWARE\Microsoft\Windows Kits\Installed Roots" /v KitsRoot10 2^>nul') do (
    if /i "%%A"=="REG_SZ" set "KITS_ROOT=%%B"
)
if defined KITS_ROOT (
    if "!KITS_ROOT:~-1!"=="\" set "KITS_ROOT=!KITS_ROOT:~0,-1!"
    for /f "delims=" %%V in ('dir /b /ad /on "!KITS_ROOT!\Include" 2^>nul') do (
        if exist "!KITS_ROOT!\Include\%%V\um\Windows.h" (
            set "HAS_SDK=1"
            set "SDK_VERSION=%%V"
        )
    )
)

if "!HAS_VS!"=="1" (
    echo [OK] Found: !VS_DISPLAY_NAME!
) else (
    echo [MISSING] Visual Studio 2022 with C++ workload not found.
)

if "!HAS_SDK!"=="1" (
    echo [OK] Found: Windows SDK !SDK_VERSION!
) else (
    echo [MISSING] Windows 10/11 SDK not found.
)

REM -- OS-aware SDK picker (Win10 -> 22621, Win11 -> 26100)
REM    WIN_SDK_COMPONENT is the matching VS Installer component ID (used as install fallback)
set "WIN_SDK_PKG=Microsoft.WindowsSDK.10.0.22621"
set "WIN_SDK_LABEL=Windows 10 SDK (10.0.22621)"
set "WIN_SDK_COMPONENT=Microsoft.VisualStudio.Component.Windows11SDK.22621"
for /f "tokens=4 delims=. " %%B in ('ver 2^>nul') do set "WIN_BUILD=%%B"
if defined WIN_BUILD if !WIN_BUILD! GEQ 22000 (
    set "WIN_SDK_PKG=Microsoft.WindowsSDK.10.0.26100"
    set "WIN_SDK_LABEL=Windows 11 SDK (10.0.26100)"
    set "WIN_SDK_COMPONENT=Microsoft.VisualStudio.Component.Windows11SDK.26100"
)

if "!HAS_VS!"=="0" if "!HAS_SDK!"=="0" (
    echo.
    echo ==============================================
    echo   MISSING: Visual Studio 2022 + Windows SDK
    echo ==============================================
    echo.
    echo Unreal Engine requires Visual Studio 2022 with
    echo the "Desktop development with C++" workload
    echo and a Windows SDK to generate project files and
    echo compile for Win64.
    echo.
    set /p "INSTALL_TOOLS=Install them now via winget? [Y/n]: "
    if /i "!INSTALL_TOOLS!" NEQ "n" (
        echo.
        echo [INFO] Installing VS 2022 Build Tools with C++ workload + !WIN_SDK_LABEL!...
        echo        This may take several minutes. An admin prompt may appear.
        echo.
        winget install Microsoft.VisualStudio.2022.BuildTools --override "--wait --passive --add Microsoft.VisualStudio.Workload.VCTools --add !WIN_SDK_COMPONENT! --includeRecommended" --accept-source-agreements --accept-package-agreements
        if !ERRORLEVEL! NEQ 0 (
            echo.
            echo [ERROR] winget install failed. Please install Visual Studio 2022 manually
            echo         with the "Desktop development with C++" workload.
            pause
            popd >nul
            exit /b 1
        )
        echo.
        echo [OK] Installation complete. Continuing...
        echo.
    ) else (
        echo.
        echo [INFO] Skipped. Install Visual Studio 2022 with C++ workload manually, then re-run.
        pause
        popd >nul
        exit /b 1
    )
    goto :preflight_done
)

if "!HAS_VS!"=="0" (
    echo.
    echo ==============================================
    echo   MISSING: Visual Studio 2022 C++ Workload
    echo ==============================================
    echo.
    echo A Windows SDK was found ^(!SDK_VERSION!^), but Visual
    echo Studio 2022 with the "Desktop development with
    echo C++" workload is required.
    echo.
    set /p "INSTALL_VS=Install VS 2022 Build Tools now via winget? [Y/n]: "
    if /i "!INSTALL_VS!" NEQ "n" (
        echo.
        echo [INFO] Installing VS 2022 Build Tools with C++ workload...
        echo.
        winget install Microsoft.VisualStudio.2022.BuildTools --override "--wait --passive --add Microsoft.VisualStudio.Workload.VCTools --includeRecommended" --accept-source-agreements --accept-package-agreements
        if !ERRORLEVEL! NEQ 0 (
            echo.
            echo [ERROR] winget install failed. Install Visual Studio 2022 manually.
            pause
            popd >nul
            exit /b 1
        )
        echo.
        echo [OK] Installation complete. Continuing...
        echo.
    ) else (
        echo.
        echo [INFO] Skipped. Install Visual Studio 2022 with C++ workload manually, then re-run.
        pause
        popd >nul
        exit /b 1
    )
    goto :preflight_done
)

if "!HAS_SDK!"=="0" (
    echo.
    echo ==============================================
    echo   MISSING: Windows 10/11 SDK
    echo ==============================================
    echo.
    echo Visual Studio 2022 with C++ was found, but no
    echo Windows SDK was detected. The SDK is required
    echo to compile for Win64.
    echo.
    set /p "INSTALL_SDK=Install !WIN_SDK_LABEL! now via winget? [Y/n]: "
    if /i "!INSTALL_SDK!" NEQ "n" (
        echo.
        echo [INFO] Installing !WIN_SDK_LABEL!...
        echo.
        winget install !WIN_SDK_PKG! --accept-source-agreements --accept-package-agreements
        if !ERRORLEVEL! NEQ 0 (
            echo [WARN] winget standalone SDK install failed. Trying via VS 2022 Build Tools modifier...
            winget install Microsoft.VisualStudio.2022.BuildTools --override "--wait --passive --add !WIN_SDK_COMPONENT!" --accept-source-agreements --accept-package-agreements
            if !ERRORLEVEL! NEQ 0 (
                echo.
                echo [ERROR] Could not install Windows SDK. Install it manually via
                echo         Visual Studio Installer: add a Windows 10/11 SDK component.
                pause
                popd >nul
                exit /b 1
            )
        )
        echo.
        echo [OK] !WIN_SDK_LABEL! installed. Continuing...
        echo.
    ) else (
        echo.
        echo [INFO] Skipped. Add a Windows SDK via Visual Studio Installer, then re-run.
        pause
        popd >nul
        exit /b 1
    )
)

:preflight_done
echo.
echo [INFO] Step 1/2: Generate project files...
echo(

"%UVS_EXE%" /projectfiles "%UPROJECT%"
set "GEN_ERR=%ERRORLEVEL%"

echo(
if not "%GEN_ERR%"=="0" (
    echo [ERROR] Generate project files failed. ErrorLevel=%GEN_ERR%
    pause
    popd >nul
    exit /b %GEN_ERR%
)

echo [OK] Project files generated.

REM -----------------------------
REM Read EngineAssociation from .uproject
REM -----------------------------
set "ENGINE_ID="
for /f "usebackq tokens=1,2 delims=:" %%A in (`findstr /i /c:"EngineAssociation" "%UPROJECT%"`) do (
    set "ENGINE_ID=%%B"
)

if not defined ENGINE_ID (
    echo [ERROR] Could not read EngineAssociation from .uproject.
    pause
    popd >nul
    exit /b 1
)

REM Clean ENGINE_ID: remove spaces, commas, quotes, braces
set "ENGINE_ID=%ENGINE_ID: =%"
set "ENGINE_ID=%ENGINE_ID:,=%"
set "ENGINE_ID=%ENGINE_ID:\"=%"
set "ENGINE_ID=%ENGINE_ID:"=%"
set "ENGINE_ID=%ENGINE_ID:}=%"

echo [INFO] EngineAssociation: "%ENGINE_ID%"

REM Convert 5.7 -> UE_5.7 folder name pattern
set "ENGINE_FOLDER=UE_%ENGINE_ID%"

REM -----------------------------
REM Find Engine directory dynamically
REM 1) Try registry GUID mapping (if EngineAssociation is GUID)
REM 2) BFS all drives for UE_x.y folder (depth 0-2)
REM 3) Prompt user for manual path
REM -----------------------------
set "ENGINE_DIR="

REM (1) Registry mapping (works when EngineAssociation is a GUID)
for /f "tokens=2,*" %%A in ('reg query "HKCU\Software\Epic Games\Unreal Engine\Builds" /v "%ENGINE_ID%" 2^>nul ^| find /i "REG_SZ"') do (
    set "ENGINE_DIR=%%B"
)
if not defined ENGINE_DIR (
    for /f "tokens=2,*" %%A in ('reg query "HKLM\Software\Epic Games\Unreal Engine\Builds" /v "%ENGINE_ID%" 2^>nul ^| find /i "REG_SZ"') do (
        set "ENGINE_DIR=%%B"
    )
)

REM (2) Breadth-first search: scan all drives for a folder named UE_x.y
REM     Searches root level first, then one level deep, then two levels deep
if not defined ENGINE_DIR (
    echo [INFO] Searching for %ENGINE_FOLDER% folder across all drives...
    for %%D in (C D E F G H I J K L M N O P Q R S T U V W X Y Z) do (
        if not defined ENGINE_DIR if exist "%%D:\" (
            REM Depth 0: drive root (e.g. C:\UE_5.7)
            if exist "%%D:\%ENGINE_FOLDER%\Engine\Build\BatchFiles\Build.bat" (
                set "ENGINE_DIR=%%D:\%ENGINE_FOLDER%"
                goto :engine_found
            )
            REM Depth 1: one folder deep (e.g. C:\Epic Games\UE_5.7)
            for /f "delims=" %%P in ('dir /b /ad "%%D:\" 2^>nul') do (
                if not defined ENGINE_DIR if exist "%%D:\%%P\%ENGINE_FOLDER%\Engine\Build\BatchFiles\Build.bat" (
                    set "ENGINE_DIR=%%D:\%%P\%ENGINE_FOLDER%"
                    goto :engine_found
                )
            )
            REM Depth 2: two folders deep (e.g. C:\Program Files\Epic Games\UE_5.7)
            for /f "delims=" %%P in ('dir /b /ad "%%D:\" 2^>nul') do (
                if not defined ENGINE_DIR (
                    for /f "delims=" %%Q in ('dir /b /ad "%%D:\%%P\" 2^>nul') do (
                        if not defined ENGINE_DIR if exist "%%D:\%%P\%%Q\%ENGINE_FOLDER%\Engine\Build\BatchFiles\Build.bat" (
                            set "ENGINE_DIR=%%D:\%%P\%%Q\%ENGINE_FOLDER%"
                            goto :engine_found
                        )
                    )
                )
            )
        )
    )
)

:engine_found
if not defined ENGINE_DIR (
    echo [WARNING] Could not find %ENGINE_FOLDER% on this machine.
    echo.
    echo Please enter the full path to your %ENGINE_FOLDER% folder.
    echo Example: C:\Program Files\Epic Games\%ENGINE_FOLDER%
    echo.
    set /p "ENGINE_DIR=%ENGINE_FOLDER% path: "
)

if not defined ENGINE_DIR (
    echo [ERROR] No path provided. Aborting.
    pause
    popd >nul
    exit /b 1
)

REM Strip trailing backslash and quotes from user input
set "ENGINE_DIR=%ENGINE_DIR:"=%"
if "%ENGINE_DIR:~-1%"=="\" set "ENGINE_DIR=%ENGINE_DIR:~0,-1%"

if not exist "%ENGINE_DIR%\Engine" (
    echo [ERROR] Invalid engine path: "%ENGINE_DIR%"
    echo         Could not find Engine subfolder.
    pause
    popd >nul
    exit /b 1
)

set "BUILD_BAT=%ENGINE_DIR%\Engine\Build\BatchFiles\Build.bat"
if not exist "%BUILD_BAT%" (
    echo [ERROR] Build.bat not found at: "%BUILD_BAT%"
    pause
    popd >nul
    exit /b 1
)

echo [INFO] Engine  : "%ENGINE_DIR%"
echo [INFO] Step 2/2: Build %PROJECT_NAME%Editor Win64 Development...
echo(

REM ---- Detect UBT layout (UE4 vs UE5)
REM    UE5 -> Engine\Binaries\DotNET\UnrealBuildTool\UnrealBuildTool.dll (invoked via bundled dotnet.exe)
REM    UE4 -> Engine\Binaries\DotNET\UnrealBuildTool.exe (.NET Framework, invoked directly)
set "UBT_DLL=%ENGINE_DIR%\Engine\Binaries\DotNET\UnrealBuildTool\UnrealBuildTool.dll"
set "UBT_EXE=%ENGINE_DIR%\Engine\Binaries\DotNET\UnrealBuildTool.exe"
if exist "%UBT_DLL%" goto :build_ue5
if exist "%UBT_EXE%" goto :build_ue4

echo [ERROR] UnrealBuildTool not found. Looked for:
echo         "%UBT_DLL%"
echo         "%UBT_EXE%"
pause
popd >nul
exit /b 1

:build_ue5
REM ---- UE5: find engine-bundled dotnet.exe (versioned subfolders under ThirdParty\DotNet)
set "DOTNET_EXE="
for /f "delims=" %%D in ('dir /b /s "%ENGINE_DIR%\Engine\Binaries\ThirdParty\DotNet\dotnet.exe" 2^>nul') do (
    set "DOTNET_EXE=%%D"
    goto :got_dotnet
)

:got_dotnet
if not defined DOTNET_EXE (
    echo [ERROR] Could not find bundled dotnet.exe under:
    echo         "%ENGINE_DIR%\Engine\Binaries\ThirdParty\DotNet"
    pause
    popd >nul
    exit /b 1
)

REM ---- Sanitize DOTNET_EXE (remove any stray quotes/apostrophes)
set "DOTNET_EXE=%DOTNET_EXE:"=%"
set "DOTNET_EXE=%DOTNET_EXE:'=%"

if not exist "%DOTNET_EXE%" (
    echo [ERROR] dotnet.exe path does not exist:
    echo         "%DOTNET_EXE%"
    pause
    popd >nul
    exit /b 1
)

echo [INFO] UBT     : "%UBT_DLL%" (UE5 .NET)
echo [INFO] DotNet  : "%DOTNET_EXE%"
echo [INFO] Building: %PROJECT_NAME%Editor Win64 Development
echo(

"%DOTNET_EXE%" "%UBT_DLL%" %PROJECT_NAME%Editor Win64 Development "%UPROJECT%" -waitmutex
set "BUILD_ERR=%ERRORLEVEL%"
goto :build_check

:build_ue4
echo [INFO] UBT     : "%UBT_EXE%" (UE4 .NET Framework)
echo [INFO] Building: %PROJECT_NAME%Editor Win64 Development
echo(

"%UBT_EXE%" %PROJECT_NAME%Editor Win64 Development "%UPROJECT%" -waitmutex
set "BUILD_ERR=%ERRORLEVEL%"

:build_check
echo(
if not "%BUILD_ERR%"=="0" (
    echo [ERROR] Build failed. ErrorLevel=%BUILD_ERR%
    popd >nul
    pause
    exit /b %BUILD_ERR%
)

:build_ok
echo ======================================
echo   BUILD COMPLETE
echo ======================================
echo(
pause
exit
