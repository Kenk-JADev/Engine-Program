#pragma once
// Qt-Editor: eingebettetes OpenGL-Widget - der "Game View" des Editors.
// Besitzt den GL-Kontext (Qt), laedt glad, initialisiert die Engine im
// Embedded-Modus und rendert jede paintGL()-Runde den Engine-Frame.
// Zusaetzlich: Tile-Paint-Modus + RmlUi-Input-Bruecke.

#include <QOpenGLWidget>
#include <QElapsedTimer>

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
    bool paintMode() const { return mPaintMode; }

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
    bool mPainting = false; // LMB gehalten im Paint-Modus
};

} // namespace qt_editor
