#pragma once
// Qt-Editor-Hauptfenster: QMainWindow mit eigenen, nativen Dock-Fenstern
// (das, was mit ImGui nicht moeglich ist), eingebetteter Game-View (Qt GL),
// Menues/Toolbars/Statuszeile und dem Qt-getriebenen Game-Loop (QTimer).

#include <QMainWindow>
#include <memory>

class QDockWidget;
class QListWidget;
class QLabel;
class QPlainTextEdit;
class QTimer;
class QElapsedTimer;

namespace rpg { class Engine; }

namespace qt_editor {

class QtGameViewWidget;

class QtEditorWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit QtEditorWindow(QWidget* parent = nullptr);
    ~QtEditorWindow() override;

protected:
    void closeEvent(QCloseEvent* event) override;

private slots:
    void onTick();               // Game-Loop (QTimer, ~60 Hz)
    void onPlaytestToggled(bool on);
    void onAboutToQuit();        // sauberes Engine-Shutdown mit GL-Kontext

private:
    void buildMenus();
    void buildDocks();
    void buildToolbar();
    void log(const QString& msg);

    std::unique_ptr<rpg::Engine> mEngine;
    QtGameViewWidget* mView = nullptr;

    QDockWidget* mDockProject = nullptr;
    QDockWidget* mDockProperties = nullptr;
    QDockWidget* mDockConsole = nullptr;
    QPlainTextEdit* mConsole = nullptr;
    QLabel* mStatusInfo = nullptr;

    QTimer* mTimer = nullptr;
    QElapsedTimer* mClock = nullptr;
    float mFpsAccum = 0.0f;
    int mFpsFrames = 0;
};

} // namespace qt_editor
