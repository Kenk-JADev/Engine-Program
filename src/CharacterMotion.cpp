// RPG Maker 3D - CharacterMotion (PAKET 29) - siehe CharacterMotion.h
#include "rpgmaker3d/CharacterMotion.h"
#include <algorithm>
#include <cmath>

namespace rpg {

namespace xp {

float TileSecondsForSpeed(int speed1to6) {
    // Index = Stufe-1. Dokumentiertes XP-Verhalten (40-fps-Bezug):
    // Stufe 1 = 64 Frames/Kachel .. Stufe 6 = 2 Frames/Kachel.
    static const float kTable[6] = { 1.60f, 0.80f, 0.40f, 0.20f, 0.10f, 0.05f };
    const int s = std::clamp(speed1to6, 1, 6);
    return kTable[s - 1];
}

float TilesPerSecondForSpeed(int speed1to6) {
    return 1.0f / TileSecondsForSpeed(speed1to6);
}

float PauseSecondsForFrequency(int frequency1to6) {
    const int f = std::clamp(frequency1to6, 1, 6);
    return (7 - f) * 0.25f; // 1.50s .. 0.25s
}

int CardinalDirFromDelta(const Vec3& delta, int fallback) {
    if (std::fabs(delta.x) < 1e-6f && std::fabs(delta.z) < 1e-6f) return fallback;
    // dominant-Achse (2=unten/+Z, 4=links/-X, 6=rechts/+X, 8=oben/-Z)
    if (std::fabs(delta.x) >= std::fabs(delta.z))
        return delta.x > 0.0f ? 6 : 4;
    return delta.z > 0.0f ? 2 : 8;
}

int DiagonalDir(const Vec3& delta, int currentDir) {
    const int horiz = delta.x > 0.0f ? 6 : 4;  // 6=rechts, 4=links
    const int vert  = delta.z > 0.0f ? 2 : 8;  // 2=unten, 8=oben
    // XP-Regel: horizontale Achse bevorzugen; steht die Richtung aber schon
    // auf der VERTIKALEN Achse dieses Schritts, bleibt sie stehen.
    if (currentDir == vert) return vert;
    return horiz;
}

} // namespace xp

// ---------------------------------------------------------------------------
bool CharacterMotion::BeginStep(const Vec3& from, const Vec3& delta, int speed1to6) {
    mFrom = from;
    mTo = from + delta;
    mTo.y = mFrom.y; // Kachelschritte bleiben plan (Hoehe = Boden)
    mDuration = xp::TileSecondsForSpeed(speed1to6);
    mElapsed = 0.0f;
    mArcHeight = 0.0f;
    mJump = false;
    mActive = true;
    mPauseAfter = 0.0f;
    return true;
}

bool CharacterMotion::BeginJump(const Vec3& from, const Vec3& target, int speed1to6) {
    mFrom = from;
    mTo = target;
    mTo.y = mFrom.y;
    const Vec3 flat(mTo.x - mFrom.x, 0.0f, mTo.z - mFrom.z);
    const float dist = glm::length(flat);
    // Sprungdauer skaliert mit der Distanz (ein Kurz-Hop soll nicht träge
    // wirken); Grunddauer an der Speed-Tabelle ausgerichtet.
    const float base = xp::TileSecondsForSpeed(speed1to6);
    mDuration = std::clamp(0.20f + dist * std::max(base, 0.10f) * 0.60f, 0.25f, 0.90f);
    mElapsed = 0.0f;
    mArcHeight = std::min(0.45f + dist * 0.12f, 1.10f);
    mJump = true;
    mActive = true;
    mPauseAfter = 0.0f;
    return true;
}

void CharacterMotion::Cancel(const Vec3& snapTo) {
    mActive = false;
    mJump = false;
    mFrom = mTo = snapTo;
    mElapsed = mDuration = 0.0f;
    mArcHeight = 0.0f;
    mPauseAfter = 0.0f;
}

Vec3 CharacterMotion::EvaluateCurve(float t, float groundY) const {
    Vec3 p;
    // XZ linear (konstante Schrittgeschwindigkeit - der Charakter
    // "rutscht" nicht; leichte Ease-Kurve wuerde den 3D-Boden
    // unkonstant schnell abfahren).
    p.x = mFrom.x + (mTo.x - mFrom.x) * t;
    p.z = mFrom.z + (mTo.z - mFrom.z) * t;
    float y = mFrom.y + (mTo.y - mFrom.y) * t;
    if (mJump) {
        // Parabelbogen auf echter Y-Achse (XP-Stil, physisch sichtbar).
        y = groundY + mArcHeight * (4.0f * t * (1.0f - t));
    }
    p.y = y;
    return p;
}

bool CharacterMotion::Update(float dt, Vec3& outWorld, float groundY) {
    if (!mActive) {
        outWorld = mTo;
        return false;
    }
    mElapsed += dt;
    if (mElapsed >= mDuration) {
        mActive = false;
        mJump = false;
        outWorld = mTo;
        outWorld.y = mTo.y; // Landeposition exakt (kein float-Drift auf Y)
        return false;
    }
    const float t = mDuration > 0.0f ? mElapsed / mDuration : 1.0f;
    outWorld = EvaluateCurve(std::clamp(t, 0.0f, 1.0f), groundY);
    return true;
}

} // namespace rpg
