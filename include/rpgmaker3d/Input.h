#pragma once

#include <array>
#include "Types.h"

struct _SDL_GameController; // SDL-Vorwaertsdeklaration (PAKET 44: Gamepad-Polling)

namespace rpg {

enum class Key {
    Unknown = 0,
    A, B, C, D, E, F, G, H, I, J, K, L, M,
    N, O, P, Q, R, S, T, U, V, W, X, Y, Z,
    Num0, Num1, Num2, Num3, Num4, Num5, Num6, Num7, Num8, Num9,
    Escape, Space, Enter, Tab, Backspace, Delete,
    Left, Right, Up, Down,
    LShift, LCtrl, LAlt,
    F1, F2, F3, F4, F5, F6, F7, F8, F9, F10, F11, F12,
    Count
};

enum class MouseButton {
    Left = 0,
    Right,
    Middle,
    Count
};

class Input {
public:
    void Update();

    bool IsKeyDown(Key key) const;
    bool IsKeyPressed(Key key) const;
    bool IsKeyReleased(Key key) const;

    bool IsMouseDown(MouseButton button) const;
    bool IsMousePressed(MouseButton button) const;
    Vec2 GetMousePosition() const { return mMousePosition; }
    Vec2 GetMouseDelta() const { return mMouseDelta; }
    float GetMouseWheel() const { return mMouseWheel; }

    void OnKeyChanged(Key key, bool down);
    void OnMouseChanged(MouseButton button, bool down);
    void OnMouseMoved(float x, float y);
    void OnMouseWheel(float delta);

    // ---- PAKET 44: Gamepad (SDL_GameController) ----
    // Das Pad ergaenzt die Tastatur PARALLEL: Abfragen liefern
    // (Tastatur || Pad). Gepollt wird am Anfang von Update() - kein
    // zusaetzlicher Aufruf noetig, Hotplug wird dort erkannt.
    bool IsGamepadConnected() const { return mGamepad != nullptr; }

private:
    void PollGamepad();
    void TryOpenGamepad();
    void CloseGamepad();

    // Tastatur-Rohzustand (Events via OnKeyChanged) ...
    std::array<bool, static_cast<size_t>(Key::Count)> mCurrentKeys{};
    // ... kombinierter Zustand (Tastatur || Pad) des LETZTEN Frames
    std::array<bool, static_cast<size_t>(Key::Count)> mPreviousKeys{};
    // PAKET 44: Pad-Anteil (letzter Poll) + der kombinierte Stand
    std::array<bool, static_cast<size_t>(Key::Count)> mPadKeys{};
    std::array<bool, static_cast<size_t>(Key::Count)> mCombined{};
    _SDL_GameController* mGamepad = nullptr;
    int mGamepadScanCooldown = 0; // Frames bis zur naechsten Pad-Suche

    std::array<bool, static_cast<size_t>(MouseButton::Count)> mCurrentMouse{};
    std::array<bool, static_cast<size_t>(MouseButton::Count)> mPreviousMouse{};
    Vec2 mMousePosition{0.0f};
    Vec2 mMouseDelta{0.0f};
    float mMouseWheel = 0.0f;
};

} // namespace rpg
