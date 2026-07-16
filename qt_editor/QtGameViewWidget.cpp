// WICHTIG: glad VOR Qt/OpenGL-Headern, sonst gl.h-Doppelinclude.
#include <glad/gl.h>

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

#include <QOpenGLContext>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QWheelEvent>
#include <QInputMethodEvent>
#include <cmath>
#include <algorithm>
#include <vector>

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

    try {
        if (!mEngine->InitializeEmbedded(width(), height(), true)) {
            emit engineInitFailed(QStringLiteral("Engine::InitializeEmbedded ist fehlgeschlagen (siehe engine.log)."));
            return;
        }
    } catch (const std::exception& e) {
        emit engineInitFailed(QStringLiteral("Engine-Initialisierung warf Exception: ")
                              + QString::fromUtf8(e.what()));
        return;
    } catch (...) {
        emit engineInitFailed(QStringLiteral("Engine-Initialisierung warf unbekannte Exception."));
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
    // Gizmo drag
    if (mGizmoDragging && !mEngine->IsPlaying()) {
        dragGizmo(static_cast<float>(p.x()), static_cast<float>(p.y()));
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
            // Gizmo-Achse greifen wenn Entity selektiert
            int axis = -1;
            if (mGizmoMode == 1 && mEngine->GetSelectedEntity() >= 0 &&
                tryPickGizmoAxis(static_cast<float>(p.x()), static_cast<float>(p.y()), axis)) {
                mGizmoDragging = true;
                mGizmoAxis = axis;
                mGizmoStartMouse = p;
                auto* tc = mEngine->GetScene().GetComponent<rpg::TransformComponent>(
                    static_cast<rpg::EntityID>(mEngine->GetSelectedEntity()));
                if (tc) {
                    mGizmoStartPos[0] = tc->transform.position.x;
                    mGizmoStartPos[1] = tc->transform.position.y;
                    mGizmoStartPos[2] = tc->transform.position.z;
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

    if (b == rpg::MouseButton::Left) {
        if (mPainting || mStrokeActive) flushPaintStroke();
        if (mGizmoDragging && mEngine && mEngine->GetSelectedEntity() >= 0) {
            auto* tc = mEngine->GetScene().GetComponent<rpg::TransformComponent>(
                static_cast<rpg::EntityID>(mEngine->GetSelectedEntity()));
            if (tc) {
                rpg::Vec3 oldP(mGizmoStartPos[0], mGizmoStartPos[1], mGizmoStartPos[2]);
                rpg::Vec3 newP = tc->transform.position;
                if (glm::length(newP - oldP) > 1e-4f) {
                    // Reset to old then Execute so history matches
                    tc->transform.position = oldP;
                    auto cmd = std::make_shared<rpg::MoveEntityCommand>(
                        static_cast<rpg::EntityID>(mEngine->GetSelectedEntity()), oldP, newP);
                    mEngine->GetCommandHistory().Execute(*mEngine, cmd);
                }
            }
        }
        mPainting = false;
        mStrokeActive = false;
        mGizmoDragging = false;
        mGizmoAxis = -1;
    }
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

// Stroke-Buffer fuer ein Undo pro Pinselstrich
static std::vector<rpg::BatchTileCommand::Change> s_stroke;
static int s_strokeLastX = -99999, s_strokeLastZ = -99999;

void QtGameViewWidget::flushPaintStroke() {
    if (!mEngine || s_stroke.empty()) { s_stroke.clear(); return; }
    auto cmd = std::make_shared<rpg::BatchTileCommand>(std::move(s_stroke), "Brush Stroke");
    s_stroke.clear();
    s_strokeLastX = s_strokeLastZ = -99999;
    mEngine->GetCommandHistory().Execute(*mEngine, cmd);
}

void QtGameViewWidget::paintTileAtScreen(float sx, float sy) {
    int x = 0, z = 0;
    if (!tryGroundHit(sx, sy, x, z)) return;
    auto& map = mEngine->GetMap();
    if (x < 0 || z < 0 || x >= map.GetWidth() || z >= map.GetHeight()) return;
    const int layer = mPaintLayer;
    if (layer < 0 || layer >= static_cast<int>(map.GetLayers().size())) return;
    if (x == s_strokeLastX && z == s_strokeLastZ) return;
    s_strokeLastX = x; s_strokeLastZ = z;
    const int oldTile = map.GetTile(layer, x, z);
    const int newTile = mPaintTile;
    if (oldTile == newTile) return;
    // sofort anwenden fuer Feedback, Undo ueber Batch am Stroke-Ende
    map.SetTile(layer, x, z, newTile);
    s_stroke.push_back({layer, x, z, oldTile, newTile});
    mStrokeActive = true;
    emit tilePainted(x, z, newTile);
}

bool QtGameViewWidget::tryPickGizmoAxis(float sx, float sy, int& outAxis) {
    outAxis = -1;
    const int sel = mEngine->GetSelectedEntity();
    if (sel < 0) return false;
    auto* tc = mEngine->GetScene().GetComponent<rpg::TransformComponent>(static_cast<rpg::EntityID>(sel));
    if (!tc) return false;
    rpg::Camera& cam = mEngine->GetRenderer().GetCamera();
    const rpg::Ray ray = rpg::Raycast::ScreenPointToRay(cam,
        rpg::Vec2(sx, sy), rpg::Vec2((float)width(), (float)height()));
    const rpg::Vec3 origin = tc->transform.position;
    // Einfache Achsen-Picking: naechster Punkt auf Achsen-Segment (Laenge ~1.5)
    float best = 0.25f; // max Distanz zur Achse
    int bestAxis = -1;
    const rpg::Vec3 axes[3] = {
        rpg::Vec3(1.5f, 0, 0), rpg::Vec3(0, 1.5f, 0), rpg::Vec3(0, 0, 1.5f)
    };
    for (int a = 0; a < 3; ++a) {
        // distance ray to segment origin->origin+axis
        const rpg::Vec3 d = axes[a];
        const rpg::Vec3 w0 = ray.origin - origin;
        const float a_ = glm::dot(ray.direction, ray.direction);
        const float b_ = glm::dot(ray.direction, d);
        const float c_ = glm::dot(d, d);
        const float d_ = glm::dot(ray.direction, w0);
        const float e_ = glm::dot(d, w0);
        const float denom = a_ * c_ - b_ * b_;
        float t = 0.f, s = 0.f;
        if (std::fabs(denom) > 1e-6f) {
            t = (b_ * e_ - c_ * d_) / denom;
            s = (a_ * e_ - b_ * d_) / denom;
        }
        s = std::max(0.f, std::min(1.f, s));
        t = std::max(0.f, t);
        const rpg::Vec3 pRay = ray.origin + ray.direction * t;
        const rpg::Vec3 pSeg = origin + d * s;
        const float dist = glm::length(pRay - pSeg);
        if (dist < best) { best = dist; bestAxis = a; }
    }
    if (bestAxis < 0) return false;
    outAxis = bestAxis;
    return true;
}

void QtGameViewWidget::dragGizmo(float sx, float sy) {
    const int sel = mEngine->GetSelectedEntity();
    if (sel < 0 || mGizmoAxis < 0) return;
    auto* tc = mEngine->GetScene().GetComponent<rpg::TransformComponent>(static_cast<rpg::EntityID>(sel));
    if (!tc) return;
    // Maus-Delta in Screen -> Welt entlang Achse (einfach skaliert)
    const float dx = (sx - static_cast<float>(mGizmoStartMouse.x())) * 0.02f;
    const float dy = (static_cast<float>(mGizmoStartMouse.y()) - sy) * 0.02f;
    rpg::Vec3 pos(mGizmoStartPos[0], mGizmoStartPos[1], mGizmoStartPos[2]);
    if (mGizmoAxis == 0) pos.x += dx;
    else if (mGizmoAxis == 1) pos.y += dy;
    else if (mGizmoAxis == 2) pos.z += dx;
    tc->transform.position = pos;
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
    std::vector<rpg::BatchTileCommand::Change> changes;
    for (int z = z0; z <= z1; ++z) {
        for (int x = x0; x <= x1; ++x) {
            const int oldTile = map.GetTile(layer, x, z);
            if (oldTile == mPaintTile) continue;
            changes.push_back({layer, x, z, oldTile, mPaintTile});
        }
    }
    if (changes.empty()) return;
    auto cmd = std::make_shared<rpg::BatchTileCommand>(std::move(changes), "Rect Paint");
    mEngine->GetCommandHistory().Execute(*mEngine, cmd);
    emit tilePainted(x0, z0, mPaintTile);
}

} // namespace qt_editor
