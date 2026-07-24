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
// Editor-Theme "XP Classic" (PAKET 35): klassische Redaktions-Optik wie
// eine XP-aehnliche MAKER-Oberflaeche — helle Werkzeugflaechen (Himmel-Grau
// mit Blau-Akzenten), klare 1px-Rahmen, markierte Werkzeug-Buttons.
// Eigenes Farbset (kein Nachbau einer fremden Skin-Datei).
// ============================================================================
static void ApplyXPEditorTheme(QApplication& app) {
    QApplication::setStyle(QStyleFactory::create(QStringLiteral("Fusion")));

    QPalette p;
    const QColor window(222, 229, 241);   // Werkzeuggrau-Blau (XP-aehnlich)
    const QColor base(245, 248, 252);     // Eingabeflaechen
    const QColor alt(233, 239, 248);      // Alternierende Zeilen
    const QColor text(20, 26, 34);
    const QColor disabled(130, 138, 148);
    const QColor accent(62, 110, 175);    // XP-Blau
    const QColor accentHi(96, 148, 212);
    const QColor button(214, 222, 236);

    p.setColor(QPalette::Window, window);
    p.setColor(QPalette::WindowText, text);
    p.setColor(QPalette::Base, base);
    p.setColor(QPalette::AlternateBase, alt);
    p.setColor(QPalette::ToolTipBase, alt);
    p.setColor(QPalette::ToolTipText, text);
    p.setColor(QPalette::Text, text);
    p.setColor(QPalette::Button, button);
    p.setColor(QPalette::ButtonText, text);
    p.setColor(QPalette::BrightText, QColor(190, 40, 40));
    p.setColor(QPalette::Link, accent);
    p.setColor(QPalette::Highlight, accent);
    p.setColor(QPalette::HighlightedText, Qt::white);
    p.setColor(QPalette::PlaceholderText, disabled);
    p.setColor(QPalette::Disabled, QPalette::Text, disabled);
    p.setColor(QPalette::Disabled, QPalette::ButtonText, disabled);
    p.setColor(QPalette::Disabled, QPalette::WindowText, disabled);
    app.setPalette(p);

    app.setStyleSheet(QStringLiteral(R"(
        QToolTip {
            color: #1a2230; background-color: #f2f6fc;
            border: 1px solid #9db3cf; padding: 3px;
        }
        QMenuBar { background-color: #dde5f1; border-bottom: 1px solid #b7c5d8; }
        QMenuBar::item { padding: 4px 10px; background: transparent; }
        QMenuBar::item:selected { background-color: #b9cde8; border-radius: 2px; }
        QMenuBar::item:pressed { background-color: #3e6eaf; color: #ffffff; }
        QMenu {
            background-color: #f4f7fb; border: 1px solid #a9bdd6;
            selection-color: #ffffff;
        }
        QMenu::item { padding: 5px 26px 5px 26px; }
        QMenu::item:selected { background-color: #3e6eaf; color: #ffffff; }
        QMenu::separator { height: 1px; background: #b7c5d8; margin: 3px 6px; }
        QTabWidget::pane { border: 1px solid #a9bdd6; background: #eef3fa; top: -1px; }
        QTabBar::tab {
            background: #cfdaea; color: #22324a;
            padding: 6px 16px; border: 1px solid #a9bdd6;
            border-bottom: none; border-top-left-radius: 3px;
            border-top-right-radius: 3px; margin-right: 2px;
        }
        QTabBar::tab:selected { background: #ffffff; color: #10305a; border-bottom: 1px solid #ffffff; }
        QTabBar::tab:hover:!selected { background: #e3ecf7; }
        QTabBar::tab:!selected { margin-bottom: 2px; }
        QTabWidget#centralTabs QTabBar::tab { min-width: 110px; font-weight: bold; }
        QDockWidget { color: #1a2230; }
        QDockWidget::title {
            background: qlineargradient(x1:0, y1:0, x2:0, y2:1,
                                        stop:0 #eaf0f9, stop:1 #ccd9ea);
            padding: 5px 8px; border-bottom: 1px solid #a9bdd6; font-weight: bold;
        }
        QGroupBox {
            border: 1px solid #b7c5d8; border-radius: 3px; margin-top: 10px;
            padding-top: 8px; font-weight: bold; color: #22324a;
        }
        QGroupBox::title { subcontrol-origin: margin; left: 8px; padding: 0 4px; }
        QPushButton, QToolButton {
            background-color: #e9eef7; border: 1px solid #a9bdd6;
            border-radius: 3px; padding: 5px 12px; color: #1a2230;
        }
        QPushButton:hover, QToolButton:hover { background-color: #d9e5f5; border: 1px solid #7d9dc7; }
        QPushButton:pressed, QToolButton:pressed { background-color: #3e6eaf; color: #ffffff; }
        QPushButton:checked, QToolButton:checked {
            background-color: #3e6eaf; color: #ffffff; border: 1px solid #2c5286;
        }
        QPushButton:disabled, QToolButton:disabled {
            background-color: #e3e6ec; color: #9aa3ae;
        }
        QPushButton#playTabPrimary {
            background-color: #2f7d43; color: #ffffff; font-size: 15px;
            font-weight: bold; border-radius: 4px; border: 1px solid #236133;
        }
        QPushButton#playTabPrimary:hover { background-color: #3a9953; }
        QLabel#playTabTitle { font-size: 20px; font-weight: bold; }
        /* XP-Symbolleiste: ruhige Icon-Zeile mit Check-Markierung */
        QToolBar#mainToolBar {
            background: qlineargradient(x1:0, y1:0, x2:0, y2:1,
                                        stop:0 #eef3fa, stop:1 #d8e2f0);
            border-bottom: 1px solid #a9bdd6; spacing: 3px; padding: 2px 6px;
        }
        QToolBar#mainToolBar QToolButton { padding: 3px; margin: 1px; border-radius: 3px; }
        QToolBar#mainToolBar QToolButton:hover { background: #dbe6f4; }
        QToolBar#mainToolBar QToolButton:checked {
            background: #3e6eaf; color: #ffffff; border: 1px solid #5a8ec5;
        }
        QStatusBar { background: #dde5f1; border-top: 1px solid #b7c5d8; color: #22324a; }
        QSplitter::handle { background: #cdd8e8; }
        QSplitter::handle:hover { background: #b7c9e0; }
        QLineEdit, QSpinBox, QDoubleSpinBox, QComboBox, QPlainTextEdit,
        QListWidget, QTreeWidget {
            background-color: #ffffff; border: 1px solid #aab9cf;
            border-radius: 2px; padding: 3px; color: #1a2230;
            selection-background-color: #3e6eaf; selection-color: #ffffff;
        }
        QLineEdit:focus, QSpinBox:focus, QComboBox:focus, QPlainTextEdit:focus {
            border: 1px solid #3e6eaf;
        }
        QComboBox QAbstractItemView {
            background-color: #ffffff; border: 1px solid #a9bdd6;
            selection-background-color: #3e6eaf; selection-color: #ffffff;
        }
        QTreeWidget::item:selected, QListWidget::item:selected {
            background-color: #3e6eaf; color: #ffffff;
        }
        QScrollBar:vertical { background: #e9edf4; width: 14px; margin: 0px; }
        QScrollBar::handle:vertical {
            background: #b9c7db; min-height: 26px; border-radius: 5px; margin: 2px;
        }
        QScrollBar::handle:vertical:hover { background: #93abcb; }
        QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height: 0px; }
        QScrollBar:horizontal { background: #e9edf4; height: 14px; margin: 0px; }
        QScrollBar::handle:horizontal {
            background: #b9c7db; min-width: 26px; border-radius: 5px; margin: 2px;
        }
        QScrollBar::handle:horizontal:hover { background: #93abcb; }
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
    ApplyXPEditorTheme(app);

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
