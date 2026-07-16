#pragma once
// Asset-Browser: durchsucht Projekt-/Assets-Ordner und oeffnet Dateien.

#include <QWidget>
#include <QString>

class QTreeWidget;
class QLabel;
class QLineEdit;
class QPushButton;

namespace rpg { class Engine; }

namespace qt_editor {

class QtAssetBrowserDock : public QWidget {
    Q_OBJECT
public:
    explicit QtAssetBrowserDock(rpg::Engine* engine, QWidget* parent = nullptr);
    void refresh();

signals:
    void logMessage(const QString& msg);
    void assetActivated(const QString& path);

private slots:
    void onRefresh();
    void onItemActivated();
    void onFilterChanged(const QString& text);
    void onOpenExternal();
    void onCopyPath();

private:
    void buildUi();
    void scanDir(const QString& root, const QString& rel, QTreeWidget* tree);
    QString selectedPath() const;

    rpg::Engine* mEngine = nullptr;
    QTreeWidget* mTree = nullptr;
    QLineEdit* mFilter = nullptr;
    QLabel* mInfo = nullptr;
    QString mRootPath;
};

} // namespace qt_editor
