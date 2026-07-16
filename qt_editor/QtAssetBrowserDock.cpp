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
#include <QDir>
#include <QDirIterator>
#include <QDesktopServices>
#include <QUrl>
#include <QClipboard>
#include <QApplication>
#include <QHeaderView>
#include <QSet>

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
    auto* bOpen = new QPushButton("Oeffnen", this);
    auto* bCopy = new QPushButton("Pfad kopieren", this);
    connect(bRef, &QPushButton::clicked, this, &QtAssetBrowserDock::onRefresh);
    connect(bOpen, &QPushButton::clicked, this, &QtAssetBrowserDock::onOpenExternal);
    connect(bCopy, &QPushButton::clicked, this, &QtAssetBrowserDock::onCopyPath);
    tb->addWidget(bRef);
    tb->addWidget(bOpen);
    tb->addWidget(bCopy);
    tb->addStretch(1);
    root->addLayout(tb);

    mFilter = new QLineEdit(this);
    mFilter->setPlaceholderText("Filter (z.B. .png, tileset, audio)...");
    connect(mFilter, &QLineEdit::textChanged, this, &QtAssetBrowserDock::onFilterChanged);
    root->addWidget(mFilter);

    mTree = new QTreeWidget(this);
    mTree->setHeaderLabels(QStringList() << "Asset" << "Typ");
    mTree->header()->setStretchLastSection(true);
    mTree->setRootIsDecorated(true);
    mTree->setAlternatingRowColors(true);
    connect(mTree, &QTreeWidget::itemDoubleClicked, this, [this](QTreeWidgetItem*, int) {
        onItemActivated();
    });
    root->addWidget(mTree, 1);

    mInfo = new QLabel("Doppelklick = oeffnen. Texturen/Audio/Scripts aus Projekt + assets/.", this);
    mInfo->setWordWrap(true);
    root->addWidget(mInfo);
}

void QtAssetBrowserDock::refresh() {
    mRootPath.clear();
    if (mEngine && !mEngine->GetProject().GetProjectPath().empty())
        mRootPath = QString::fromStdString(mEngine->GetProject().GetProjectPath());
    if (mRootPath.isEmpty()) mRootPath = ".";

    mTree->clear();
    // Projekt-Unterordner
    const QStringList roots = {
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
        top->setData(0, Qt::UserRole, fi.absoluteFilePath());
        QDirIterator it(fi.absoluteFilePath(), QDir::Files | QDir::NoSymLinks, QDirIterator::Subdirectories);
        while (it.hasNext()) {
            it.next();
            const QFileInfo f = it.fileInfo();
            auto* item = new QTreeWidgetItem(top);
            item->setText(0, QDir(fi.absoluteFilePath()).relativeFilePath(f.absoluteFilePath()));
            item->setText(1, f.suffix().toLower());
            item->setData(0, Qt::UserRole, f.absoluteFilePath());
        }
        top->setExpanded(true);
    }
    onFilterChanged(mFilter ? mFilter->text() : QString());
    mInfo->setText(QString("Root: %1").arg(mRootPath));
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
    emit logMessage("Asset geoeffnet: " + path);
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

} // namespace qt_editor
