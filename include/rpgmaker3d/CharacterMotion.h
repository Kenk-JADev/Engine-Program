#pragma once
// RPG Maker 3D - CharacterMotion (PAKET 29)
//
// Zeitbasierte (nicht framebasierte) Kachelbewegung fuer Charaktere,
// nach dem Verhaltensmodell von RPG Maker XP, neu implementiert fuer 3D.
//
// Was XP fest verdrahtet hatte und hier sauber/verbessert ist:
//   - XP zaehlte Bewegungen in FRAMES bei festen 40 fps (RGSS
//     Game_Character-Modell). Window-resize, VSYNC oder Langsamlauf
//     veraenderten das Spieltempo. Hier laeuft alles ueber dt (Sekunden)
//     und ist damit framerate-unabhaengig.
//   - XP kannte keine echte Hoehe: Spruenge waren eine flache
//     Parabel im 2D-Sprite. Hier bewegt der Sprung die echte Y-Achse.
//   - XP "Annaehern"-Bewegung war gierig in 4 Richtungen und blieb an
//     jeder Kante haengen. Die Engine nutzt (EventSystem) eine begrenzte
//     Breitensuche, damit NPCs um Hindernisse herumlaufen.
//
// Tabellen (dokumentiertes XP-Verhalten, 40-fps-Bezug):
//   Geschwindigkeit 1..6  -> Sekunden pro Kachel:
//       1=1.60  2=0.80  3=0.40  4=0.20  5=0.10  6=0.05
//     (Stufe 4 = "Normal" = 5 Kacheln/s, wie der XP-Spieler)
//   Haeufigkeit 1..6 -> Pause zwischen autonomen Zuegen / Route-Loops:
//       (7 - Stufe) * 0.25s  =>  1.50s .. 0.25s

#include "Types.h"

namespace rpg {

namespace xp {

/// Sekunden, die ein Charakter mit Geschwindigkeitsstufe 1..6 fuer eine
/// Kachel braucht (XP-Bezug: 64/32/16/8/4/2 Frames bei 40 fps).
float TileSecondsForSpeed(int speed1to6);

/// Kacheln pro Sekunde (Kehrwert von TileSecondsForSpeed).
float TilesPerSecondForSpeed(int speed1to6);

/// Rueckkehrende Pause nach Haeufigkeitsstufe 1..6 (Sekunden).
/// Dient autonomen Zuegen UND der Route-Loop-Pause.
float PauseSecondsForFrequency(int frequency1to6);

/// XP-Richtungsfeld fuer einen Delta-Vektor (2/4/6/8; dominant-Achse).
int  CardinalDirFromDelta(const Vec3& delta, int fallback = 2);

/// XP-Regel fuer diagonale Bewegungen: bevorzugte Blickrichtung ist die
/// HORIZONTALE Achse des Schritts, ausser die aktuelle Richtung liegt
/// schon auf der vertikalen Achse des Schritts (dann bleibt sie).
int  DiagonalDir(const Vec3& delta, int currentDir);

/// XP-"Ecke-schneiden"-Regel bei Diagonalen: Der Diagonal-Schritt ist nur
/// erlaubt, wenn beide orthogonalen Zwischenzellen passierbar sind.
/// `passBlocked(cellTarget)` muss den Target-Mittelpunkt auf Belegschaft
/// pruefen (Tile + Events + Spieler).
template <typename F> bool DiagonalPassable(const Vec3& pos, const Vec3& delta, F&& cellBlocked) {
    const Vec3 stepH(pos.x + (delta.x != 0.0f ? (delta.x > 0.0f ? 1.0f : -1.0f) : 0.0f), 0.0f, pos.z);
    const Vec3 stepV(pos.x, 0.0f, pos.z + (delta.z != 0.0f ? (delta.z > 0.0f ? 1.0f : -1.0f) : 0.0f));
    const Vec3 stepD(pos.x + delta.x, pos.y, pos.z + delta.z);
    if (cellBlocked(stepH)) return false;
    if (cellBlocked(stepV)) return false;
    if (cellBlocked(stepD)) return false;
    return true;
}

} // namespace xp

// ---------------------------------------------------------------------------
// CharacterMotion: laufender Kachelschritt ODER Sprung eines Charakters.
// Der logische Zustand (Rasterzelle, Richtung) liegt weiter im Besitz des
// Aufrufers; CharacterMotion liefert nur die interpolierte Sichtposition.
// ---------------------------------------------------------------------------
class CharacterMotion {
public:
    bool   IsActive() const { return mActive; }
    bool   IsJumping() const { return mActive && mJump; }
    const Vec3& Target() const { return mTo; }
    /// Pause, die der Aufrufer NACH Abschluss einlegen soll (Route-Rhythmus).
    float  PauseAfter() const { return mPauseAfter; }
    void   SetPauseAfter(float seconds) { mPauseAfter = seconds; }

    /// Startet einen Kachelschritt von `from` um `delta`.
    /// Dauer = Geschwindigkeitstabelle (Diagonal: dieselbe Dauer wie
    /// Kardinal - XP-Charakteristik, Kachelausschnitt bleibt rhythmisch).
    bool BeginStep(const Vec3& from, const Vec3& delta, int speed1to6);

    /// Startet einen Sprung von `from` nach `target`. Dauer waechst mit
    /// der Distanz (Kacheln), die Hoehe ist eine Parabel auf echter Y-Achse.
    bool BeginJump(const Vec3& from, const Vec3& target, int speed1to6);

    /// Bricht die Bewegung ab; Sichtposition springt auf `snapTo`.
    void Cancel(const Vec3& snapTo);

    /// Schreibt die aktuelle Sichtposition (inkl. Sprunghoehe) in outWorld.
    /// Rueckgabe: true solange die Bewegung laeuft, false im Endframe
    /// (dann ist outWorld == Ziel, exakt auf groundY).
    bool Update(float dt, Vec3& outWorld, float groundY = 0.0f);

private:
    Vec3 EvaluateCurve(float t, float groundY) const;

    Vec3  mFrom{0.0f};
    Vec3  mTo{0.0f};
    float mElapsed = 0.0f;
    float mDuration = 0.2f;
    float mArcHeight = 0.0f;
    float mPauseAfter = 0.0f;
    bool  mJump = false;
    bool  mActive = false;
};

} // namespace rpg
