# RPG Maker 3D Engine – Windows Build-Skript
# Führt folgende Schritte aus:
# 1. vcpkg installieren
# 2. SDL2 + glm über vcpkg installieren
# 3. glad2 generieren
# 4. CMake-Projekt bauen (Release)
# 5. Assets in den Build-Ordner kopieren
#
# Ausführen in PowerShell (als Administrator empfohlen für vcpkg):
#   .\scripts\build-windows.ps1

param(
    [string]$VcpkgRoot = "C:\dev\vcpkg",
    [string]$BuildType = "Release"
)

$ErrorActionPreference = "Stop"
$ProjectRoot = Split-Path -Parent $PSScriptRoot

Write-Host "========================================" -ForegroundColor Cyan
Write-Host "RPG Maker 3D Engine – Windows Build" -ForegroundColor Cyan
Write-Host "========================================" -ForegroundColor Cyan

# 1. vcpkg prüfen/klonen
if (-not (Test-Path $VcpkgRoot)) {
    Write-Host "vcpkg wird heruntergeladen..." -ForegroundColor Yellow
    git clone https://github.com/Microsoft/vcpkg.git $VcpkgRoot
    & "$VcpkgRoot\bootstrap-vcpkg.bat"
}

# 2. Dependencies installieren
Write-Host "Installiere SDL2 und glm via vcpkg..." -ForegroundColor Yellow
& "$VcpkgRoot\vcpkg.exe" install sdl2:x64-windows-static glm:x64-windows-static --triplet x64-windows-static

# 3. glad2 installieren und generieren
Write-Host "Installiere glad2..." -ForegroundColor Yellow
python -m pip install --upgrade pip
python -m pip install glad2

Write-Host "Generiere OpenGL 3.3 Core Loader..." -ForegroundColor Yellow
$GladOut = "$ProjectRoot\third_party\glad_generated"
if (Test-Path $GladOut) {
    Remove-Item -Recurse -Force $GladOut
}
python -m glad --api gl:core=3.3 --out-path $GladOut c

# 4. CMake Build
$BuildDir = "$ProjectRoot\build"
if (-not (Test-Path $BuildDir)) {
    New-Item -ItemType Directory -Path $BuildDir | Out-Null
}

$Toolchain = "$VcpkgRoot\scripts\buildsystems\vcpkg.cmake"
Write-Host "Generiere CMake-Projekt..." -ForegroundColor Yellow
cmake -B $BuildDir -S $ProjectRoot `
    -DCMAKE_TOOLCHAIN_FILE="$Toolchain" `
    -DVCPKG_TARGET_TRIPLET=x64-windows-static `
    -DCMAKE_BUILD_TYPE=$BuildType

Write-Host "Baue Projekt ($BuildType)..." -ForegroundColor Yellow
cmake --build $BuildDir --config $BuildType --parallel

# 5. Assets kopieren
$OutputDir = "$BuildDir\$BuildType"
Write-Host "Kopiere Assets nach $OutputDir ..." -ForegroundColor Yellow
if (Test-Path "$ProjectRoot\assets") {
    Copy-Item -Recurse -Force "$ProjectRoot\assets" "$OutputDir\assets"
}

Write-Host "========================================" -ForegroundColor Green
Write-Host "Build abgeschlossen!" -ForegroundColor Green
Write-Host "Executable: $OutputDir\RPGMaker3D.exe" -ForegroundColor Green
Write-Host "========================================" -ForegroundColor Green
