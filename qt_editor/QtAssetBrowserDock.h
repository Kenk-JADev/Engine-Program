#pragma once
// Asset-Browser: durchsucht Projekt-/Assets-Ordner, oeffnet Dateien und
// importiert Medien-Dateien in die XP-Ordnerstruktur (Graphics/…, Audio/…)
// des Projekts — XP-„Material base"-Dialog als Import-Kategorie-Wahl.

#include <QWidget>
#include <QString>
#include <QStringList>

class QTreeWidget;
class QLabel;
class QLineEdit;
class QPushButton;
class QMimeData;
class QPoint;

namespace rpg { class Engine; }

namespace qt_editor {

class QtAssetBrowserDock : public QWidget {
    Q_OBJECT
public:
    explicit QtAssetBrowserDock(rpg::Engine* engine, QWidget* parent = nullptr);
    void refresh();

    /// true, wenn die Datei ein importierbares Medium ist (Bild oder Audio).
    static bool IsMediaFile(const QString& path);

signals:
    void logMessage(const QString& msg);
    void assetActivated(const QString& path);

private slots:
    void onRefresh();
    void onItemActivated();
    void onFilterChanged(const QString& text);
    void onOpenExternal();
    void onCopyPath();
    void onImport();            // XP-Importdialog („Material base") via Dateidialog

private:
    void buildUi();
    void scanDir(const QString& root, const QString& rel, QTreeWidget* tree);
    QString selectedPath() const;

    /// Kopiert Dateien nach destDirAbs (Ordner wird angelegt), fragt bei
    /// Konflikten nach (Ueberschreiben/Ueberspringen), refreshed danach.
    void importFilesInto(const QStringList& files, const QString& destDirAbs);
    /// Drag&Drop aus dem Dateimanager: Zielordner aus Drop-Position ableiten
    /// (Datei -> ihr Ordner, Ordner -> er selbst, Leere -> Kategorie-Dialog).
    bool handleExternalDrop(const QMimeData* mime, const QPoint& pos);
    /// XP-Kategorie-Dialog; liefert relativen Zielpfad ("Graphics/Tilesets")
    /// oder "" bei Abbruch. wantAudio/wantImage filtern die Kategorieliste,
    /// fileCount ist nur Anzeige (Ueberschrift).
    QString chooseImportCategory(bool wantAudio, bool wantImage, int fileCount);

    rpg::Engine* mEngine = nullptr;
    QTreeWidget* mTree = nullptr;
    QLineEdit* mFilter = nullptr;
    QLabel* mInfo = nullptr;
    QString mRootPath;
};

} // namespace qt_editor
