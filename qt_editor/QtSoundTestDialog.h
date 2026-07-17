#pragma once
// XP-artiger Sound-Test-Dialog (Werkzeuge -> Sound-Test).
// Tabs BGM / BGS / ME / SE, Dateiliste mit "(Kein)", Wiedergabe/Stopp,
// Lautstärke-Regler (0-100 %) und Pitch-Regler (50-150 %) wie in RPG Maker XP.

#include <QDialog>
#include <QString>

#include "rpgmaker3d/AudioManager.h"

class QTabWidget;
class QListWidget;
class QSlider;
class QLabel;
class QPushButton;

namespace rpg { class Engine; }

namespace qt_editor {

class QtSoundTestDialog : public QDialog {
    Q_OBJECT
public:
    QtSoundTestDialog(rpg::Engine* engine, QWidget* parent = nullptr);

    /// Modal öffnen und abspielen lassen; beim Schließen wird gestoppt.
    static void ShowSoundTest(QWidget* parent, rpg::Engine* engine);

private slots:
    void onPlay();
    void onStop();
    void onVolumeChanged(int v);
    void onPitchChanged(int v);

private:
    enum Kind { BGM = 0, BGS = 1, ME = 2, SE = 3 };

    void refill(Kind kind);                 // Liste eines Tabs neu einlesen
    QString folderFor(Kind kind) const;     // erster existierender Ordner
    QString selectedPath() const;           // "" = Eintrag "(Kein)"
    void stopCurrent();

    void done(int r) override;              // Playback beim Schließen stoppen

    rpg::Engine* mEngine = nullptr;
    QString mProjectPath;

    QTabWidget* mTabs = nullptr;
    QListWidget* mLists[4] = {};
    QPushButton* mPlayBtn = nullptr;
    QPushButton* mStopBtn = nullptr;
    QSlider* mVolume = nullptr;
    QSlider* mPitch = nullptr;
    QLabel* mVolumeLabel = nullptr;
    QLabel* mPitchLabel = nullptr;

    rpg::SoundHandle mHandle;               // aktuell laufende Wiedergabe
};

} // namespace qt_editor
