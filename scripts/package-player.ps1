# Packt Player + Assets + SampleProject (Windows)
param([string]$OutDir = "dist/RPGMaker3D-Player")
$Root = Split-Path -Parent $PSScriptRoot
New-Item -ItemType Directory -Force -Path $OutDir | Out-Null
$candidates = @(
  "$Root/build/Release/RPGMaker3D_Player.exe",
  "$Root/build/RPGMaker3D_Player.exe"
)
foreach ($c in $candidates) {
  if (Test-Path $c) { Copy-Item -Force $c $OutDir }
}
foreach ($d in @("assets","SampleProject","ruby")) {
  if (Test-Path "$Root/$d") { Copy-Item -Recurse -Force "$Root/$d" "$OutDir/$d" }
}
Get-ChildItem "$Root/build/Release/*.dll" -ErrorAction SilentlyContinue | Copy-Item -Force -Destination $OutDir
Write-Host "Player-Paket: $OutDir"
Get-ChildItem $OutDir
