#pragma once
// Qt-Editor: eingebettetes OpenGL-Widget - der "Game View" des Editors.
// Besitzt den GL-Kontext (Qt), laedt glad, initialisiert die Engine im
// Embedded-Modus und rendert jede paintGL()-Runde den Engine-Frame.

#include <QOpenGLWidget>
#include <QElapsedTimer>

namespace rpg { class Engine; }

namespace qt_editor {

class QtGameViewWidget : public QOpenGLWidget {
    Q_OBJECT
public:
    explicit QtGameViewWidget(rpg::Engine* engine, QWidget* parent = nullptr);

    bool IsEngineReady() const { return mEngineReady; }

signals:
    void engineInitFailed(QString message);
    void engineReady();
    // 3D-Klick-Selektion: Linksklick im View hat eine Entity getroffen
    // (id >= 0) bzw. ins Leere gegriffen (id == -1 -> Selektion aufheben).
    void entityPicked(int id);

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
    rpg::Engine* mEngine = nullptr; // nicht owned (QtEditorWindow besitzt)
    bool mGladLoaded = false;
    bool mEngineReady = false;
};

} // namespace qt_editor
