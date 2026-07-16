#pragma once
// Database-Editor: QTableView-Modelle fuer Actors / Items / Enemies / System.

#include <QWidget>
#include <QString>
#include <QAbstractTableModel>
#include <vector>

class QTabWidget;
class QTableView;
class QLineEdit;
class QSpinBox;
class QFormLayout;

namespace rpg {
class Engine;
struct ActorData;
struct ItemData;
struct EnemyData;
}

namespace qt_editor {

class ActorsTableModel : public QAbstractTableModel {
    Q_OBJECT
public:
    explicit ActorsTableModel(QObject* parent = nullptr);
    void reload();
    int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    int columnCount(const QModelIndex& parent = QModelIndex()) const override;
    QVariant data(const QModelIndex& index, int role) const override;
    bool setData(const QModelIndex& index, const QVariant& value, int role) override;
    QVariant headerData(int section, Qt::Orientation o, int role) const override;
    Qt::ItemFlags flags(const QModelIndex& index) const override;
    bool insertRows(int row, int count, const QModelIndex& parent = QModelIndex()) override;
    bool removeRows(int row, int count, const QModelIndex& parent = QModelIndex()) override;
};

class ItemsTableModel : public QAbstractTableModel {
    Q_OBJECT
public:
    explicit ItemsTableModel(QObject* parent = nullptr);
    void reload();
    int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    int columnCount(const QModelIndex& parent = QModelIndex()) const override;
    QVariant data(const QModelIndex& index, int role) const override;
    bool setData(const QModelIndex& index, const QVariant& value, int role) override;
    QVariant headerData(int section, Qt::Orientation o, int role) const override;
    Qt::ItemFlags flags(const QModelIndex& index) const override;
    bool insertRows(int row, int count, const QModelIndex& parent = QModelIndex()) override;
    bool removeRows(int row, int count, const QModelIndex& parent = QModelIndex()) override;
};

class EnemiesTableModel : public QAbstractTableModel {
    Q_OBJECT
public:
    explicit EnemiesTableModel(QObject* parent = nullptr);
    void reload();
    int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    int columnCount(const QModelIndex& parent = QModelIndex()) const override;
    QVariant data(const QModelIndex& index, int role) const override;
    bool setData(const QModelIndex& index, const QVariant& value, int role) override;
    QVariant headerData(int section, Qt::Orientation o, int role) const override;
    Qt::ItemFlags flags(const QModelIndex& index) const override;
    bool insertRows(int row, int count, const QModelIndex& parent = QModelIndex()) override;
    bool removeRows(int row, int count, const QModelIndex& parent = QModelIndex()) override;
};

class QtDatabaseEditorDock : public QWidget {
    Q_OBJECT
public:
    explicit QtDatabaseEditorDock(rpg::Engine* engine, QWidget* parent = nullptr);

    void refresh();

signals:
    void logMessage(const QString& msg);

private slots:
    void onSave();
    void onReload();
    void onAddRow();
    void onRemoveRow();
    void onSystemApply();

private:
    void buildUi();
    void syncSystemForm();

    rpg::Engine* mEngine = nullptr;
    QTabWidget* mTabs = nullptr;
    QTableView* mActorsView = nullptr;
    QTableView* mItemsView = nullptr;
    QTableView* mEnemiesView = nullptr;
    ActorsTableModel* mActorsModel = nullptr;
    ItemsTableModel* mItemsModel = nullptr;
    EnemiesTableModel* mEnemiesModel = nullptr;

    QLineEdit* mGameTitle = nullptr;
    QLineEdit* mCurrency = nullptr;
    QSpinBox* mStartMap = nullptr;
    QSpinBox* mStartX = nullptr;
    QSpinBox* mStartY = nullptr;
    QLineEdit* mBattleBgm = nullptr;
    QLineEdit* mTitleBgm = nullptr;
};

} // namespace qt_editor
