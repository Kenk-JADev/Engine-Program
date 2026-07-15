#pragma once
// Maps Qt key codes (Qt::Key) to the engine's rpg::Key enum (editor-only).

#include "rpgmaker3d/Input.h"
#include <QKeyEvent>  // Qt::Key_A .. enums (QtCore)

namespace qt_editor {

inline rpg::Key MapQtKey(int qtKey) {
    using rpg::Key;
    // Qt::Key_A..Z sind 'A'..'Z' (ASCII), Key_0..9 ebenso
    if (qtKey >= Qt::Key_A && qtKey <= Qt::Key_Z)
        return static_cast<Key>(static_cast<int>(Key::A) + (qtKey - Qt::Key_A));
    if (qtKey >= Qt::Key_0 && qtKey <= Qt::Key_9)
        return static_cast<Key>(static_cast<int>(Key::Num0) + (qtKey - Qt::Key_0));

    switch (qtKey) {
        case Qt::Key_Escape: return Key::Escape;
        case Qt::Key_Space: return Key::Space;
        case Qt::Key_Return:
        case Qt::Key_Enter: return Key::Enter;
        case Qt::Key_Tab: return Key::Tab;
        case Qt::Key_Backspace: return Key::Backspace;
        case Qt::Key_Delete: return Key::Delete;
        case Qt::Key_Left: return Key::Left;
        case Qt::Key_Right: return Key::Right;
        case Qt::Key_Up: return Key::Up;
        case Qt::Key_Down: return Key::Down;
        case Qt::Key_Shift: return Key::LShift;
        case Qt::Key_Control: return Key::LCtrl;
        case Qt::Key_Alt: return Key::LAlt;
        case Qt::Key_F1: return Key::F1;
        case Qt::Key_F2: return Key::F2;
        case Qt::Key_F3: return Key::F3;
        case Qt::Key_F4: return Key::F4;
        case Qt::Key_F5: return Key::F5;
        case Qt::Key_F6: return Key::F6;
        case Qt::Key_F7: return Key::F7;
        case Qt::Key_F8: return Key::F8;
        case Qt::Key_F9: return Key::F9;
        case Qt::Key_F10: return Key::F10;
        case Qt::Key_F11: return Key::F11;
        case Qt::Key_F12: return Key::F12;
        default: return Key::Unknown;
    }
}

} // namespace qt_editor
