@echo off
setlocal enabledelayedexpansion

REM ==============================================
REM Auto-Updater for Saburo Application
REM ==============================================

echo [UPDATE] Starting update process...
echo.

REM === STEP 1: Check if running as update_new.bat (for self-update) ===
if /i "%~nx0"=="update_new.bat" (
    echo [SELF-UPDATE] Renaming to update.bat...
    timeout /t 1 /nobreak >nul
    if exist "%~dp0update.bat" del /f /q "%~dp0update.bat"
    ren "%~f0" "update.bat"
    echo [SELF-UPDATE] Restarting update.bat...
    start "" "%~dp0update.bat"
    exit /b
)

REM === STEP 2: Wait for Saburo.exe to exit ===
echo === STEP 1: Wait for Saburo.exe to exit ===
:waitloop
tasklist /fi "imagename eq Saburo.exe" 2>nul | find /i "Saburo.exe" >nul
if not errorlevel 1 (
    echo [WAIT] Saburo.exe is still running, waiting...
    timeout /t 2 /nobreak >nul
    goto waitloop
)

echo [OK] Saburo.exe has exited.
echo.

REM === STEP 3: Delete old files ===
echo === STEP 2: Delete old files ===
set "files=Logs.txt Taigei64.dll Version.txt Saburo.exe kdu.exe Hiroyoshi.sys drv64.dll"
for %%F in (%files%) do (
    if exist "%~dp0%%F" (
        del /f /q "%~dp0%%F" 2>nul
        if exist "%~dp0%%F" (
            echo [ERROR] Failed to delete: %%F
        ) else (
            echo [DELETED] %%F
        )
    )
)
echo.

REM === STEP 4: Download and parse Pastebin config ===
echo === STEP 3: Download Pastebin config ===
set "pastebinUrl=https://pastebin.com/raw/nsmMY5Nk"
set "configFile=%~dp0temp_config.txt"

curl -s "!pastebinUrl!" > "!configFile!"
if not exist "!configFile!" (
    echo !!! ERROR: Failed to download config from !pastebinUrl!
    pause
    exit /b 1
)

echo [CONFIG] Parsing configuration...
echo.

set "currentVersion="
set "downloadUrl="
set "loaderUrl="
set "updaterUrl="
set "device_symbollink="

REM Parse each line in the config file
for /f "usebackq tokens=1,* delims==" %%A in ("!configFile!") do (
    set "key=%%A"
    set "value=%%B"
    
    REM Remove quotes and trim spaces
    set "key=!key:"=!"
    set "key=!key: =!"
    set "value=!value:"=!"
    set "value=!value: =!"
    
    if "!key!"=="version" set "currentVersion=!value!"
    if "!key!"=="downloadUrl" set "downloadUrl=!value!"
    if "!key!"=="loaderUrl" set "loaderUrl=!value!"
    if "!key!"=="updaterUrl" set "updaterUrl=!value!"
    if "!key!"=="device_symbollink" set "device_symbollink=!value!"
)

echo [CONFIG] Parsed values:
echo   Version: !currentVersion!
echo   Download URL: !downloadUrl!
echo   Loader URL: !loaderUrl!
echo   Updater URL: !updaterUrl!
echo   Device: !device_symbollink!
echo.

del "!configFile!" 2>nul

REM === STEP 5: Self-update check (update.bat itself) ===
echo === STEP 4: Check for updater self-update ===
if not "!updaterUrl!"=="" (
    echo [SELF-UPDATE] Checking for updater update...
    
    REM Download latest updater as update_new.bat
    curl -L -o "%~dp0update_new.bat" "!updaterUrl!" 2>nul
    if exist "%~dp0update_new.bat" (
        REM Compare if different (simple size check)
        for %%F in ("%~dp0update_new.bat") do set "newSize=%%~zF"
        for %%F in ("%~f0") do set "currentSize=%%~zF"
        
        if not "!newSize!"=="!currentSize!" (
            echo [SELF-UPDATE] New updater found, restarting...
            start "" "%~dp0update_new.bat"
            exit /b
        ) else (
            echo [SELF-UPDATE] Updater is up to date.
            del /f /q "%~dp0update_new.bat" 2>nul
        )
    )
)
echo.

REM === STEP 6: Download main application files ===
echo === STEP 5: Download main application ===

REM Check if we need to download loader
if not "!loaderUrl!"=="" (
    echo [DOWNLOAD] Downloading Saburo.exe from loaderUrl...
    curl -L -o "%~dp0Saburo.exe" "!loaderUrl!"
    if exist "%~dp0Saburo.exe" (
        echo [OK] Saburo.exe downloaded successfully.
    ) else (
        echo [WARNING] Failed to download from loaderUrl, trying downloadUrl...
    )
)

REM If Saburo.exe not downloaded yet, try downloadUrl
if not exist "%~dp0Saburo.exe" and not "!downloadUrl!"=="" (
    echo [DOWNLOAD] Downloading from downloadUrl...
    
    REM Check if it's a ZIP file
    echo !downloadUrl! | find /i ".zip" >nul
    if not errorlevel 1 (
        echo [DOWNLOAD] Detected ZIP file, downloading...
        curl -L -o "%~dp0Hiroyoshi.zip" "!downloadUrl!"
        
        REM Try to extract ZIP
        echo [EXTRACT] Extracting ZIP file...
        
        REM Method 1: Using PowerShell (Windows 7+)
        powershell -command "Expand-Archive -Path '%~dp0Hiroyoshi.zip' -DestinationPath '%~dp0' -Force" 2>nul
        
        REM Method 2: Using tar (Windows 10+)
        if not exist "%~dp0Saburo.exe" (
            tar -xf "%~dp0Hiroyoshi.zip" -C "%~dp0" 2>nul
        )
        
        REM Clean up ZIP
        del /f /q "%~dp0Hiroyoshi.zip" 2>nul
        
    ) else (
        REM Direct executable download
        curl -L -o "%~dp0Saburo.exe" "!downloadUrl!"
    )
)

REM === STEP 7: Verify download ===
echo.
echo === STEP 6: Verify download ===
if exist "%~dp0Saburo.exe" (
    echo [SUCCESS] Saburo.exe is ready.
    
    REM Create/update Version.txt with current version
    if not "!currentVersion!"=="" (
        echo !currentVersion! > "%~dp0Version.txt"
        echo [VERSION] Set version to: !currentVersion!
    )
    
    REM Create empty Logs.txt if it doesn't exist
    if not exist "%~dp0Logs.txt" (
        echo. > "%~dp0Logs.txt"
    )
    
) else (
    echo !!! ERROR: Failed to download Saburo.exe!
    echo !!! Check your internet connection and try again.
    pause
    exit /b 1
)

REM === STEP 8: Launch application ===
echo.
echo === STEP 7: Launch application ===
echo [LAUNCH] Starting Saburo.exe...
start "" "%~dp0Saburo.exe"
timeout /t 3 /nobreak >nul

REM === STEP 9: Create version check helper for Saburo.exe ===
REM This creates a simple indicator that Saburo.exe can check
echo 1 > "%~dp0_updated.flag"

echo [COMPLETE] Update process finished!
echo.

REM === STEP 10: Self-cleanup after successful update ===
REM The batch file will delete itself after a delay
echo [CLEANUP] This updater will delete itself in 5 seconds...
echo [NOTE] Saburo.exe should detect and delete update.bat after verifying the update.
timeout /t 5 /nobreak >nul

REM Optionally delete ourselves (uncomment if needed)
REM del /f /q "%~f0" 2>nul
REM exit /b