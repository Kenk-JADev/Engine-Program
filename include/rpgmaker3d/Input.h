#pragma once

#include <array>
#include "Types.h"

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

private:
    std::array<bool, static_cast<size_t>(Key::Count)> mCurrentKeys{};
    std::array<bool, static_cast<size_t>(Key::Count)> mPreviousKeys{};
    std::array<bool, static_cast<size_t>(MouseButton::Count)> mCurrentMouse{};
    std::array<bool, static_cast<size_t>(MouseButton::Count)> mPreviousMouse{};
    Vec2 mMousePosition{0.0f};
    Vec2 mMouseDelta{0.0f};
    float mMouseWheel = 0.0f;
};

} // namespace rpg
