#include "QtGameViewWidget.h"
#include "QtKeyMap.h"

#include "rpgmaker3d/Engine.h"
#include "rpgmaker3d/Window.h"
#include "rpgmaker3d/Input.h"
#include "rpgmaker3d/Scene.h"
#include "rpgmaker3d/Renderer.h"
#include "rpgmaker3d/Camera.h"
#include "rpgmaker3d/Raycast.h"
#include "rpgmaker3d/RmlUiSystem.h"

#ifdef RPGMAKER3D_ENABLE_RMLUI
#include <RmlUi/Core/Input.h>
#endif

#include <glad/gl.h>

#include <QOpenGLContext>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QWheelEvent>

#include <cmath>

namespace qt_editor {

#ifdef RPGMAKER3D_ENABLE_RMLUI
// Qt-Modifier -> Rml::Input::KeyModifier-Bitmasken
static int QtModsToRml(Qt::KeyboardModifiers mods) {
    int r = 0;
    if (mods & Qt::ShiftModifier)   r |= Rml::Input::KM_SHIFT;
    if (mods & Qt::ControlModifier) r |= Rml::Input::KM_CTRL;
    if (mods & Qt::AltModifier)     r |= Rml::Input::KM_ALT;
    if (mods & Qt::MetaModifier)    r |= Rml::Input::KM_META;
    return r;
}

// Qt::Key -> Rml::Input::KeyIdentifier (0 = nicht gemappt)
static int QtKeyToRmlKey(int qtKey) {
    namespace RI = Rml::Input;
    if (qtKey >= Qt::Key_A && qtKey <= Qt::Key_Z) return RI::KI_A + (qtKey - Qt::Key_A);
    if (qtKey >= Qt::Key_0 && qtKey <= Qt::Key_9) return RI::KI_0 + (qtKey - Qt::Key_0);
    if (qtKey >= Qt::Key_F1 && qtKey <= Qt::Key_F12) return RI::KI_F1 + (qtKey - Qt::Key_F1);
    switch (qtKey) {
        case Qt::Key_Escape: return RI::KI_ESCAPE;
        case Qt::Key_Space: return RI::KI_SPACE;
        case Qt::Key_Return:
        case Qt::Key_Enter: return RI::KI_RETURN;
        case Qt::Key_Tab: return RI::KI_TAB;
        case Qt::Key_Backspace: return RI::KI_BACK;
        case Qt::Key_Delete: return RI::KI_DELETE;
        case Qt::Key_Left: return RI::KI_LEFT;
        case Qt::Key_Right: return RI::KI_RIGHT;
        case Qt::Key_Up: return RI::KI_UP;
        case Qt::Key_Down: return RI::KI_DOWN;
        case Qt::Key_Shift: return RI::KI_LSHIFT;
        case Qt::Key_Control: return RI::KI_LCONTROL;
        case Qt::Key_Alt: return RI::KI_LMENU;
        default: return 0;
    }
}

// RmlUi-Button-Reihenfolge (wie im SDL-Pfad): 0=Links 1=Mitte 2=Rechts
static int QtButtonToRml(Qt::MouseButton b) {
    if (b == Qt::LeftButton) return 0;
    if (b == Qt::MiddleButton) return 1;
    if (b == Qt::RightButton) return 2;
    return -1;
}
#endif // RPGMAKER3D_ENABLE_RMLUI

// glad-Loader ueber den aktuellen Qt-Kontext.
// Hinweis: GLADloadfunc erwartet GLADapiproc (fn-ptr) als Rueckgabetyp;
// QOpenGLContext::getProcAddress liefert QFunctionPointer. Auf x64 (einzige
// Zielplattform) identische Aufrufkonvention -> reinterpret_cast ist sicher.
static void* QtGlLoaderRaw(const char* name) {
    QOpenGLContext* ctx = QOpenGLContext::currentContext();
    return ctx ? reinterpret_cast<void*>(ctx->getProcAddress(name)) : nullptr;
}
static GLADloadfunc QtGlLoader() {
    return reinterpret_cast<GLADloadfunc>(&QtGlLoaderRaw);
}

QtGameViewWidget::QtGameViewWidget(rpg::Engine* engine, QWidget* parent)
    : QOpenGLWidget(parent), mEngine(engine) {
    setFocusPolicy(Qt::StrongFocus);
    setMouseTracking(true);
    setMinimumSize(640, 360);
}

void QtGameViewWidget::initializeGL() {
    if (mGladLoaded) return;

    if (!gladLoadGL(QtGlLoader())) {
        emit engineInitFailed(QStringLiteral("glad konnte die GL-Funktionen nicht laden."));
        return;
    }
    mGladLoaded = true;

    // Engine im Embedded-Modus: Qt besitzt Fenster + Kontext.
    if (!mEngine->InitializeEmbedded(width(), height(), true)) {
        emit engineInitFailed(QStringLiteral("Engine::InitializeEmbedded ist fehlgeschlagen (siehe engine.log)."));
        return;
    }
    mEngineReady = true;
    emit engineReady();
}

void QtGameViewWidget::paintGL() {
    if (!mEngineReady) return;
    // Engine rendert kompletten Frame (3D + RmlUi-Overlay); Qt swapped danach.
    mEngine->Render();
}

void QtGameViewWidget::resizeGL(int w, int h) {
    if (mEngineReady) {
        mEngine->GetWindow().SetForeignSize(w, h);
#ifdef RPGMAKER3D_ENABLE_RMLUI
        if (auto* ui = mEngine->GetRmlUi())
            ui->SetContextSize(w, h); // Kontext-Dimensionen + Viewport nachziehen
#endif
    }
}

// ---------------------------------------------------------------------------
// Input-Bruecke in die Engine
// ---------------------------------------------------------------------------
void QtGameViewWidget::keyPressEvent(QKeyEvent* event) {
    if (!mEngineReady) return;
    if (event->isAutoRepeat()) return;
#ifdef RPGMAKER3D_ENABLE_RMLUI
    if (auto* ui = mEngine->GetRmlUi()) {
        const int rmlKey = QtKeyToRmlKey(event->key());
        bool captured = false;
        if (rmlKey != 0)
            captured = ui->InjectKey(rmlKey, true, QtModsToRml(event->modifiers()));
        const std::string text = event->text().toStdString();
        if (!text.empty())
            captured = ui->InjectText(text.c_str()) || captured;
        if (captured) return; // UI hat die Taste verarbeitet (z.B. F9-Toggle)
    }
#endif
    mEngine->GetInput().OnKeyChanged(MapQtKey(event->key()), true);
}

void QtGameViewWidget::keyReleaseEvent(QKeyEvent* event) {
    if (!mEngineReady) return;
    if (event->isAutoRepeat()) return;
#ifdef RPGMAKER3D_ENABLE_RMLUI
    if (auto* ui = mEngine->GetRmlUi()) {
        const int rmlKey = QtKeyToRmlKey(event->key());
        if (rmlKey != 0 &&
            ui->InjectKey(rmlKey, false, QtModsToRml(event->modifiers())))
            return;
    }
#endif
    mEngine->GetInput().OnKeyChanged(MapQtKey(event->key()), false);
}

void QtGameViewWidget::mouseMoveEvent(QMouseEvent* event) {
    if (!mEngineReady) return;
    const QPointF p = event->position();
#ifdef RPGMAKER3D_ENABLE_RMLUI
    if (auto* ui = mEngine->GetRmlUi()) {
        if (ui->InjectMouseMove(static_cast<int>(p.x()), static_cast<int>(p.y()),
                                QtModsToRml(event->modifiers())))
            return; // Kamera/3D-Interaktion unter dem HUD pausieren
    }
#endif
    mEngine->GetInput().OnMouseMoved(static_cast<float>(p.x()), static_cast<float>(p.y()));

    // Drag-Malen: linke Taste gehalten + Paint/Erase-Modus (nicht im Playtest)
    if (mViewMode != ViewMode::Select && !mEngine->IsPlaying() &&
        (event->buttons() & Qt::LeftButton)) {
        emitGroundHit(p);
    }
}

void QtGameViewWidget::mousePressEvent(QMouseEvent* event) {
    if (!mEngineReady) return;
#ifdef RPGMAKER3D_ENABLE_RMLUI
    if (auto* ui = mEngine->GetRmlUi()) {
        const int rb = QtButtonToRml(event->button());
        if (rb >= 0 && ui->InjectMouseButton(rb, true, QtModsToRml(event->modifiers())))
            return; // Klick landete auf dem HUD -> nicht in die 3D-Welt
    }
#endif
    rpg::MouseButton b = rpg::MouseButton::Count;
    if (event->button() == Qt::LeftButton) b = rpg::MouseButton::Left;
    else if (event->button() == Qt::RightButton) b = rpg::MouseButton::Right;
    else if (event->button() == Qt::MiddleButton) b = rpg::MouseButton::Middle;
    if (b != rpg::MouseButton::Count)
        mEngine->GetInput().OnMouseChanged(b, true);

    // Rechtsklick: Startposition merken (Klick-vs-Drag fuer Kontextmenue)
    if (event->button() == Qt::RightButton)
        mRightPressPos = event->position();

    // Linksklick (nicht waehrend Playtest): je nach Modus Entity-Pick
    // (Select) oder Boden-Treffer fuer Tile-Malen/Radieren (Paint/Erase).
    if (b == rpg::MouseButton::Left && !mEngine->IsPlaying()) {
        const QPointF p = event->position();
        if (mViewMode == ViewMode::Select) {
            // Ray aus der Editor-Kamera durch den Klickpunkt, naechste Entity
            rpg::Camera& cam = mEngine->GetRenderer().GetCamera();
            const rpg::Ray ray = rpg::Raycast::ScreenPointToRay(cam,
                rpg::Vec2(static_cast<float>(p.x()), static_cast<float>(p.y())),
                rpg::Vec2(static_cast<float>(width()), static_cast<float>(height())));
            const rpg::RaycastHit hit = rpg::Raycast::PickEntity(ray, mEngine->GetScene(), 2000.0f);
            emit entityPicked(hit.hit ? static_cast<int>(hit.entity) : -1);
        } else {
            emitGroundHit(p);
        }
    }
}

// Drag-Malen: mouseMoveEvent ruft dies bei gedrueckter linker Taste
void QtGameViewWidget::emitGroundHit(const QPointF& pos) {
    rpg::Camera& cam = mEngine->GetRenderer().GetCamera();
    const rpg::Ray ray = rpg::Raycast::ScreenPointToRay(cam,
        rpg::Vec2(static_cast<float>(pos.x()), static_cast<float>(pos.y())),
        rpg::Vec2(static_cast<float>(width()), static_cast<float>(height())));
    // Bodenebene y=0 (wie Editor::HandleSceneViewPicking)
    const rpg::RaycastHit plane = rpg::Raycast::IntersectPlane(ray,
        rpg::Vec3(0, 1, 0), rpg::Vec3(0, 0, 0));
    if (plane.hit)
        emit groundClicked(plane.point.x, plane.point.z);
}

void QtGameViewWidget::mouseReleaseEvent(QMouseEvent* event) {
    if (!mEngineReady) return;
#ifdef RPGMAKER3D_ENABLE_RMLUI
    if (auto* ui = mEngine->GetRmlUi()) {
        const int rb = QtButtonToRml(event->button());
        if (rb >= 0 && ui->InjectMouseButton(rb, false, QtModsToRml(event->modifiers())))
            return;
    }
#endif
    rpg::MouseButton b = rpg::MouseButton::Count;
    if (event->button() == Qt::LeftButton) b = rpg::MouseButton::Left;
    else if (event->button() == Qt::RightButton) b = rpg::MouseButton::Right;
    else if (event->button() == Qt::MiddleButton) b = rpg::MouseButton::Middle;
    if (b != rpg::MouseButton::Count)
        mEngine->GetInput().OnMouseChanged(b, false);

    // Rechts-Klick (kein Drag, < 6px) auf den Boden -> Kontextmenue-Signal.
    // Rechts-DRAG bleibt Kamera-Orbit (unveraendert in Engine::Update).
    if (event->button() == Qt::RightButton && !mEngine->IsPlaying() &&
        mRightPressPos.x() >= 0.0) {
        const QPointF d = event->position() - mRightPressPos;
        mRightPressPos = QPointF(-1.0, -1.0);
        if (std::abs(d.x()) < 6.0 && std::abs(d.y()) < 6.0) {
            rpg::Camera& cam = mEngine->GetRenderer().GetCamera();
            const QPointF p = event->position();
            const rpg::Ray ray = rpg::Raycast::ScreenPointToRay(cam,
                rpg::Vec2(static_cast<float>(p.x()), static_cast<float>(p.y())),
                rpg::Vec2(static_cast<float>(width()), static_cast<float>(height())));
            const rpg::RaycastHit plane = rpg::Raycast::IntersectPlane(ray,
                rpg::Vec3(0, 1, 0), rpg::Vec3(0, 0, 0));
            if (plane.hit) {
                const QPoint g = event->globalPosition().toPoint();
                emit groundContextMenu(g.x(), g.y(), plane.point.x, plane.point.z);
            }
        }
    }
}

void QtGameViewWidget::wheelEvent(QWheelEvent* event) {
    if (!mEngineReady) return;
#ifdef RPGMAKER3D_ENABLE_RMLUI
    if (auto* ui = mEngine->GetRmlUi()) {
        const float delta = static_cast<float>(event->angleDelta().y()) / 120.0f;
        if (ui->InjectMouseWheel(delta, QtModsToRml(event->modifiers())))
            return;
    }
#endif
    const float delta = static_cast<float>(event->angleDelta().y()) / 120.0f;
    mEngine->GetInput().OnMouseWheel(delta);
}

} // namespace qt_editor
