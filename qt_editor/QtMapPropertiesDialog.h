#pragma once
// XP-artiger "Karteneigenschaften"-Dialog (Name, Tileset, Größe, Scroll-Typ,
// Encounter, BGM/BGS mit Anhören, Rennen-Verbot). Arbeitet direkt auf einem
// MapInfo-Eintrag; bei Übernehmen werden Engine-Karte (Resize/Tileset) und
// Dateien (map-Datei + Datenbank) aktualisiert.

#include <QDialog>

#include "rpgmaker3d/AudioManager.h"

class QLineEdit;
class QComboBox;
class QSpinBox;
class QCheckBox;
class QToolButton;

namespace rpg { class Engine; }

namespace qt_editor {

class QtMapPropertiesDialog : public QDialog {
    Q_OBJECT
public:
    QtMapPropertiesDialog(rpg::Engine* engine, int mapInfoIndex, QWidget* parent = nullptr);

    /// Modal bearbeiten; true = übernommen (MapInfo/Engine/Dateien aktualisiert).
    /// mapInfoIndex = Index in Database::Get().MapInfos().
    static bool EditMapProperties(QWidget* parent, rpg::Engine* engine, int mapInfoIndex);

private slots:
    void onOk();
    void onToggleBgmPreview();
    void onToggleBgsPreview();

private:
    QString resolveAudio(const QString& sub, const QString& name) const;
    void stopPreview();
    void done(int r) override;

    rpg::Engine* mEngine = nullptr;
    int mIndex = -1;

    QLineEdit* mName = nullptr;
    QComboBox* mTileset = nullptr;
    QSpinBox* mWidth = nullptr;
    QSpinBox* mHeight = nullptr;
    QComboBox* mScrollType = nullptr;
    QSpinBox* mEncounterStep = nullptr;
    QLineEdit* mEncounters = nullptr;
    QLineEdit* mBgm = nullptr;
    QCheckBox* mBgmAuto = nullptr;
    QToolButton* mBgmPlay = nullptr;
    QLineEdit* mBgs = nullptr;
    QCheckBox* mBgsAuto = nullptr;
    QToolButton* mBgsPlay = nullptr;
    QCheckBox* mNoDash = nullptr;

    rpg::SoundHandle mPreview;
};

} // namespace qt_editor
