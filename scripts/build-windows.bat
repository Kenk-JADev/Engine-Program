@echo off
REM RPG Maker 3D Engine - Windows Build Wrapper (Batch)
REM Ruft das PowerShell-Skript auf

echo ========================================
echo RPG Maker 3D Engine - Windows Build
echo ========================================

where pwsh >nul 2>&1
if %ERRORLEVEL% EQU 0 (
    echo Nutze PowerShell Core (pwsh)
    pwsh -ExecutionPolicy Bypass -File "%~dp0build-windows.ps1" %*
) else (
    echo Nutze Windows PowerShell
    powershell -ExecutionPolicy Bypass -File "%~dp0build-windows.ps1" %*
)

if %ERRORLEVEL% NEQ 0 (
    echo.
    echo [FEHLER] Build fehlgeschlagen! Siehe Logs oben.
    pause
    exit /b 1
)

echo.
echo [OK] Build erfolgreich!
echo EXE liegt in: build\Release\RPGMaker3D.exe
echo.
pause
