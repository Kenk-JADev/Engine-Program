#!/usr/bin/env bash
# Packt Player + Assets + SampleProject (ohne Editor/Qt)
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
OUT="${1:-$ROOT/dist/RPGMaker3D-Player}"
mkdir -p "$OUT"
# Binaries (anpassen je nach Build-Ordner)
for b in RPGMaker3D_Player RPGMaker3D_Player.exe; do
  if [[ -f "$ROOT/build/Release/$b" ]]; then cp -f "$ROOT/build/Release/$b" "$OUT/"; fi
  if [[ -f "$ROOT/build/$b" ]]; then cp -f "$ROOT/build/$b" "$OUT/"; fi
done
cp -a "$ROOT/assets" "$OUT/" 2>/dev/null || true
cp -a "$ROOT/SampleProject" "$OUT/" 2>/dev/null || true
cp -a "$ROOT/ruby" "$OUT/" 2>/dev/null || true
# SDL dlls if present
cp -f "$ROOT/build/Release/"*.dll "$OUT/" 2>/dev/null || true
echo "Player-Paket: $OUT"
ls -la "$OUT"
