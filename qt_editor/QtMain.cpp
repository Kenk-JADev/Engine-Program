// RPG Maker 3D - Qt Editor entry point
// Ersetzt src/main.cpp, wenn RPGMAKER3D_EDITOR_QT=ON.

#include <QApplication>
#include <QSurfaceFormat>
#include <QCoreApplication>
#include <QGuiApplication>
#include <QByteArray>

#include "rpgmaker3d/Platform.h"
#include "rpgmaker3d/StartupError.h"
#include "QtEditorWindow.h"

// ============================================================================
// Windows DPI – exakte Ursache der Warnung:
//
//   SetProcessDpiAwarenessContext() failed: Zugriff verweigert
//
// Windows erlaubt pro Prozess GENAU EINEN erfolgreichen DPI-Set.
// Reihenfolge ohne Fix:
//   1) app.manifest / gdiScaling / alte qt.conf setzt Awareness
//   2) Qt 6 QWindowsIntegration ruft erneut SetProcessDpiAwarenessContext(V2)
//   3) -> ACCESS_DENIED + qt.qpa.window Log
//
// Fix:
//   1) Manifest ohne DPI/gdiScaling
//   2) Platform::SetDPIAware() EINMAL vor QApplication (PerMonitorV2)
//   3) Qt-Warnung qt.qpa.window unterdruecken (Qt versucht trotzdem den
//      Set-Call; das ist harmlos, wenn wir schon V2 sind – nur Log-Noise)
//   4) Kein dpiawareness=-1 (das ist "Invalid" und erzeugt andere Fehler)
// ============================================================================

int main(int argc, char** argv) {
#if defined(_WIN32)
    // 1) DPI einmal setzen, BEVOR Qt die QPA-Plugin-Init laeuft
    rpg::Platform::SetDPIAware();
#endif

    // GLOBAL: jede ungefangene Exception (z.B. in QtEditorWindow-Ctor oder
    // waehrend initializeGL aus dem Event-Loop) soll eine sichtbare Meldung
    // + engine.log-Eintrag erzeugen, statt lautlos (std::terminate) zu sterben.
    rpg::InstallStartupTerminateHandler();

#if defined(_WIN32)
    // 2) Qt soll die bekannte, harmlose Doppel-Set-Warnung nicht spammen.
    //    (Qt ruft intern trotzdem SetProcessDpiAwarenessContext auf.)
    {
        QByteArray rules = qgetenv("QT_LOGGING_RULES");
        const char* silence = "qt.qpa.window.warning=false";
        if (rules.isEmpty()) {
            qputenv("QT_LOGGING_RULES", silence);
        } else if (!rules.contains("qt.qpa.window")) {
            rules += ";";
            rules += silence;
            qputenv("QT_LOGGING_RULES", rules);
        }
    }
#endif

#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
    QCoreApplication::setAttribute(Qt::AA_EnableHighDpiScaling);
    QCoreApplication::setAttribute(Qt::AA_UseHighDpiPixmaps);
#endif
    QGuiApplication::setHighDpiScaleFactorRoundingPolicy(
        Qt::HighDpiScaleFactorRoundingPolicy::PassThrough);

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

    try {
        qt_editor::QtEditorWindow window;
        window.show();

        return QApplication::exec();
    } catch (const std::exception& e) {
        rpg::ReportStartupError("Qt-Editor-Start", e.what());
        return -3;
    } catch (...) {
        rpg::ReportStartupError("Qt-Editor-Start", "Unbekannter Fehler beim Start des Editors.");
        return -3;
    }
}
