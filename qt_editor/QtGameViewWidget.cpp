#include "QtGameViewWidget.h"
#include "QtKeyMap.h"

#include "rpgmaker3d/Engine.h"
#include "rpgmaker3d/Window.h"
#include "rpgmaker3d/Input.h"
#include "rpgmaker3d/Scene.h"
#include "rpgmaker3d/Renderer.h"
#include "rpgmaker3d/Camera.h"
#include "rpgmaker3d/Raycast.h"
#include "rpgmaker3d/Map.h"
#include "rpgmaker3d/Command.h"
#include "rpgmaker3d/CommandHistory.h"
#ifdef RPGMAKER3D_ENABLE_RMLUI
#include "rpgmaker3d/RmlUiSystem.h"
#endif

#include <glad/gl.h>

#include <QOpenGLContext>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QWheelEvent>
#include <QInputMethodEvent>
#include <cmath>
#include <algorithm>

namespace qt_editor {

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
    setAttribute(Qt::WA_InputMethodEnabled, true);
}

void QtGameViewWidget::initializeGL() {
    if (mGladLoaded) return;

    if (!gladLoadGL(QtGlLoader())) {
        emit engineInitFailed(QStringLiteral("glad konnte die GL-Funktionen nicht laden."));
        return;
    }
    mGladLoaded = true;

    if (!mEngine->InitializeEmbedded(width(), height(), true)) {
        emit engineInitFailed(QStringLiteral("Engine::InitializeEmbedded ist fehlgeschlagen (siehe engine.log)."));
        return;
    }
    mEngineReady = true;
#ifdef RPGMAKER3D_ENABLE_RMLUI
    if (auto* rml = mEngine->GetRmlUi())
        rml->NotifyViewport(width(), height());
#endif
    emit engineReady();
}

void QtGameViewWidget::paintGL() {
    if (!mEngineReady) return;
    mEngine->Render();
}

void QtGameViewWidget::resizeGL(int w, int h) {
    if (mEngineReady) {
        mEngine->GetWindow().SetForeignSize(w, h);
#ifdef RPGMAKER3D_ENABLE_RMLUI
        if (auto* rml = mEngine->GetRmlUi())
            rml->NotifyViewport(w, h);
#endif
    }
}

// ---------------------------------------------------------------------------
// Input-Bruecke: Engine + RmlUi
// ---------------------------------------------------------------------------
void QtGameViewWidget::keyPressEvent(QKeyEvent* event) {
    if (!mEngineReady) return;
    if (event->isAutoRepeat()) return;

#ifdef RPGMAKER3D_ENABLE_RMLUI
    if (auto* rml = mEngine->GetRmlUi()) {
        // F9 immer an Rml (Toggle)
        if (event->key() == Qt::Key_F9) {
            rml->ProcessKeyQt(event->key(), true, static_cast<int>(event->modifiers()));
            return;
        }
        if (rml->IsVisible()) {
            rml->ProcessKeyQt(event->key(), true, static_cast<int>(event->modifiers()));
            // Text fuer Rml (Buchstaben)
            if (!event->text().isEmpty() && event->text()[0].isPrint()) {
                rml->ProcessTextInput(event->text().toUtf8().toStdString());
            }
        }
    }
#endif
    mEngine->GetInput().OnKeyChanged(MapQtKey(event->key()), true);
}

void QtGameViewWidget::keyReleaseEvent(QKeyEvent* event) {
    if (!mEngineReady) return;
    if (event->isAutoRepeat()) return;
#ifdef RPGMAKER3D_ENABLE_RMLUI
    if (auto* rml = mEngine->GetRmlUi()) {
        if (rml->IsVisible())
            rml->ProcessKeyQt(event->key(), false, static_cast<int>(event->modifiers()));
    }
#endif
    mEngine->GetInput().OnKeyChanged(MapQtKey(event->key()), false);
}

void QtGameViewWidget::inputMethodEvent(QInputMethodEvent* event) {
#ifdef RPGMAKER3D_ENABLE_RMLUI
    if (mEngineReady) {
        if (auto* rml = mEngine->GetRmlUi()) {
            if (rml->IsVisible() && !event->commitString().isEmpty()) {
                rml->ProcessTextInput(event->commitString().toUtf8().toStdString());
            }
        }
    }
#endif
    QOpenGLWidget::inputMethodEvent(event);
}

QVariant QtGameViewWidget::inputMethodQuery(Qt::InputMethodQuery query) const {
    return QOpenGLWidget::inputMethodQuery(query);
}

