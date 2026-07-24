#include "QtAssetBrowserDock.h"

#include "rpgmaker3d/Engine.h"
#include "rpgmaker3d/Project.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QTreeWidget>
#include <QTreeWidgetItem>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QFileInfo>
#include <QFile>
#include <QDir>
#include <QDirIterator>
#include <QDesktopServices>
#include <QUrl>
#include <QClipboard>
#include <QApplication>
#include <QHeaderView>
#include <QSet>
#include <QFileDialog>
#include <QMessageBox>
#include <QComboBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QMimeData>
#include <QDragEnterEvent>
#include <QDragMoveEvent>
#include <QDropEvent>
#include <QPoint>
#include <functional>

namespace {

// Tree-Widget mit Drag&Drop-Aufnahme fuer externe Dateien (Dateimanager ->
// Asset-Browser = Import). Internes QTreeWidget-DnD bleibt unangetastet;
// nur URL-MimeData (lokale Dateien) wird abgefangen. Kein Q_OBJECT noetig:
// es werden nur virtuelle Methoden ueberschrieben.
class AssetTreeWidget : public QTreeWidget {
public:
    using QTreeWidget::QTreeWidget;
    std::function<bool(const QMimeData*, const QPoint&)> externalDrop;

protected:
    void dragEnterEvent(QDragEnterEvent* e) override {
        if (externalDrop && e->mimeData()->hasUrls()) { e->acceptProposedAction(); return; }
        QTreeWidget::dragEnterEvent(e);
    }
    void dragMoveEvent(QDragMoveEvent* e) override {
        if (externalDrop && e->mimeData()->hasUrls()) { e->acceptProposedAction(); return; }
        QTreeWidget::dragMoveEvent(e);
    }
    void dropEvent(QDropEvent* e) override {
        if (externalDrop && e->mimeData()->hasUrls() &&
            externalDrop(e->mimeData(), e->position().toPoint())) {
            e->acceptProposedAction();
            return;
        }
        QTreeWidget::dropEvent(e);
    }
};

// XP-„Material base"-Zielwahl: Kategorie-Combo mit den XP-Ordnern, die die
// Engine auch wirklich durchsucht (RgssResolveGraphic / RPG::Cache /
// Engine::ResolveAudioPath). Kein Q_OBJECT: nur Lambdas/Standard-Signale.
class ImportTargetDialog : public QDialog {
public:
    ImportTargetDialog(QWidget* parent, bool wantAudio, bool wantImage,
                       int fileCount, const QString& preselectRel)
        : QDialog(parent) {
        setWindowTitle("Material importieren");
        setMinimumWidth(420);
        auto* lay = new QVBoxLayout(this);
        auto* head = new QLabel(
            QString("%1 Datei(en) importieren — Ziel-Kategorie im Projekt wählen:")
                .arg(fileCount), this);
        head->setWordWrap(true);
        lay->addWidget(head);

        mCombo = new QComboBox(this);
        auto add = [this](const QString& label, const QString& rel) {
            mCombo->addItem(label, rel);
        };
        if (wantImage) {
            add("Grafik: Tilesets (Graphics/Tilesets)",        "Graphics/Tilesets");
            add("Grafik: Autotiles (Graphics/Autotiles)",      "Graphics/Autotiles");
            add("Grafik: Charaktere (Graphics/Characters)",    "Graphics/Characters");
            add("Grafik: Animationen (Graphics/Animations)",   "Graphics/Animations");
            add("Grafik: Battler/Gegner (Graphics/Battlers)",  "Graphics/Battlers");
            add("Grafik: Battlebacks (Graphics/Battlebacks)",  "Graphics/Battlebacks");
            add("Grafik: Panoramen (Graphics/Panoramas)",      "Graphics/Panoramas");
            add("Grafik: Nebel (Graphics/Fogs)",               "Graphics/Fogs");
            add("Grafik: Pictures (Graphics/Pictures)",        "Graphics/Pictures");
            add("Grafik: Titel (Graphics/Titles)",             "Graphics/Titles");
            add("Grafik: Gameover (Graphics/Gameovers)",       "Graphics/Gameovers");
            add("Grafik: Icons (Graphics/Icons)",              "Graphics/Icons");
            add("Grafik: Übergänge (Graphics/Transitions)",    "Graphics/Transitions");
            add("Grafik: System/Windowskin (Graphics/System)", "Graphics/System");
            add("Grafik: Windowskins (Graphics/Windowskins)",  "Graphics/Windowskins");
        }
        if (wantAudio) {
            add("Audio: BGM (Audio/BGM)", "Audio/BGM");
            add("Audio: BGS (Audio/BGS)", "Audio/BGS");
            add("Audio: ME (Audio/ME)",   "Audio/ME");
            add("Audio: SE (Audio/SE)",   "Audio/SE");
        }
        if (!preselectRel.isEmpty()) {
            const int idx = mCombo->findData(preselectRel);
            if (idx >= 0) mCombo->setCurrentIndex(idx);
        }
        lay->addWidget(mCombo);

        auto* note = new QLabel(
            "Hinweis zu Transparenz: Die Engine nutzt echten Alpha-Kanal (PNG). "
            "Der XP-Farbschlüssel-Import (linke/rechte Transparenzfarbe aus dem "
            "XP-Dialog) entfällt bewusst — Grafiken bitte als PNG mit Alpha "
            "speichern.", this);
        note->setWordWrap(true);
        note->setStyleSheet("color: #a80; ");
        lay->addWidget(note);

        auto* bb = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
        bb->button(QDialogButtonBox::Ok)->setText("Importieren");
        connect(bb, &QDialogButtonBox::accepted, this, &QDialog::accept);
        connect(bb, &QDialogButtonBox::rejected, this, &QDialog::reject);
        lay->addWidget(bb);
    }

