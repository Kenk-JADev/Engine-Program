#pragma once
// Qt-Editor: eingebettetes OpenGL-Widget - der "Game View" des Editors.
// Besitzt den GL-Kontext (Qt), laedt glad, initialisiert die Engine im
// Embedded-Modus und rendert jede paintGL()-Runde den Engine-Frame.

#include <QOpenGLWidget>
#include <QElapsedTimer>

namespace rpg { class Engine; }

namespace qt_editor {

// Interaktionsmodus des Game-Views: 3D-Selektion oder Tile-Malen/Radieren
// (radieren entspricht ImGui-Editor: mSelectedTile == -1 = Radiergummi)
enum class ViewMode { Select, Paint, Erase };

class QtGameViewWidget : public QOpenGLWidget {
    Q_OBJECT
public:
    explicit QtGameViewWidget(rpg::Engine* engine, QWidget* parent = nullptr);

    bool IsEngineReady() const { return mEngineReady; }
    void SetViewMode(ViewMode mode) { mViewMode = mode; }
    ViewMode GetViewMode() const { return mViewMode; }

signals:
    void engineInitFailed(QString message);
    void engineReady();
    // 3D-Klick-Selektion: Linksklick im View hat eine Entity getroffen
    // (id >= 0) bzw. ins Leere gegriffen (id == -1 -> Selektion aufheben).
    void entityPicked(int id);
    // Tile-Modus (Paint/Erase): Linksklick oder Drag traf den Boden (y=0)
    // an Weltposition (wx, wz). Umrechnung in Map-Kacheln macht das Fenster.
    void groundClicked(float wx, float wz);
    // Rechtsklick OHNE Ziehen auf den Boden (Drag bleibt Kamera-Orbit):
    // Kontextmenue an (globalX, globalY), Bodentreffer bei (wx, wz).
    void groundContextMenu(int globalX, int globalY, float wx, float wz);

protected:
    void initializeGL() override;
    void paintGL() override;
    void resizeGL(int w, int h) override;

    // Engine-Input-Bruecke (das Widget hat StrongFocus)
    void keyPressEvent(QKeyEvent* event) override;
    void keyReleaseEvent(QKeyEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void wheelEvent(QWheelEvent* event) override;

private:
    void emitGroundHit(const QPointF& pos); // Ray -> Bodenebene -> Signal

    rpg::Engine* mEngine = nullptr; // nicht owned (QtEditorWindow besitzt)
    bool mGladLoaded = false;
    bool mEngineReady = false;
    ViewMode mViewMode = ViewMode::Select;
    QPointF mRightPressPos{-1.0, -1.0}; // fuer Klick-vs-Drag-Erkennung (Rechts)
};

} // namespace qt_editor