void QtGameViewWidget::mouseMoveEvent(QMouseEvent* event) {
    if (!mEngineReady) return;
    const QPointF p = event->position();
    mEngine->GetInput().OnMouseMoved(static_cast<float>(p.x()), static_cast<float>(p.y()));
#ifdef RPGMAKER3D_ENABLE_RMLUI
    if (auto* rml = mEngine->GetRmlUi()) {
        if (rml->IsVisible())
            rml->ProcessMouseMove(static_cast<int>(p.x()), static_cast<int>(p.y()),
                                  static_cast<int>(event->modifiers()));
    }
#endif
    // Drag-Paint
    if (mPainting && mPaintMode && !mEngine->IsPlaying()) {
        paintTileAtScreen(static_cast<float>(p.x()), static_cast<float>(p.y()));
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

    const QPointF p = event->position();
#ifdef RPGMAKER3D_ENABLE_RMLUI
    if (auto* rml = mEngine->GetRmlUi()) {
        if (rml->IsVisible()) {
            int btn = 0;
            if (event->button() == Qt::LeftButton) btn = 0;
            else if (event->button() == Qt::MiddleButton) btn = 1;
            else if (event->button() == Qt::RightButton) btn = 2;
            rml->ProcessMouseButton(btn, true, static_cast<int>(event->modifiers()));
        }
    }
#endif

    if (b == rpg::MouseButton::Left && !mEngine->IsPlaying()) {
        if (mPaintMode) {
            if (mBrushMode == 1) {
                int x=0,z=0;
                if (tryGroundHit(static_cast<float>(p.x()), static_cast<float>(p.y()), x, z)) {
                    if (!mRectHasFirst) {
                        mRectX0 = x; mRectZ0 = z; mRectHasFirst = true;
                    } else {
                        fillRect(mRectX0, mRectZ0, x, z);
                        mRectHasFirst = false;
                    }
                }
            } else {
                mPainting = true;
                paintTileAtScreen(static_cast<float>(p.x()), static_cast<float>(p.y()));
            }
        } else {
            // 3D-Entity-Selektion
            rpg::Camera& cam = mEngine->GetRenderer().GetCamera();
            const rpg::Ray ray = rpg::Raycast::ScreenPointToRay(cam,
                rpg::Vec2(static_cast<float>(p.x()), static_cast<float>(p.y())),
                rpg::Vec2(static_cast<float>(width()), static_cast<float>(height())));
            const rpg::RaycastHit hit = rpg::Raycast::PickEntity(ray, mEngine->GetScene(), 2000.0f);
            emit entityPicked(hit.hit ? static_cast<int>(hit.entity) : -1);
        }
    }
}

void QtGameViewWidget::mouseReleaseEvent(QMouseEvent* event) {
    if (!mEngineReady) return;
    rpg::MouseButton b = rpg::MouseButton::Count;
    if (event->button() == Qt::LeftButton) b = rpg::MouseButton::Left;
    else if (event->button() == Qt::RightButton) b = rpg::MouseButton::Right;
    else if (event->button() == Qt::MiddleButton) b = rpg::MouseButton::Middle;
    if (b != rpg::MouseButton::Count)
        mEngine->GetInput().OnMouseChanged(b, false);

#ifdef RPGMAKER3D_ENABLE_RMLUI
    if (auto* rml = mEngine->GetRmlUi()) {
        if (rml->IsVisible()) {
            int btn = 0;
            if (event->button() == Qt::LeftButton) btn = 0;
            else if (event->button() == Qt::MiddleButton) btn = 1;
            else if (event->button() == Qt::RightButton) btn = 2;
            rml->ProcessMouseButton(btn, false, static_cast<int>(event->modifiers()));
        }
    }
#endif

    if (b == rpg::MouseButton::Left) mPainting = false;
}

void QtGameViewWidget::wheelEvent(QWheelEvent* event) {
    if (!mEngineReady) return;
    const float delta = static_cast<float>(event->angleDelta().y()) / 120.0f;
    mEngine->GetInput().OnMouseWheel(delta);
#ifdef RPGMAKER3D_ENABLE_RMLUI
    if (auto* rml = mEngine->GetRmlUi()) {
        if (rml->IsVisible())
            rml->ProcessMouseWheel(delta, static_cast<int>(event->modifiers()));
    }
#endif
}

bool QtGameViewWidget::tryGroundHit(float sx, float sy, int& outX, int& outZ) {
    rpg::Camera& cam = mEngine->GetRenderer().GetCamera();
    const rpg::Ray ray = rpg::Raycast::ScreenPointToRay(cam,
        rpg::Vec2(sx, sy),
        rpg::Vec2(static_cast<float>(width()), static_cast<float>(height())));
    const auto hit = rpg::Raycast::IntersectPlane(ray, rpg::Vec3(0, 1, 0), rpg::Vec3(0, 0, 0));
    if (!hit.hit) return false;
    outX = static_cast<int>(std::floor(hit.point.x));
    outZ = static_cast<int>(std::floor(hit.point.z));
    return true;
}

void QtGameViewWidget::paintTileAtScreen(float sx, float sy) {
    int x = 0, z = 0;
    if (!tryGroundHit(sx, sy, x, z)) return;
    auto& map = mEngine->GetMap();
    if (x < 0 || z < 0 || x >= map.GetWidth() || z >= map.GetHeight()) return;
    const int layer = mPaintLayer;
    if (layer < 0 || layer >= static_cast<int>(map.GetLayers().size())) return;
    const int oldTile = map.GetTile(layer, x, z);
    const int newTile = mPaintTile; // -1 = eraser
    if (oldTile == newTile) return;
    auto cmd = std::make_shared<rpg::SetTileCommand>(layer, x, z, oldTile, newTile);
    mEngine->GetCommandHistory().Execute(*mEngine, cmd);
    emit tilePainted(x, z, newTile);
}

void QtGameViewWidget::fillRect(int x0, int z0, int x1, int z1) {
    if (!mEngine) return;
    auto& map = mEngine->GetMap();
    if (x0 > x1) std::swap(x0, x1);
    if (z0 > z1) std::swap(z0, z1);
    x0 = std::max(0, x0); z0 = std::max(0, z0);
    x1 = std::min(map.GetWidth() - 1, x1);
    z1 = std::min(map.GetHeight() - 1, z1);
    const int layer = mPaintLayer;
    if (layer < 0 || layer >= static_cast<int>(map.GetLayers().size())) return;
    int painted = 0;
    for (int z = z0; z <= z1; ++z) {
        for (int x = x0; x <= x1; ++x) {
            const int oldTile = map.GetTile(layer, x, z);
            if (oldTile == mPaintTile) continue;
            auto cmd = std::make_shared<rpg::SetTileCommand>(layer, x, z, oldTile, mPaintTile);
            mEngine->GetCommandHistory().Execute(*mEngine, cmd);
            ++painted;
        }
    }
    if (painted > 0)
        emit tilePainted(x0, z0, mPaintTile);
}

} // namespace qt_editor
