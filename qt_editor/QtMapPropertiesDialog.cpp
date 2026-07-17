#include "QtMapPropertiesDialog.h"

#include "rpgmaker3d/Engine.h"
#include "rpgmaker3d/Map.h"
#include "rpgmaker3d/Tileset.h"
#include "rpgmaker3d/Database.h"
#include "rpgmaker3d/Project.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QFileInfo>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QSpinBox>
#include <QToolButton>
#include <QVBoxLayout>

namespace qt_editor {

#ifndef QL
#define QL(x) QStringLiteral(x)
#endif

QtMapPropertiesDialog::QtMapPropertiesDialog(rpg::Engine* engine, int mapInfoIndex, QWidget* parent)
    : QDialog(parent), mEngine(engine), mIndex(mapInfoIndex) {
    setModal(true);
    setWindowTitle(QL("Karteneigenschaften"));

    auto& infos = rpg::Database::Get().MapInfos();
    if (mIndex < 0 || mIndex >= (int)infos.size()) { mIndex = -1; return; }
    const auto& info = infos[(size_t)mIndex];

    auto* root = new QVBoxLayout(this);

    // ---- Allgemein (wie XP) ----------------------------------------------
    auto* genBox = new QGroupBox(QL("Allgemein"), this);
    auto* gen = new QFormLayout(genBox);
    mName = new QLineEdit(QString::fromStdString(info.name), genBox);
    mTileset = new QComboBox(genBox);
    for (const auto& ts : rpg::Database::Get().Tilesets())
        mTileset->addItem(QL("%1: %2").arg(ts.id, 3, 10, QLatin1Char('0'))
                              .arg(QString::fromStdString(ts.name)), ts.id);
    {
        const int ti = mTileset->findData(info.tilesetId);
        if (ti >= 0) mTileset->setCurrentIndex(ti);
    }
    auto* sizeRow = new QWidget(genBox);
    auto* sizeLay = new QHBoxLayout(sizeRow);
    sizeLay->setContentsMargins(0, 0, 0, 0);
    mWidth = new QSpinBox(sizeRow);  mWidth->setRange(1, 999);  mWidth->setValue(info.width);
    mHeight = new QSpinBox(sizeRow); mHeight->setRange(1, 999); mHeight->setValue(info.height);
    sizeLay->addWidget(mWidth);
    sizeLay->addWidget(new QLabel(QL("×"), sizeRow));
    sizeLay->addWidget(mHeight);
    sizeLay->addStretch(1);
    mScrollType = new QComboBox(genBox);
    mScrollType->addItems({QL("Keine Schleife"), QL("Vertikal"), QL("Horizontal"), QL("Beide")});
    mScrollType->setCurrentIndex(info.scrollType >= 0 && info.scrollType <= 3 ? info.scrollType : 0);
    gen->addRow(QL("Name"), mName);
    gen->addRow(QL("Tileset"), mTileset);
    gen->addRow(QL("Größe (Breite × Höhe)"), sizeRow);
    gen->addRow(QL("Scroll-Typ"), mScrollType);
    root->addWidget(genBox);

    // ---- Encounter ---------------------------------------------------------
    auto* encBox = new QGroupBox(QL("Gegner-Begegnungen"), this);
    auto* enc = new QFormLayout(encBox);
    mEncounterStep = new QSpinBox(encBox);
    mEncounterStep->setRange(1, 999);
    mEncounterStep->setValue(info.encounterStep > 0 ? info.encounterStep : 30);
    QStringList troops;
    for (int t : info.encounterList) if (t > 0) troops << QString::number(t);
    mEncounters = new QLineEdit(troops.join(QL(", ")), encBox);
    mEncounters->setPlaceholderText(QL("Trupp-IDs, Komma-getrennt (z.B. 1, 2)"));
    enc->addRow(QL("Schritte (Ø)"), mEncounterStep);
    enc->addRow(QL("Trupps"), mEncounters);
    root->addWidget(encBox);

    // ---- Musik (BGM/BGS wie XP, mit Anhören) -------------------------------
    auto* musBox = new QGroupBox(QL("Musik"), this);
    auto* mus = new QFormLayout(musBox);
    auto* bgmRow = new QWidget(musBox);
    auto* bgmLay = new QHBoxLayout(bgmRow);
    bgmLay->setContentsMargins(0, 0, 0, 0);
    mBgm = new QLineEdit(QString::fromStdString(info.bgmName), bgmRow);
    mBgmAuto = new QCheckBox(QL("Auto"), bgmRow);
    mBgmAuto->setChecked(info.bgmAutoPlay);
    mBgmPlay = new QToolButton(bgmRow);
    mBgmPlay->setText(QL("▶"));
    mBgmPlay->setCheckable(true);
    mBgmPlay->setToolTip(QL("BGM anhören / stoppen"));
    bgmLay->addWidget(mBgm, 1);
    bgmLay->addWidget(mBgmAuto);
    bgmLay->addWidget(mBgmPlay);
    auto* bgsRow = new QWidget(musBox);
    auto* bgsLay = new QHBoxLayout(bgsRow);
    bgsLay->setContentsMargins(0, 0, 0, 0);
    mBgs = new QLineEdit(QString::fromStdString(info.bgsName), bgsRow);
    mBgsAuto = new QCheckBox(QL("Auto"), bgsRow);
    mBgsAuto->setChecked(info.bgsAutoPlay);
    mBgsPlay = new QToolButton(bgsRow);
    mBgsPlay->setText(QL("▶"));
    mBgsPlay->setCheckable(true);
    mBgsPlay->setToolTip(QL("BGS anhören / stoppen"));
    bgsLay->addWidget(mBgs, 1);
    bgsLay->addWidget(mBgsAuto);
    bgsLay->addWidget(mBgsPlay);
    mus->addRow(QL("BGM"), bgmRow);
    mus->addRow(QL("BGS"), bgsRow);
    root->addWidget(musBox);

    // ---- Optionen -----------------------------------------------------------
    mNoDash = new QCheckBox(QL("Rennen auf dieser Karte verboten"), this);
    mNoDash->setChecked(info.disableDashing);
    root->addWidget(mNoDash);

    // ---- Buttons ------------------------------------------------------------
    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    buttons->button(QDialogButtonBox::Ok)->setText(QL("OK"));
    buttons->button(QDialogButtonBox::Cancel)->setText(QL("Abbrechen"));
    connect(buttons, &QDialogButtonBox::accepted, this, &QtMapPropertiesDialog::onOk);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    root->addWidget(buttons);

    connect(mBgmPlay, &QToolButton::toggled, this, [this](bool) { onToggleBgmPreview(); });
    connect(mBgsPlay, &QToolButton::toggled, this, [this](bool) { onToggleBgsPreview(); });
}

QString QtMapPropertiesDialog::resolveAudio(const QString& sub, const QString& name) const {
    if (!mEngine || name.trimmed().isEmpty()) return QString();
    const QString pp = QString::fromStdString(mEngine->GetProject().GetProjectPath());
    const QString low = sub.toLower();
    const QStringList dirs = { pp + "/Audio/" + sub, pp + "/audio/" + low,
                               pp + "/assets/audio/" + low };
    static const char* exts[] = {".wav", ".mp3", ".ogg", ".flac"};
    for (const QString& d : dirs) {
        for (const char* e : exts) {
            const QString p = d + "/" + name + QString::fromLatin1(e);
            if (QFileInfo::exists(p)) return p;
        }
        const QString p = d + "/" + name; // Name könnte Extension enthalten
        if (QFileInfo::exists(p)) return p;
    }
    return QString();
}

void QtMapPropertiesDialog::onToggleBgmPreview() {
    if (!mEngine) return;
    stopPreview();
    if (!mBgmPlay->isChecked()) return;
    const QString p = resolveAudio(QL("BGM"), mBgm->text().trimmed());
    if (p.isEmpty()) {
        mBgmPlay->setChecked(false);
        QMessageBox::information(this, QL("Anhören"),
            QL("BGM „%1“ wurde in den Audio-Ordnern nicht gefunden.").arg(mBgm->text().trimmed()));
        return;
    }
    mBgsPlay->setChecked(false);
    mPreview = mEngine->GetAudio().PlayBGM(p.toStdString(), true, 1.0f, 1.0f);
}

void QtMapPropertiesDialog::onToggleBgsPreview() {
    if (!mEngine) return;
    stopPreview();
    if (!mBgsPlay->isChecked()) return;
    const QString p = resolveAudio(QL("BGS"), mBgs->text().trimmed());
    if (p.isEmpty()) {
        mBgsPlay->setChecked(false);
        QMessageBox::information(this, QL("Anhören"),
            QL("BGS „%1“ wurde in den Audio-Ordnern nicht gefunden.").arg(mBgs->text().trimmed()));
        return;
    }
    mBgmPlay->setChecked(false);
    mPreview = mEngine->GetAudio().PlayBGS(p.toStdString(), true, 1.0f, 1.0f);
}

void QtMapPropertiesDialog::stopPreview() {
    if (mPreview.valid() && mEngine) {
        mEngine->GetAudio().Stop(mPreview);
        mPreview = rpg::SoundHandle{};
    }
}

void QtMapPropertiesDialog::done(int r) {
    stopPreview();
    QDialog::done(r);
}

void QtMapPropertiesDialog::onOk() {
    auto& db = rpg::Database::Get();
    auto& infos = db.MapInfos();
    if (mIndex < 0 || mIndex >= (int)infos.size()) { reject(); return; }
    auto& info = infos[(size_t)mIndex];

    info.name = mName->text().trimmed().isEmpty()
        ? ("Karte " + std::to_string(info.id))
        : mName->text().trimmed().toStdString();
    info.tilesetId = mTileset->currentData().toInt();
    info.scrollType = mScrollType->currentIndex();
    info.encounterStep = mEncounterStep->value();
    info.bgmName = mBgm->text().trimmed().toStdString();
    info.bgmAutoPlay = mBgmAuto->isChecked();
    info.bgsName = mBgs->text().trimmed().toStdString();
    info.bgsAutoPlay = mBgsAuto->isChecked();
    info.disableDashing = mNoDash->isChecked();
    // Trupp-CSV -> encounterList[8]
    for (int& t : info.encounterList) t = 0;
    {
        const QStringList parts = mEncounters->text().split(QLatin1Char(','), Qt::SkipEmptyParts);
        int slot = 0;
        for (const QString& p : parts) {
            if (slot >= 8) break;
            bool okn = false;
            const int v = p.trimmed().toInt(&okn);
            if (okn && v > 0) info.encounterList[slot++] = v;
        }
    }

    // Engine-Karte nachziehen (diese Karte ist im Dock/Der Ansicht geladen)
    if (mEngine && mEngine->IsInitialized()) {
        mEngine->GetMap().Resize(mWidth->value(), mHeight->value()); // inhaltserhaltend
        info.width = mWidth->value();
        info.height = mHeight->value();
        // Tileset ggf. neu laden (wie Map-Dock)
        auto& tilesets = db.Tilesets();
        for (const auto& ts : tilesets) {
            if (ts.id == info.tilesetId) {
                auto tileset = std::make_shared<rpg::Tileset>();
                const std::string path = mEngine->GetProject().GetAssetPath("textures/" + ts.tilesetName);
                if (!tileset->Load(path, 32, 32))
                    tileset->Load("assets/textures/tileset_demo.png", 32, 32);
                mEngine->GetMap().SetTileset(tileset);
                break;
            }
        }
        // Dateien sichern: Karten-Datei + Datenbank
        const std::string pp = mEngine->GetProject().GetProjectPath();
        if (!pp.empty()) {
            mEngine->GetMap().Save(mEngine->GetProject().GetMapPath(info.id));
            db.Save(pp);
        }
    }
    accept();
}

bool QtMapPropertiesDialog::EditMapProperties(QWidget* parent, rpg::Engine* engine, int mapInfoIndex) {
    QtMapPropertiesDialog dlg(engine, mapInfoIndex, parent);
    if (dlg.mIndex < 0) return false;
    return dlg.exec() == QDialog::Accepted;
}

} // namespace qt_editor
