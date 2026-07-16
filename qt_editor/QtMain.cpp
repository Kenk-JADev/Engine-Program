// RPG Maker 3D - Qt Editor entry point
// Ersetzt src/main.cpp, wenn RPGMAKER3D_EDITOR_QT=ON.

#include <QApplication>
#include <QSurfaceFormat>
#include <QCoreApplication>
#include <QGuiApplication>
#include <QByteArray>

#include "QtEditorWindow.h"

// Windows DPI:
// - app.manifest setzt bereits PerMonitorV2 beim Prozessstart.
// - Qt 6 ruft standardmaessig SetProcessDpiAwarenessContext(V2) auf.
// - Der zweite Aufruf schlaegt mit "Zugriff verweigert" fehl und loggt
//   qt.qpa.window-Warnungen. dpiawareness=-1 = Qt setzt DPI NICHT selbst.
// Siehe: https://doc.qt.io/qt-6/highdpi.html#configuring-windows

int main(int argc, char** argv) {
#if defined(_WIN32)
    // Muss VOR QGuiApplication/QApplication gesetzt werden.
    // Erhaelt bestehendes Platform-Argument, haengt nur dpiawareness an.
    {
        QByteArray plat = qgetenv("QT_QPA_PLATFORM");
        if (plat.isEmpty()) {
            qputenv("QT_QPA_PLATFORM", "windows:dpiawareness=-1");
        } else if (!plat.contains("dpiawareness")) {
            if (plat.startsWith("windows")) {
                qputenv("QT_QPA_PLATFORM", plat + ":dpiawareness=-1");
            }
            // sonst: User hat anderes Plugin gesetzt – nicht anfassen
        }
    }
#endif

#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
    QCoreApplication::setAttribute(Qt::AA_EnableHighDpiScaling);
    QCoreApplication::setAttribute(Qt::AA_UseHighDpiPixmaps);
#endif
    QGuiApplication::setHighDpiScaleFactorRoundingPolicy(
        Qt::HighDpiScaleFactorRoundingPolicy::PassThrough);

    // GL 3.3 Core
    QSurfaceFormat fmt;
    fmt.setRenderableType(QSurfaceFormat::OpenGL);
    fmt.setVersion(3, 3);
    fmt.setProfile(QSurfaceFormat::CoreProfile);
    fmt.setDepthBufferSize(24);
    fmt.setStencilBufferSize(8);
    fmt.setSwapInterval(1);
    QSurfaceFormat::setDefaultFormat(fmt);

    QApplication app(argc, argv);
    QApplication::setApplicationName("RPGMaker3D-Editor-Qt");
    QApplication::setOrganizationName("RPGMaker3D");

    qt_editor::QtEditorWindow window;
    window.show();

    return QApplication::exec();
}