    QString selectedRel() const { return mCombo->currentData().toString(); }

private:
    QComboBox* mCombo = nullptr;
};

} // anonymous namespace

namespace qt_editor {

QtAssetBrowserDock::QtAssetBrowserDock(rpg::Engine* engine, QWidget* parent)
    : QWidget(parent), mEngine(engine) {
    buildUi();
    refresh();
}

void QtAssetBrowserDock::buildUi() {
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(4, 4, 4, 4);

    auto* tb = new QHBoxLayout();
    auto* bRef = new QPushButton("Aktualisieren", this);
    auto* bOpen = new QPushButton("Öffnen", this);
    auto* bCopy = new QPushButton("Pfad kopieren", this);
    auto* bImport = new QPushButton("Importieren…", this);
    bImport->setToolTip("Medien-Datei ins Projekt kopieren (XP: „Material base“)");
    connect(bRef, &QPushButton::clicked, this, &QtAssetBrowserDock::onRefresh);
    connect(bOpen, &QPushButton::clicked, this, &QtAssetBrowserDock::onOpenExternal);
    connect(bCopy, &QPushButton::clicked, this, &QtAssetBrowserDock::onCopyPath);
    connect(bImport, &QPushButton::clicked, this, &QtAssetBrowserDock::onImport);
    tb->addWidget(bRef);
    tb->addWidget(bOpen);
    tb->addWidget(bCopy);
    tb->addWidget(bImport);
    tb->addStretch(1);
    root->addLayout(tb);

    mFilter = new QLineEdit(this);
    mFilter->setPlaceholderText("Filter (z.B. .png, tileset, audio)...");
    connect(mFilter, &QLineEdit::textChanged, this, &QtAssetBrowserDock::onFilterChanged);
    root->addWidget(mFilter);

    auto* tree = new AssetTreeWidget(this);
    tree->externalDrop = [this](const QMimeData* mime, const QPoint& pos) {
        return handleExternalDrop(mime, pos);
    };
    mTree = tree;
    mTree->setHeaderLabels(QStringList() << "Asset" << "Typ");
    // Easy-to-use: Name darf so breit wie möglich sein (nichts abschneiden),
    // "Typ" nur so breit wie nötig; voller Name steht im Tooltip.
    mTree->header()->setStretchLastSection(false);
    mTree->header()->setSectionResizeMode(0, QHeaderView::Stretch);
    mTree->header()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    mTree->setRootIsDecorated(true);
    mTree->setAlternatingRowColors(true);
    // Drag&Drop: externe Dateien aus dem Dateimanager auf den Baum ziehen
    // importiert sie (Ziel = Ordner unter dem Cursor). Internes Verschieben
    // von Eintraegen bleibt abgeschaltet.
    mTree->setAcceptDrops(true);
    mTree->setDropIndicatorShown(true);
    mTree->setDragDropMode(QAbstractItemView::DropOnly);
    connect(mTree, &QTreeWidget::itemDoubleClicked, this, [this](QTreeWidgetItem*, int) {
        onItemActivated();
    });
    root->addWidget(mTree, 1);

    mInfo = new QLabel(
        "Doppelklick = öffnen. „Importieren…“ oder Dateien per Drag & Drop "
        "hierher ziehen = ins Projekt kopieren (Graphics/…, Audio/…).",
        this);
    mInfo->setWordWrap(true);
    root->addWidget(mInfo);
}

void QtAssetBrowserDock::refresh() {
    // Engine erst nach initializeGL initialisiert; GetProject() wuerde sonst
    // auf nullptr dereferenzieren. Guard, damit engineReady() neu befuellt.
    if (!mEngine || !mEngine->IsInitialized()) return;
    mRootPath.clear();
    if (mEngine && !mEngine->GetProject().GetProjectPath().empty())
        mRootPath = QString::fromStdString(mEngine->GetProject().GetProjectPath());
    if (mRootPath.isEmpty()) mRootPath = ".";

    mTree->clear();
    // Projekt-Unterordner. Graphics/ + Audio/ (XP-Ordnerstruktur) zuerst:
    // hier landen importierte Medien — sie muessen sichtbar sein.
    const QStringList roots = {
        mRootPath + "/Graphics",
        mRootPath + "/Audio",
        mRootPath + "/assets",
        mRootPath + "/scripts",
        mRootPath + "/database",
        mRootPath + "/maps",
        "assets",
        "ruby"
    };
    QSet<QString> seen;
    for (const QString& r : roots) {
        QFileInfo fi(r);
        if (!fi.exists() || !fi.isDir()) continue;
        const QString key = fi.absoluteFilePath();
        if (seen.contains(key)) continue;
        seen.insert(key);
        auto* top = new QTreeWidgetItem(mTree);
        top->setText(0, fi.fileName().isEmpty() ? r : fi.fileName());
        top->setText(1, "Ordner");
        top->setData(0, Qt::UserRole, fi.absoluteFilePath());
        scanDir(fi.absoluteFilePath(), fi.absoluteFilePath(), mTree);
        // reparent children under top
        // scanDir adds to tree root - fix: scan into top
    }
    // Re-scan properly into folders
    mTree->clear();
    for (const QString& r : roots) {
        QFileInfo fi(r);
        if (!fi.exists() || !fi.isDir()) continue;
        const QString key = fi.absoluteFilePath();
        if (seen.contains(key + "#done")) continue;
        seen.insert(key + "#done");
        auto* top = new QTreeWidgetItem(mTree);
        top->setText(0, QDir(mRootPath).relativeFilePath(fi.absoluteFilePath()));
        if (top->text(0).isEmpty() || top->text(0) == ".") top->setText(0, fi.fileName());
        top->setText(1, "Ordner");
        top->setToolTip(0, fi.absoluteFilePath());
        top->setData(0, Qt::UserRole, fi.absoluteFilePath());
        QDirIterator it(fi.absoluteFilePath(), QDir::Files | QDir::NoSymLinks, QDirIterator::Subdirectories);
        while (it.hasNext()) {
            it.next();
            const QFileInfo f = it.fileInfo();
            auto* item = new QTreeWidgetItem(top);
            item->setText(0, QDir(fi.absoluteFilePath()).relativeFilePath(f.absoluteFilePath()));
            item->setText(1, f.suffix().toLower());
            item->setToolTip(0, f.absoluteFilePath()); // voller Pfad im Tooltip
            item->setData(0, Qt::UserRole, f.absoluteFilePath());
        }
        top->setExpanded(true);
    }
    onFilterChanged(mFilter ? mFilter->text() : QString());
    mInfo->setText(QString(
        "Root: %1 — Doppelklick = öffnen, Drag & Drop / „Importieren…“ = ins Projekt kopieren.")
            .arg(mRootPath));
}

void QtAssetBrowserDock::scanDir(const QString&, const QString&, QTreeWidget*) {
    // unused – scan happens in refresh()
}

void QtAssetBrowserDock::onRefresh() { refresh(); }

QString QtAssetBrowserDock::selectedPath() const {
    auto* item = mTree->currentItem();
    if (!item) return {};
    return item->data(0, Qt::UserRole).toString();
}

void QtAssetBrowserDock::onItemActivated() {
    const QString path = selectedPath();
    if (path.isEmpty()) return;
    QFileInfo fi(path);
    if (fi.isDir()) return;
    QDesktopServices::openUrl(QUrl::fromLocalFile(path));
    emit logMessage("Asset geöffnet: " + path);
    emit assetActivated(path);
}

void QtAssetBrowserDock::onOpenExternal() { onItemActivated(); }

void QtAssetBrowserDock::onCopyPath() {
    const QString path = selectedPath();
    if (path.isEmpty()) return;
    QApplication::clipboard()->setText(path);
    emit logMessage("Pfad kopiert: " + path);
}

void QtAssetBrowserDock::onFilterChanged(const QString& text) {
    const QString f = text.trimmed().toLower();
    for (int i = 0; i < mTree->topLevelItemCount(); ++i) {
        auto* top = mTree->topLevelItem(i);
        bool any = false;
        for (int j = 0; j < top->childCount(); ++j) {
            auto* c = top->child(j);
            const bool match = f.isEmpty()
                || c->text(0).toLower().contains(f)
                || c->text(1).toLower().contains(f);
            c->setHidden(!match);
            if (match) any = true;
        }
        top->setHidden(!any && !f.isEmpty());
    }
}

// ---------------------------------------------------------------------------
// XP-Import („Material base"): Medien-Dateien in Projekt-Kategorien kopieren
// ---------------------------------------------------------------------------

bool QtAssetBrowserDock::IsMediaFile(const QString& path) {
    const QString sfx = QFileInfo(path).suffix().toLower();
    static const QSet<QString> kImg = {"png", "jpg", "jpeg", "bmp"};
    static const QSet<QString> kAud = {"ogg", "mp3", "wav", "flac"};
    return kImg.contains(sfx) || kAud.contains(sfx);
}

void QtAssetBrowserDock::onImport() {
    if (mRootPath.isEmpty()) {
        emit logMessage("Import: kein Projektpfad — bitte zuerst ein Projekt öffnen.");
        return;
    }
    // XP-Importdialog: Datei(en) waehlen -> Kategorie waehlen -> kopieren.
    const QString filter =
        "Medien-Dateien (*.png *.jpg *.jpeg *.bmp *.ogg *.mp3 *.wav *.flac);;"
        "Bilder (*.png *.jpg *.jpeg *.bmp);;"
        "Audio (*.ogg *.mp3 *.wav *.flac);;"
        "Alle Dateien (*)";
    const QStringList files = QFileDialog::getOpenFileNames(
        this, "Material importieren (XP: „Material base“)", QString(), filter);
    if (files.isEmpty()) return;

    QStringList media;
    bool wantAudio = false, wantImage = false;
    for (const QString& f : files) {
        if (!IsMediaFile(f)) {
            emit logMessage("Import: übersprungen (kein Medien-Format): " + f);
            continue;
        }
        media << f;
        const QString sfx = QFileInfo(f).suffix().toLower();
        if (sfx == "ogg" || sfx == "mp3" || sfx == "wav" || sfx == "flac") wantAudio = true;
        else wantImage = true;
    }
    if (media.isEmpty()) return;

    const QString rel = chooseImportCategory(wantAudio, wantImage, (int)media.size());
    if (rel.isEmpty()) return; // Abbruch
    importFilesInto(media, mRootPath + "/" + rel);
}

QString QtAssetBrowserDock::chooseImportCategory(bool wantAudio, bool wantImage,
                                                 int fileCount) {
    // Vorauswahl: bei reinem Audio SE (haeufigster Fall), sonst Tilesets.
    const QString preselect = wantAudio && !wantImage
        ? QString("Audio/SE") : QString("Graphics/Tilesets");
    ImportTargetDialog dlg(this, wantAudio, wantImage, fileCount, preselect);
    if (dlg.exec() != QDialog::Accepted) return QString();
    return dlg.selectedRel();
}

void QtAssetBrowserDock::importFilesInto(const QStringList& files, const QString& destDirAbs) {
    if (files.isEmpty() || destDirAbs.isEmpty()) return;
    QDir dest(destDirAbs);
    if (!dest.exists() && !dest.mkpath(".")) {
        emit logMessage("Import: Zielordner konnte nicht angelegt werden: " + destDirAbs);
        QMessageBox::warning(this, "Import",
            "Zielordner konnte nicht angelegt werden:\n" + destDirAbs);
        return;
    }

    int copied = 0, skipped = 0, failed = 0;
    bool overwriteAll = false;
    for (const QString& f : files) {
        const QFileInfo src(f);
        const QString dst = destDirAbs + "/" + src.fileName();
        if (QFile::exists(dst) && !overwriteAll) {
            QMessageBox mb(QMessageBox::Question, "Datei existiert",
                src.fileName() + " existiert im Ziel bereits.\nÜberschreiben?",
                QMessageBox::Yes | QMessageBox::YesToAll | QMessageBox::No, this);
            mb.button(QMessageBox::Yes)->setText("Überschreiben");
            mb.button(QMessageBox::YesToAll)->setText("Alle überschreiben");
            mb.button(QMessageBox::No)->setText("Überspringen");
            const int r = mb.exec();
            if (r == QMessageBox::No) { ++skipped; continue; }
            if (r == QMessageBox::YesToAll) overwriteAll = true;
        }
        if (QFile::exists(dst)) QFile::remove(dst); // QFile::copy scheitert sonst
        if (QFile::copy(f, dst)) {
            ++copied;
            emit logMessage("Importiert: " + src.fileName() + " → " + destDirAbs);
        } else {
            ++failed;
            emit logMessage("Import FEHLGESCHLAGEN: " + f);
        }
    }
    emit logMessage(QString("Import abgeschlossen: %1 kopiert, %2 übersprungen, %3 fehlgeschlagen.")
                        .arg(copied).arg(skipped).arg(failed));
    refresh(); // Neue Dateien sofort im Baum zeigen (easy to use).
}

bool QtAssetBrowserDock::handleExternalDrop(const QMimeData* mime, const QPoint& pos) {
    if (!mime || !mime->hasUrls()) return false;
    if (mRootPath.isEmpty()) return false;

    QStringList media;
    bool wantAudio = false, wantImage = false;
    for (const QUrl& url : mime->urls()) {
        if (!url.isLocalFile()) continue;
        const QString f = url.toLocalFile();
        const QFileInfo fi(f);
        if (!fi.isFile() || !IsMediaFile(f)) continue;
        media << f;
        const QString sfx = fi.suffix().toLower();
        if (sfx == "ogg" || sfx == "mp3" || sfx == "wav" || sfx == "flac") wantAudio = true;
        else wantImage = true;
    }
    if (media.isEmpty()) {
        emit logMessage("Drag&Drop: keine importierbaren Medien-Dateien dabei.");
        return false;
    }

    // Zielordner aus Drop-Position: Datei -> ihr Ordner, Ordner -> er selbst.
    QString destDir;
    auto* item = mTree ? mTree->itemAt(pos) : nullptr;
    if (item) {
        const QFileInfo fi(item->data(0, Qt::UserRole).toString());
        if (fi.isDir()) destDir = fi.absoluteFilePath();
        else if (fi.isFile()) destDir = fi.absolutePath();
    }
    if (destDir.isEmpty()) {
        // Drop in die Leere: XP-Weg — Kategorie-Dialog.
        const QString rel = chooseImportCategory(wantAudio, wantImage, (int)media.size());
        if (rel.isEmpty()) return true; // konsumiert, aber abgebrochen
        destDir = mRootPath + "/" + rel;
    }
    importFilesInto(media, destDir);
    return true;
}

} // namespace qt_editor
