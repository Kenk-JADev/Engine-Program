// RPG Maker 3D - Qt Editor entry point
// Ersetzt src/main.cpp, wenn RPGMAKER3D_EDITOR_QT=ON.

#include <QApplication>
#include <QSurfaceFormat>
#include <QCoreApplication>
#include <QGuiApplication>
#include <QByteArray>
#include <QColor>
#include <QPalette>
#include <QStyleFactory>

#include "rpgmaker3d/Platform.h"
#include "rpgmaker3d/StartupError.h"
#include "QtEditorWindow.h"

// ============================================================================
// Dunkles Engine-Theme ("Fusion Dark"), angelehnt an Unreal/Unity.
// ============================================================================
static void ApplyDarkEngineTheme(QApplication& app) {
    QApplication::setStyle(QStyleFactory::create(QStringLiteral("Fusion")));

    QPalette p;
    const QColor window(37, 39, 43);
    const QColor base(28, 30, 34);
    const QColor alt(44, 47, 52);
    const QColor text(230, 230, 230);
    const QColor disabled(120, 120, 120);
    const QColor accent(64, 145, 235);
    const QColor button(48, 51, 56);

    p.setColor(QPalette::Window, window);
    p.setColor(QPalette::WindowText, text);
    p.setColor(QPalette::Base, base);
    p.setColor(QPalette::AlternateBase, alt);
    p.setColor(QPalette::ToolTipBase, alt);
    p.setColor(QPalette::ToolTipText, text);
    p.setColor(QPalette::Text, text);
    p.setColor(QPalette::Button, button);
    p.setColor(QPalette::ButtonText, text);
    p.setColor(QPalette::BrightText, Qt::red);
    p.setColor(QPalette::Link, accent);
    p.setColor(QPalette::Highlight, accent);
    p.setColor(QPalette::HighlightedText, Qt::black);
    p.setColor(QPalette::PlaceholderText, disabled);
    p.setColor(QPalette::Disabled, QPalette::Text, disabled);
    p.setColor(QPalette::Disabled, QPalette::ButtonText, disabled);
    p.setColor(QPalette::Disabled, QPalette::WindowText, disabled);
    app.setPalette(p);

    app.setStyleSheet(QStringLiteral(R"(
        QToolTip { color: #e6e6e6; background-color: #2c2f34; border: 1px solid #555; }
        QMenuBar { background-color: #2b2d31; }
        QMenuBar::item:selected { background-color: #3d6ea5; }
        QMenu { background-color: #2b2d31; border: 1px solid #444; }
        QMenu::item:selected { background-color: #3d6ea5; }
        QTabWidget::pane { border: 1px solid #444; top: -1px; }
        QTabBar::tab {
            background: #2b2d31; color: #cfcfcf;
            padding: 7px 18px; border: 1px solid #444;
            border-top-left-radius: 4px; border-top-right-radius: 4px;
        }
        QTabBar::tab:selected { background: #3d6ea5; color: #ffffff; }
        QTabBar::tab:!selected { margin-bottom: 2px; }
        /* Tabs unten (zentrale Ansicht) – Browser-Stil */
        QTabWidget#centralTabs QTabBar::tab { min-width: 110px; font-weight: bold; }
        QDockWidget { color: #e6e6e6; titlebar-close-icon: none; }
        QDockWidget::title {
            background: #2b2d31; padding: 6px 8px;
            border-bottom: 1px solid #444; font-weight: bold;
        }
        QGroupBox {
            border: 1px solid #4a4d52; border-radius: 4px; margin-top: 10px;
            padding-top: 8px; font-weight: bold;
        }
        QGroupBox::title { subcontrol-origin: margin; left: 8px; padding: 0 4px; }
        QPushButton, QToolButton {
            background-color: #3a3d42; border: 1px solid #555;
            border-radius: 4px; padding: 5px 12px;
        }
        QPushButton:hover, QToolButton:hover { background-color: #4a5261; }
        QPushButton:pressed, QToolButton:pressed { background-color: #2f4a6d; }
        QPushButton:checked, QToolButton:checked { background-color: #3d6ea5; color: #fff; }
        QPushButton#playTabPrimary {
            background-color: #2f7d43; color: #ffffff; font-size: 15px;
            font-weight: bold; border-radius: 6px;
        }
        QPushButton#playTabPrimary:hover { background-color: #3a9953; }
        QLabel#playTabTitle { font-size: 20px; font-weight: bold; }
        /* XP-Symbolleiste (PAKET 28: eine ruhige Icon-Zeile statt Ribbon) */
        QToolBar#mainToolBar {
            background: #26282c; border-bottom: 1px solid #444;
            spacing: 4px; padding: 3px 6px;
        }
        QToolBar#mainToolBar QToolButton { padding: 3px; margin: 1px; border-radius: 3px; }
        QToolBar#mainToolBar QToolButton:hover { background: #3a3d42; }
        QToolBar#mainToolBar QToolButton:checked {
            background: #3d6ea5; border: 1px solid #5a8ec5;
        }
        QStatusBar { background: #26282c; border-top: 1px solid #444; }
        QSplitter::handle { background: #2b2d31; }
        QLineEdit, QSpinBox, QDoubleSpinBox, QComboBox, QPlainTextEdit, QListWidget, QTreeWidget {
            background-color: #1e2023; border: 1px solid #4a4d52;
            border-radius: 3px; padding: 3px; selection-background-color: #3d6ea5;
        }
        QComboBox QAbstractItemView { background-color: #2b2d31; border: 1px solid #555; }
        QScrollBar:vertical { background: #26282c; width: 12px; }
        QScrollBar::handle:vertical { background: #4a4d52; min-height: 24px; border-radius: 6px; }
        QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height: 0px; }
        QScrollBar:horizontal { background: #26282c; height: 12px; }
        QScrollBar::handle:horizontal { background: #4a4d52; min-width: 24px; border-radius: 6px; }
        QScrollBar::add-line:horizontal, QScrollBar::sub-line:horizontal { width: 0px; }
    )"));
}

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
    ApplyDarkEngineTheme(app);

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
