#include "QtSoundTestDialog.h"

#include "rpgmaker3d/Engine.h"
#include "rpgmaker3d/Project.h"

#include <QDir>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QLabel>
#include <QListWidget>
#include <QMessageBox>
#include <QPushButton>
#include <QSlider>
#include <QTabWidget>
#include <QVBoxLayout>

namespace qt_editor {

#ifndef QL
#define QL(x) QStringLiteral(x)
#endif

namespace {

// Von miniaudio dekodierbare Formate (MIDI wird nicht unterstützt)
const QStringList& audioFilters() {
    static const QStringList f = {QL("*.wav"), QL("*.mp3"), QL("*.ogg"), QL("*.flac")};
    return f;
}

// Kleine Skalen-Zeile unter einem Slider (linker / mittlerer / rechter Wert)
QWidget* makeScaleRow(const QString& left, const QString& mid, const QString& right,
                      QWidget* parent) {
    auto* w = new QWidget(parent);
    auto* lay = new QHBoxLayout(w);
    lay->setContentsMargins(0, 0, 0, 0);
    auto* l = new QLabel(left, w);
    auto* m = new QLabel(mid, w);
    auto* r = new QLabel(right, w);
    m->setAlignment(Qt::AlignHCenter);
    r->setAlignment(Qt::AlignRight);
    lay->addWidget(l);
    lay->addWidget(m, 1);
    lay->addWidget(r);
    return w;
}

} // namespace

QtSoundTestDialog::QtSoundTestDialog(rpg::Engine* engine, QWidget* parent)
    : QDialog(parent), mEngine(engine) {
    setModal(true);
    setWindowTitle(QL("Sound-Test"));
    resize(600, 430);
    if (mEngine)
        mProjectPath = QString::fromStdString(mEngine->GetProject().GetProjectPath());

    auto* root = new QHBoxLayout(this);

    // ---- links: Tabs (BGM/BGS/ME/SE) mit Dateiliste -----------------------
    auto* left = new QVBoxLayout();
    mTabs = new QTabWidget(this);
    static const char* tabNames[4] = {"BGM", "BGS", "ME", "SE"};
    for (int i = 0; i < 4; ++i) {
        auto* page = new QWidget(mTabs);
        auto* pl = new QVBoxLayout(page);
        pl->setContentsMargins(4, 4, 4, 4);
        mLists[i] = new QListWidget(page);
        pl->addWidget(mLists[i]);
        mTabs->addTab(page, QString::fromLatin1(tabNames[i])); // QStringLiteral braucht Literale!
        // Doppelklick = sofort abspielen (XP-Verhalten)
        connect(mLists[i], &QListWidget::itemDoubleClicked,
                this, [this](QListWidgetItem*) { onPlay(); });
    }
    left->addWidget(mTabs, 1);
    root->addLayout(left, 1);

    // ---- rechts: Wiedergabe/Stopp + Regler (wie XP) -----------------------
    auto* right = new QVBoxLayout();
    mPlayBtn = new QPushButton(QL("Wiedergeben"), this);
    mStopBtn = new QPushButton(QL("Stopp"), this);
    right->addWidget(mPlayBtn);
    right->addWidget(mStopBtn);
    right->addSpacing(14);

    mVolumeLabel = new QLabel(this);
    right->addWidget(mVolumeLabel);
    mVolume = new QSlider(Qt::Horizontal, this);
    mVolume->setRange(0, 100);
    mVolume->setValue(100);
    mVolume->setTickPosition(QSlider::TicksBelow);
    mVolume->setTickInterval(10);
    right->addWidget(mVolume);
    right->addWidget(makeScaleRow(QL("0 %"), QL("50 %"), QL("100 %"), this));
    right->addSpacing(14);

    mPitchLabel = new QLabel(this);
    right->addWidget(mPitchLabel);
    mPitch = new QSlider(Qt::Horizontal, this);
    mPitch->setRange(50, 150);
    mPitch->setValue(100);
    mPitch->setTickPosition(QSlider::TicksBelow);
    mPitch->setTickInterval(10);
    right->addWidget(mPitch);
    right->addWidget(makeScaleRow(QL("50 %"), QL("100 %"), QL("150 %"), this));
    right->addStretch(1);

    auto* closeBtn = new QPushButton(QL("Schließen"), this);
    closeBtn->setDefault(true);
    right->addWidget(closeBtn, 0, Qt::AlignRight);
    root->addLayout(right);

    connect(mPlayBtn, &QPushButton::clicked, this, &QtSoundTestDialog::onPlay);
    connect(mStopBtn, &QPushButton::clicked, this, &QtSoundTestDialog::onStop);
    connect(closeBtn, &QPushButton::clicked, this, &QDialog::accept);
    connect(mVolume, &QSlider::valueChanged, this, &QtSoundTestDialog::onVolumeChanged);
    connect(mPitch, &QSlider::valueChanged, this, &QtSoundTestDialog::onPitchChanged);
    // Beim Tabwechsel die angezeigte Liste sicher aktuell halten
    connect(mTabs, &QTabWidget::currentChanged, this, [this](int idx) {
        if (idx >= 0 && idx < 4) refill((Kind)idx);
    });

    for (int i = 0; i < 4; ++i) refill((Kind)i);
    if (mLists[0]->count() > 0) mLists[0]->setCurrentRow(0);
    onVolumeChanged(mVolume->value());
    onPitchChanged(mPitch->value());
}

void QtSoundTestDialog::ShowSoundTest(QWidget* parent, rpg::Engine* engine) {
    QtSoundTestDialog dlg(engine, parent);
    dlg.exec();
}

// ---------------------------------------------------------------------------

QString QtSoundTestDialog::folderFor(Kind kind) const {
    static const char* sub[4] = {"BGM", "BGS", "ME", "SE"};
    const QString s = QString::fromLatin1(sub[(int)kind]);
    const QString slow = s.toLower();
    QStringList candidates;
    // XP-Konvention zuerst, dann engine-eigene Ordner
    candidates << mProjectPath + QL("/Audio/") + s
               << mProjectPath + QL("/audio/") + slow
               << mProjectPath + QL("/assets/audio/") + slow;
    if (kind == BGM)
        candidates << mProjectPath + QL("/assets/audio"); // Legacy-Flachordner
    for (const QString& c : candidates)
        if (QDir(c).exists()) return c;
    return candidates.isEmpty() ? QString() : candidates.first();
}

void QtSoundTestDialog::refill(Kind kind) {
    QListWidget* lw = mLists[(int)kind];
    if (!lw) return;
    lw->clear();

    // XP: erster Eintrag "(None)"
    auto* none = new QListWidgetItem(QL("(Kein)"), lw);
    none->setData(Qt::UserRole, QString());

    const QString dirPath = folderFor(kind);
    QDir dir(dirPath);
    const QFileInfoList files = dir.entryInfoList(audioFilters(),
                                                  QDir::Files | QDir::Readable,
                                                  QDir::Name);
    for (const QFileInfo& fi : files) {
        auto* item = new QListWidgetItem(fi.completeBaseName(), lw);
        item->setData(Qt::UserRole, fi.absoluteFilePath());
        item->setToolTip(fi.absoluteFilePath());
    }
    if (files.isEmpty()) {
        auto* hint = new QListWidgetItem(
            QL("— keine Dateien in „%1“ —").arg(dirPath), lw);
        hint->setFlags(Qt::NoItemFlags); // nicht auswählbar
        hint->setData(Qt::UserRole, QString());
    }
}

QString QtSoundTestDialog::selectedPath() const {
    const int idx = mTabs->currentIndex();
    if (idx < 0 || idx >= 4) return QString();
    QListWidgetItem* item = mLists[idx]->currentItem();
    return item ? item->data(Qt::UserRole).toString() : QString();
}

void QtSoundTestDialog::onPlay() {
    stopCurrent();
    const QString path = selectedPath();
    if (path.isEmpty() || !mEngine) return; // "(Kein)" -> nur stoppen
    if (!QFileInfo::exists(path)) {
        QMessageBox::warning(this, QL("Sound-Test"),
            QL("Die Datei wurde nicht gefunden:\n%1").arg(path));
        return;
    }
    const float vol = mVolume->value() / 100.0f;
    const float pitch = mPitch->value() / 100.0f;
    auto& audio = mEngine->GetAudio();
    switch ((Kind)mTabs->currentIndex()) {
        case BGM: mHandle = audio.PlayBGM(path.toStdString(), true,  vol, pitch); break;
        case BGS: mHandle = audio.PlayBGS(path.toStdString(), true,  vol, pitch); break;
        case ME:  mHandle = audio.PlayME (path.toStdString(), false, vol, pitch); break;
        case SE:  mHandle = audio.PlaySE (path.toStdString(), false, vol, pitch); break;
    }
}

void QtSoundTestDialog::onStop() {
    stopCurrent();
}

void QtSoundTestDialog::stopCurrent() {
    if (mHandle.valid() && mEngine) {
        mEngine->GetAudio().Stop(mHandle);
        mHandle = rpg::SoundHandle{};
    }
}

void QtSoundTestDialog::onVolumeChanged(int v) {
    mVolumeLabel->setText(QL("Lautstärke: %1 %").arg(v));
    if (mHandle.valid() && mEngine)
        mEngine->GetAudio().SetVolume(mHandle, v / 100.0f);
}

void QtSoundTestDialog::onPitchChanged(int v) {
    mPitchLabel->setText(QL("Pitch: %1 %").arg(v));
    if (mHandle.valid() && mEngine)
        mEngine->GetAudio().SetPitch(mHandle, v / 100.0f);
}

void QtSoundTestDialog::done(int r) {
    stopCurrent(); // wie XP: Schließen stoppt die Wiedergabe
    QDialog::done(r);
}

} // namespace qt_editor
