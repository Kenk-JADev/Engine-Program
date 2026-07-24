#pragma once
// Qt-Editor: eingebettetes OpenGL-Widget - der "Game View" des Editors.
// Besitzt den GL-Kontext (Qt), laedt glad, initialisiert die Engine im
// Embedded-Modus und rendert jede paintGL()-Runde den Engine-Frame.
// Zusaetzlich: Tile-Paint-Modus + Engine-Input-Bruecke (RmlUi entfallen, PAKET 10).

// glad VOR Qt OpenGL, sonst "OpenGL header already included"
#ifndef GLAD_GL_H_
#include <glad/gl.h>
#endif
#include <QOpenGLWidget>
#include <QElapsedTimer>
#include <QPointF>

namespace rpg { class Engine; }

namespace qt_editor {

class QtGameViewWidget : public QOpenGLWidget {
    Q_OBJECT
public:
    explicit QtGameViewWidget(rpg::Engine* engine, QWidget* parent = nullptr);

    bool IsEngineReady() const { return mEngineReady; }

    // Tile-Paint (vom Map-Editor-Dock gesetzt)
    void setPaintMode(bool enabled) { mPaintMode = enabled; }
    void setPaintTile(int tileId) { mPaintTile = tileId; } // -1 = eraser
    void setPaintLayer(int layer) { mPaintLayer = layer; }
    void setBrushMode(int mode) { mBrushMode = mode; mRectHasFirst = false; }
    bool paintMode() const { return mPaintMode; }

    // Gizmo: 0=off/select, 1=translate (default when not painting)
    void setGizmoMode(int mode) { mGizmoMode = mode; }
    int gizmoMode() const { return mGizmoMode; }

signals:
    void engineInitFailed(QString message);
    void engineReady();
    // 3D-Klick-Selektion: Linksklick im View hat eine Entity getroffen
    // (id >= 0) bzw. ins Leere gegriffen (id == -1 -> Selektion aufheben).
    void entityPicked(int id);
    void tilePainted(int x, int z, int tileId);

protected:
    void initializeGL() override;
    void paintGL() override;
    void resizeGL(int w, int h) override;

    void keyPressEvent(QKeyEvent* event) override;
    void keyReleaseEvent(QKeyEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void wheelEvent(QWheelEvent* event) override;
    void inputMethodEvent(QInputMethodEvent* event) override;
    QVariant inputMethodQuery(Qt::InputMethodQuery query) const override;

private:
    void paintTileAtScreen(float sx, float sy);
    bool tryGroundHit(float sx, float sy, int& outX, int& outZ);

    rpg::Engine* mEngine = nullptr;
    bool mGladLoaded = false;
    bool mEngineReady = false;

    bool mPaintMode = false;
    int mPaintTile = 0;
    int mPaintLayer = 0;
    bool mPainting = false;
    bool mStrokeActive = false;
    int mBrushMode = 0; // 0 paint, 1 rect
    bool mRectHasFirst = false;
    int mRectX0 = 0, mRectZ0 = 0;
    void fillRect(int x0, int z0, int x1, int z1);
    void flushPaintStroke();

    // Gizmo translate
    // PAKET 26: Standard = AUS (0). Sonst liessen sich Entities in der
    // Spielansicht sofort versehentlich per Maus verschieben. Aktivierung
    // nur explizit ueber das Ribbon "Gizmo" (Editor-Tab Werkzeuge).
    int mGizmoMode = 0; // 0=aus, 1=translate
    int mGizmoAxis = -1; // 0=X 1=Y 2=Z
    bool mGizmoDragging = false;
    float mGizmoStartPos[3] = {0,0,0};
    QPointF mGizmoStartMouse{};
    bool tryPickGizmoAxis(float sx, float sy, int& outAxis);
    void dragGizmo(float sx, float sy);
};

} // namespace qt_editor
