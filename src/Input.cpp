#include "rpgmaker3d/Input.h"

namespace rpg {

void Input::Update() {
    mPreviousKeys = mCurrentKeys;
    mPreviousMouse = mCurrentMouse;
    mMouseDelta = Vec2(0.0f);
    mMouseWheel = 0.0f;
}

bool Input::IsKeyDown(Key key) const {
    return mCurrentKeys[static_cast<size_t>(key)];
}

bool Input::IsKeyPressed(Key key) const {
    size_t i = static_cast<size_t>(key);
    return mCurrentKeys[i] && !mPreviousKeys[i];
}

bool Input::IsKeyReleased(Key key) const {
    size_t i = static_cast<size_t>(key);
    return !mCurrentKeys[i] && mPreviousKeys[i];
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
