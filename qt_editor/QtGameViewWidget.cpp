#include "QtGameViewWidget.h"
#include "QtKeyMap.h"

#include "rpgmaker3d/Engine.h"
#include "rpgmaker3d/Window.h"
#include "rpgmaker3d/Input.h"
#include "rpgmaker3d/Scene.h"
#include "rpgmaker3d/Renderer.h"
#include "rpgmaker3d/Camera.h"
#include "rpgmaker3d/Raycast.h"

#include <glad/gl.h>

#include <QOpenGLContext>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QWheelEvent>

namespace qt_editor {

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
    }
}

// ---------------------------------------------------------------------------
// Input-Bruecke in die Engine
// ---------------------------------------------------------------------------
void QtGameViewWidget::keyPressEvent(QKeyEvent* event) {
    if (!mEngineReady) return;
    if (event->isAutoRepeat()) return;
    mEngine->GetInput().OnKeyChanged(MapQtKey(event->key()), true);
}

void QtGameViewWidget::keyReleaseEvent(QKeyEvent* event) {
    if (!mEngineReady) return;
    if (event->isAutoRepeat()) return;
    mEngine->GetInput().OnKeyChanged(MapQtKey(event->key()), false);
}

void QtGameViewWidget::mouseMoveEvent(QMouseEvent* event) {
    if (!mEngineReady) return;
    const QPointF p = event->position();
    mEngine->GetInput().OnMouseMoved(static_cast<float>(p.x()), static_cast<float>(p.y()));

    // Drag-Malen: linke Taste gehalten + Paint/Erase-Modus (nicht im Playtest)
    if (mViewMode != ViewMode::Select && !mEngine->IsPlaying() &&
        (event->buttons() & Qt::LeftButton)) {
        emitGroundHit(p);
    }
}

void QtGameViewWidget::mousePressEvent(QMouseEvent* event) {
    if (!mEngineReady) return;
    rpg::MouseButton b = rpg::MouseButton::Count;
    if (event->button() == Qt::LeftButton) b = rpg::MouseButton::Left;
    else if (event->button() == Qt::RightButton) b = rpg::MouseButton::Right;
    else if (event->button() == Qt::MiddleButton) b = rpg::MouseButton::Middle;
    if (b != rpg::MouseButton::Count)
        mEngine->GetInput().OnMouseChanged(b, true);

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
    rpg::MouseButton b = rpg::MouseButton::Count;
    if (event->button() == Qt::LeftButton) b = rpg::MouseButton::Left;
    else if (event->button() == Qt::RightButton) b = rpg::MouseButton::Right;
    else if (event->button() == Qt::MiddleButton) b = rpg::MouseButton::Middle;
    if (b != rpg::MouseButton::Count)
        mEngine->GetInput().OnMouseChanged(b, false);
}

void QtGameViewWidget::wheelEvent(QWheelEvent* event) {
    if (!mEngineReady) return;
    const float delta = static_cast<float>(event->angleDelta().y()) / 120.0f;
    mEngine->GetInput().OnMouseWheel(delta);
}

} // namespace qt_editor
