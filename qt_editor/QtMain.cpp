// RPG Maker 3D - Qt Editor entry point
// Ersetzt src/main.cpp, wenn RPGMAKER3D_EDITOR_QT=ON:
// SDL-Fenster + ImGui entfallen; der Editor ist eine native Qt-App
// (QMainWindow + Dock-Fenster, Game-View als QOpenGLWidget).

#include <QApplication>
#include <QSurfaceFormat>
#include <QCoreApplication>
#include <QGuiApplication>

#include "QtEditorWindow.h"

// Windows: Qt 6 setzt DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2 selbst.
// Manifest (app.manifest) deklariert dasselbe. Kein manuelles
// SetProcessDpiAwarenessContext vor QApplication – sonst "Zugriff verweigert".

int main(int argc, char** argv) {
    // High-DPI: Qt 6 default ist bereits passend; Attribute nur absichern.
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
    QCoreApplication::setAttribute(Qt::AA_EnableHighDpiScaling);
    QCoreApplication::setAttribute(Qt::AA_UseHighDpiPixmaps);
#endif
    // Rounding-Policy: schaerfere UI auf gemischten Monitoren
    QGuiApplication::setHighDpiScaleFactorRoundingPolicy(
        Qt::HighDpiScaleFactorRoundingPolicy::PassThrough);

    // GL 3.3 Core (wie SDL-Pfad: EngineConfig::OPENGL_MAJOR/MINOR)
    QSurfaceFormat fmt;
    fmt.setRenderableType(QSurfaceFormat::OpenGL);
    fmt.setVersion(3, 3);
    fmt.setProfile(QSurfaceFormat::CoreProfile);
    fmt.setDepthBufferSize(24);
    fmt.setStencilBufferSize(8);
    fmt.setSwapInterval(1); // VSync
    QSurfaceFormat::setDefaultFormat(fmt);

    QApplication app(argc, argv);
    QApplication::setApplicationName("RPGMaker3D-Editor-Qt");
    QApplication::setOrganizationName("RPGMaker3D");

    qt_editor::QtEditorWindow window;
    window.show();

    return QApplication::exec();
}
