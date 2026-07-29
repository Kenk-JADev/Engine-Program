#include "rpgmaker3d/Input.h"
#include "rpgmaker3d/Logger.h"
#include <SDL.h>

#include <cmath>

namespace rpg {

namespace {
// ---- PAKET 44: Gamepad-Abbildung (moderne Belegung) ----------------------
// Steuerkreuz/linker Stick  -> Pfeiltasten (dominante Achse, Deadzone)
// A (unten)                 -> Enter  (Bestaetigen: Menue/Text/Interaktion)
// B (rechts) + Start        -> Escape (Zurueck/Menue)
// X + Schulter links        -> Q      (XP-Taste L)
// Y + Schulter rechts       -> Tab    (XP-Taste R)
// Rueckseite rechter Stick bleibt bewusst frei (Kamera-Freiheit).
constexpr float kStickDeadzone = 0.40f;    // Anteil des Vollausschlags
constexpr int   kPadRescanFrames = 90;     // ~1,5 s bei 60 fps

bool PadButton(_SDL_GameController* pad, int button) {
    return pad && SDL_GameControllerGetButton(
        pad, static_cast<SDL_GameControllerButton>(button)) != 0;
}
} // namespace

void Input::Update() {
    PollGamepad(); // PAKET 44: Pad-Anteil vor dem Snapshot aktualisieren
    // Kombinierter Stand (Tastatur || Pad) - ohne Pad ist das exakt der alte
    // reine Tastaturstand, das Kantenverhalten bleibt damit unveraendert.
    mPreviousKeys = mCombined;
    mCombined = mCurrentKeys;
    for (size_t i = 0; i < mCombined.size(); ++i)
        if (mPadKeys[i]) mCombined[i] = true;
    mPreviousMouse = mCurrentMouse;
    mMouseDelta = Vec2(0.0f);
    mMouseWheel = 0.0f;
}

bool Input::IsKeyDown(Key key) const {
    return mCombined[static_cast<size_t>(key)];
}

bool Input::IsKeyPressed(Key key) const {
    size_t i = static_cast<size_t>(key);
    return mCombined[i] && !mPreviousKeys[i];
}

bool Input::IsKeyReleased(Key key) const {
    size_t i = static_cast<size_t>(key);
    return !mCombined[i] && mPreviousKeys[i];
}

// ---------------------------------------------------------------------------
// PAKET 44: Gamepad-Polling. SDL_GameControllerUpdate() ist die offizielle
// Alternative zur SDL-Eventschleife - im Qt-eingebetteten Modus gibt es die
// SDL-Schleife nicht, darum laeuft alles ueber diesen Poll (im Player
// harmlos). Hotplug: GetAttached()==false -> schliessen und neu suchen.
// ---------------------------------------------------------------------------
void Input::PollGamepad() {
    SDL_GameControllerUpdate();

    if (mGamepad && !SDL_GameControllerGetAttached(mGamepad))
        CloseGamepad();

    if (!mGamepad) {
        mPadKeys.fill(false);
        if (mGamepadScanCooldown > 0) {
            --mGamepadScanCooldown;
        } else {
            TryOpenGamepad();
        }
        if (!mGamepad) return;
    }

    bool up    = PadButton(mGamepad, SDL_CONTROLLER_BUTTON_DPAD_UP);
    bool down  = PadButton(mGamepad, SDL_CONTROLLER_BUTTON_DPAD_DOWN);
    bool left  = PadButton(mGamepad, SDL_CONTROLLER_BUTTON_DPAD_LEFT);
    bool right = PadButton(mGamepad, SDL_CONTROLLER_BUTTON_DPAD_RIGHT);

    // Linker Stick: dominante Achse ueber Deadzone -> tile-artige Richtung
    const int ax = SDL_GameControllerGetAxis(mGamepad, SDL_CONTROLLER_AXIS_LEFTX);
    const int ay = SDL_GameControllerGetAxis(mGamepad, SDL_CONTROLLER_AXIS_LEFTY);
    const int dead = (int)(kStickDeadzone * 32767.0f);
    const int absX = std::abs(ax);
    const int absY = std::abs(ay);
    if (absX > dead || absY > dead) {
        if (absX >= absY) {
            if (ax > 0) right = true; else left = true;
        } else {
            if (ay > 0) down = true; else up = true;
        }
    }

    mPadKeys[static_cast<size_t>(Key::Up)]    = up;
    mPadKeys[static_cast<size_t>(Key::Down)]  = down;
    mPadKeys[static_cast<size_t>(Key::Left)]  = left;
    mPadKeys[static_cast<size_t>(Key::Right)] = right;
    mPadKeys[static_cast<size_t>(Key::Enter)] =
        PadButton(mGamepad, SDL_CONTROLLER_BUTTON_A);
    mPadKeys[static_cast<size_t>(Key::Escape)] =
        PadButton(mGamepad, SDL_CONTROLLER_BUTTON_B) ||
        PadButton(mGamepad, SDL_CONTROLLER_BUTTON_START);
    mPadKeys[static_cast<size_t>(Key::Q)] =
        PadButton(mGamepad, SDL_CONTROLLER_BUTTON_X) ||
        PadButton(mGamepad, SDL_CONTROLLER_BUTTON_LEFTSHOULDER);
    mPadKeys[static_cast<size_t>(Key::Tab)] =
        PadButton(mGamepad, SDL_CONTROLLER_BUTTON_Y) ||
        PadButton(mGamepad, SDL_CONTROLLER_BUTTON_RIGHTSHOULDER);
}

void Input::TryOpenGamepad() {
    mGamepadScanCooldown = kPadRescanFrames; // nicht jedes Frame neu scannen
    const int count = SDL_NumJoysticks();
    for (int i = 0; i < count; ++i) {
        if (!SDL_IsGameController(i)) continue;
        mGamepad = SDL_GameControllerOpen(i);
        if (mGamepad) {
            const char* name = SDL_GameControllerName(mGamepad);
            RPG_LOG_INFO(std::string("Gamepad verbunden: ") +
                         (name ? name : "unbekannt"));
            break;
        }
    }
}

void Input::CloseGamepad() {
    if (!mGamepad) return;
    SDL_GameControllerClose(mGamepad);
    mGamepad = nullptr;
    mPadKeys.fill(false);
    RPG_LOG_INFO("Gamepad getrennt");
}

bool Input::IsMouseDown(MouseButton button) const {
    return mCurrentMouse[static_cast<size_t>(button)];
}

bool Input::IsMousePressed(MouseButton button) const {
    size_t i = static_cast<size_t>(button);
    return mCurrentMouse[i] && !mPreviousMouse[i];
}

void Input::OnKeyChanged(Key key, bool down) {
    mCurrentKeys[static_cast<size_t>(key)] = down;
}

void Input::OnMouseChanged(MouseButton button, bool down) {
    mCurrentMouse[static_cast<size_t>(button)] = down;
}

void Input::OnMouseMoved(float x, float y) {
    Vec2 newPos(x, y);
    mMouseDelta = newPos - mMousePosition;
    mMousePosition = newPos;
}

void Input::OnMouseWheel(float delta) {
    mMouseWheel += delta;
}

} // namespace rpg
