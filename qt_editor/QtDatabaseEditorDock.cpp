#include "QtDatabaseEditorDock.h"

#include "rpgmaker3d/Engine.h"
#include "rpgmaker3d/Database.h"
#include "rpgmaker3d/Project.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QTabWidget>
#include <QTableView>
#include <QAbstractItemView>
#include <QHeaderView>
#include <QPushButton>
#include <QLineEdit>
#include <QSpinBox>
#include <QGroupBox>
#include <QMessageBox>
#include <QLabel>
#include <algorithm>

namespace qt_editor {

// ---------------------------------------------------------------------------
// Actors
// ---------------------------------------------------------------------------
ActorsTableModel::ActorsTableModel(QObject* parent) : QAbstractTableModel(parent) {}

void ActorsTableModel::reload() {
    beginResetModel();
    endResetModel();
}

int ActorsTableModel::rowCount(const QModelIndex& parent) const {
    if (parent.isValid()) return 0;
    return static_cast<int>(rpg::Database::Get().Actors().size());
}

int ActorsTableModel::columnCount(const QModelIndex&) const { return 6; }

QVariant ActorsTableModel::headerData(int section, Qt::Orientation o, int role) const {
    if (o != Qt::Horizontal || role != Qt::DisplayRole) return {};
    static const char* h[] = {"ID", "Name", "Klasse", "Level", "MHP", "ATK"};
    return (section >= 0 && section < 6) ? h[section] : QVariant();
}

QVariant ActorsTableModel::data(const QModelIndex& index, int role) const {
    if (!index.isValid() || (role != Qt::DisplayRole && role != Qt::EditRole)) return {};
    auto& a = rpg::Database::Get().Actors()[static_cast<size_t>(index.row())];
    switch (index.column()) {
        case 0: return a.id;
        case 1: return QString::fromStdString(a.name);
        case 2: return QString::fromStdString(a.className);
        case 3: return a.initialLevel;
        case 4: return a.initialStats.mhp;
        case 5: return a.initialStats.atk;
    }
    return {};
}

bool ActorsTableModel::setData(const QModelIndex& index, const QVariant& value, int role) {
    if (!index.isValid() || role != Qt::EditRole) return false;
    auto& a = rpg::Database::Get().Actors()[static_cast<size_t>(index.row())];
    switch (index.column()) {
        case 0: a.id = value.toInt(); break;
        case 1: a.name = value.toString().toStdString(); break;
        case 2: a.className = value.toString().toStdString(); break;
        case 3: a.initialLevel = value.toInt(); break;
        case 4: a.initialStats.mhp = value.toInt(); break;
        case 5: a.initialStats.atk = value.toInt(); break;
        default: return false;
    }
    emit dataChanged(index, index);
    return true;
}

Qt::ItemFlags ActorsTableModel::flags(const QModelIndex& index) const {
    if (!index.isValid()) return Qt::NoItemFlags;
    return Qt::ItemIsSelectable | Qt::ItemIsEnabled | Qt::ItemIsEditable;
}

bool ActorsTableModel::insertRows(int row, int count, const QModelIndex& parent) {
    if (parent.isValid()) return false;
    auto& v = rpg::Database::Get().Actors();
    beginInsertRows(parent, row, row + count - 1);
    for (int i = 0; i < count; ++i) {
        rpg::ActorData a;
        a.id = v.empty() ? 1 : v.back().id + 1;
        a.name = "Actor " + std::to_string(a.id);
        v.insert(v.begin() + row + i, a);
    }
    endInsertRows();
    return true;
}

bool ActorsTableModel::removeRows(int row, int count, const QModelIndex& parent) {
    if (parent.isValid()) return false;
    auto& v = rpg::Database::Get().Actors();
    if (row < 0 || row + count > static_cast<int>(v.size())) return false;
    beginRemoveRows(parent, row, row + count - 1);
    v.erase(v.begin() + row, v.begin() + row + count);
    endRemoveRows();
    return true;
}

// ---------------------------------------------------------------------------
// Items
// ---------------------------------------------------------------------------
ItemsTableModel::ItemsTableModel(QObject* parent) : QAbstractTableModel(parent) {}
void ItemsTableModel::reload() { beginResetModel(); endResetModel(); }
int ItemsTableModel::rowCount(const QModelIndex& parent) const {
    if (parent.isValid()) return 0;
    return static_cast<int>(rpg::Database::Get().Items().size());
}
int ItemsTableModel::columnCount(const QModelIndex&) const { return 5; }
QVariant ItemsTableModel::headerData(int section, Qt::Orientation o, int role) const {
    if (o != Qt::Horizontal || role != Qt::DisplayRole) return {};
    static const char* h[] = {"ID", "Name", "Preis", "HP-Heilung", "MP-Heilung"};
    return (section >= 0 && section < 5) ? h[section] : QVariant();
}
QVariant ItemsTableModel::data(const QModelIndex& index, int role) const {
    if (!index.isValid() || (role != Qt::DisplayRole && role != Qt::EditRole)) return {};
    auto& it = rpg::Database::Get().Items()[static_cast<size_t>(index.row())];
    switch (index.column()) {
        case 0: return it.id;
        case 1: return QString::fromStdString(it.name);
        case 2: return it.price;
        case 3: return it.hpRecovery;
        case 4: return it.mpRecovery;
    }
    return {};
}
bool ItemsTableModel::setData(const QModelIndex& index, const QVariant& value, int role) {
    if (!index.isValid() || role != Qt::EditRole) return false;
    auto& it = rpg::Database::Get().Items()[static_cast<size_t>(index.row())];
    switch (index.column()) {
        case 0: it.id = value.toInt(); break;
        case 1: it.name = value.toString().toStdString(); break;
        case 2: it.price = value.toInt(); break;
        case 3: it.hpRecovery = value.toInt(); break;
        case 4: it.mpRecovery = value.toInt(); break;
        default: return false;
    }
    emit dataChanged(index, index);
    return true;
}
Qt::ItemFlags ItemsTableModel::flags(const QModelIndex& index) const {
    if (!index.isValid()) return Qt::NoItemFlags;
    return Qt::ItemIsSelectable | Qt::ItemIsEnabled | Qt::ItemIsEditable;
}
bool ItemsTableModel::insertRows(int row, int count, const QModelIndex& parent) {
    if (parent.isValid()) return false;
    auto& v = rpg::Database::Get().Items();
    beginInsertRows(parent, row, row + count - 1);
    for (int i = 0; i < count; ++i) {
        rpg::ItemData it;
        it.id = v.empty() ? 1 : v.back().id + 1;
        it.name = "Item " + std::to_string(it.id);
        v.insert(v.begin() + row + i, it);
    }
    endInsertRows();
    return true;
}
bool ItemsTableModel::removeRows(int row, int count, const QModelIndex& parent) {
    if (parent.isValid()) return false;
    auto& v = rpg::Database::Get().Items();
    if (row < 0 || row + count > static_cast<int>(v.size())) return false;
    beginRemoveRows(parent, row, row + count - 1);
    v.erase(v.begin() + row, v.begin() + row + count);
    endRemoveRows();
    return true;
}

// ---------------------------------------------------------------------------
// Enemies
// ---------------------------------------------------------------------------
EnemiesTableModel::EnemiesTableModel(QObject* parent) : QAbstractTableModel(parent) {}
void EnemiesTableModel::reload() { beginResetModel(); endResetModel(); }
int EnemiesTableModel::rowCount(const QModelIndex& parent) const {
    if (parent.isValid()) return 0;
    return static_cast<int>(rpg::Database::Get().Enemies().size());
}
int EnemiesTableModel::columnCount(const QModelIndex&) const { return 6; }
QVariant EnemiesTableModel::headerData(int section, Qt::Orientation o, int role) const {
    if (o != Qt::Horizontal || role != Qt::DisplayRole) return {};
    static const char* h[] = {"ID", "Name", "HP", "ATK", "EXP", "Gold"};
    return (section >= 0 && section < 6) ? h[section] : QVariant();
}
QVariant EnemiesTableModel::data(const QModelIndex& index, int role) const {
    if (!index.isValid() || (role != Qt::DisplayRole && role != Qt::EditRole)) return {};
    auto& e = rpg::Database::Get().Enemies()[static_cast<size_t>(index.row())];
    switch (index.column()) {
        case 0: return e.id;
        case 1: return QString::fromStdString(e.name);
        case 2: return e.maxHp;
        case 3: return e.atk;
        case 4: return e.exp;
        case 5: return e.gold;
    }
    return {};
}
bool EnemiesTableModel::setData(const QModelIndex& index, const QVariant& value, int role) {
    if (!index.isValid() || role != Qt::EditRole) return false;
    auto& e = rpg::Database::Get().Enemies()[static_cast<size_t>(index.row())];
    switch (index.column()) {
        case 0: e.id = value.toInt(); break;
        case 1: e.name = value.toString().toStdString(); break;
        case 2: e.maxHp = value.toInt(); break;
        case 3: e.atk = value.toInt(); break;
        case 4: e.exp = value.toInt(); break;
        case 5: e.gold = value.toInt(); break;
        default: return false;
    }
    emit dataChanged(index, index);
    return true;
}
Qt::ItemFlags EnemiesTableModel::flags(const QModelIndex& index) const {
    if (!index.isValid()) return Qt::NoItemFlags;
    return Qt::ItemIsSelectable | Qt::ItemIsEnabled | Qt::ItemIsEditable;
}
bool EnemiesTableModel::insertRows(int row, int count, const QModelIndex& parent) {
    if (parent.isValid()) return false;
    auto& v = rpg::Database::Get().Enemies();
    beginInsertRows(parent, row, row + count - 1);
    for (int i = 0; i < count; ++i) {
        rpg::EnemyData e;
        e.id = v.empty() ? 1 : v.back().id + 1;
        e.name = "Enemy " + std::to_string(e.id);
        v.insert(v.begin() + row + i, e);
    }
    endInsertRows();
    return true;
}
bool EnemiesTableModel::removeRows(int row, int count, const QModelIndex& parent) {
    if (parent.isValid()) return false;
    auto& v = rpg::Database::Get().Enemies();
    if (row < 0 || row + count > static_cast<int>(v.size())) return false;
    beginRemoveRows(parent, row, row + count - 1);
    v.erase(v.begin() + row, v.begin() + row + count);
    endRemoveRows();
    return true;
}


// ---------------------------------------------------------------------------
// Skills / Weapons / Classes
// ---------------------------------------------------------------------------
SimpleDbTableModel::SimpleDbTableModel(Kind kind, QObject* parent)
    : QAbstractTableModel(parent), mKind(kind) {}
void SimpleDbTableModel::reload() { beginResetModel(); endResetModel(); }
int SimpleDbTableModel::rowCount(const QModelIndex& parent) const {
    if (parent.isValid()) return 0;
    auto& db = rpg::Database::Get();
    switch (mKind) {
        case Skills: return (int)db.Skills().size();
        case Weapons: return (int)db.Weapons().size();
        case Classes: return (int)db.Classes().size();
    }
    return 0;
}
int SimpleDbTableModel::columnCount(const QModelIndex&) const {
    return mKind == Classes ? 3 : 5;
}
QVariant SimpleDbTableModel::headerData(int section, Qt::Orientation o, int role) const {
    if (o != Qt::Horizontal || role != Qt::DisplayRole) return {};
    if (mKind == Skills) {
        static const char* h[] = {"ID","Name","MP","Power","Scope"};
        return (section>=0&&section<5)?h[section]:QVariant();
    }
    if (mKind == Weapons) {
        static const char* h[] = {"ID","Name","Preis","ATK","Anim"};
        return (section>=0&&section<5)?h[section]:QVariant();
    }
    static const char* h[] = {"ID","Name","ExpBase"};
    return (section>=0&&section<3)?h[section]:QVariant();
}
QVariant SimpleDbTableModel::data(const QModelIndex& index, int role) const {
    if (!index.isValid() || (role != Qt::DisplayRole && role != Qt::EditRole)) return {};
    auto& db = rpg::Database::Get();
    const int r = index.row(), c = index.column();
    if (mKind == Skills) {
        auto& s = db.Skills()[(size_t)r];
        switch (c) {
            case 0: return s.id; case 1: return QString::fromStdString(s.name);
            case 2: return s.mpCost; case 3: return s.power; case 4: return s.scope;
        }
    } else if (mKind == Weapons) {
        auto& w = db.Weapons()[(size_t)r];
        switch (c) {
            case 0: return w.id; case 1: return QString::fromStdString(w.name);
            case 2: return w.price; case 3: return w.atk; case 4: return w.animationId;
        }
    } else {
        auto& cl = db.Classes()[(size_t)r];
        switch (c) {
            case 0: return cl.id; case 1: return QString::fromStdString(cl.name);
            case 2: return cl.expBase;
        }
    }
    return {};
}
bool SimpleDbTableModel::setData(const QModelIndex& index, const QVariant& value, int role) {
    if (!index.isValid() || role != Qt::EditRole) return false;
    auto& db = rpg::Database::Get();
    const int r = index.row(), c = index.column();
    if (mKind == Skills) {
        auto& s = db.Skills()[(size_t)r];
        if (c==0) s.id=value.toInt(); else if (c==1) s.name=value.toString().toStdString();
        else if (c==2) s.mpCost=value.toInt(); else if (c==3) s.power=value.toInt();
        else if (c==4) s.scope=value.toInt(); else return false;
    } else if (mKind == Weapons) {
        auto& w = db.Weapons()[(size_t)r];
        if (c==0) w.id=value.toInt(); else if (c==1) w.name=value.toString().toStdString();
        else if (c==2) w.price=value.toInt(); else if (c==3) w.atk=value.toInt();
        else if (c==4) w.animationId=value.toInt(); else return false;
    } else {
        auto& cl = db.Classes()[(size_t)r];
        if (c==0) cl.id=value.toInt(); else if (c==1) cl.name=value.toString().toStdString();
        else if (c==2) cl.expBase=value.toInt(); else return false;
    }
    emit dataChanged(index, index);
    return true;
}
Qt::ItemFlags SimpleDbTableModel::flags(const QModelIndex& index) const {
    if (!index.isValid()) return Qt::NoItemFlags;
    return Qt::ItemIsSelectable | Qt::ItemIsEnabled | Qt::ItemIsEditable;
}
bool SimpleDbTableModel::insertRows(int row, int count, const QModelIndex& parent) {
    if (parent.isValid()) return false;
    auto& db = rpg::Database::Get();
    beginInsertRows(parent, row, row+count-1);
    for (int i=0;i<count;++i) {
        if (mKind == Skills) {
            rpg::SkillData s; s.id = db.Skills().empty()?1:db.Skills().back().id+1;
            s.name = "Skill "+std::to_string(s.id);
            db.Skills().insert(db.Skills().begin()+row+i, s);
        } else if (mKind == Weapons) {
            rpg::WeaponData w; w.id = db.Weapons().empty()?1:db.Weapons().back().id+1;
            w.name = "Weapon "+std::to_string(w.id);
            db.Weapons().insert(db.Weapons().begin()+row+i, w);
        } else {
            rpg::ClassData cl; cl.id = db.Classes().empty()?1:db.Classes().back().id+1;
            cl.name = "Class "+std::to_string(cl.id);
            db.Classes().insert(db.Classes().begin()+row+i, cl);
        }
    }
    endInsertRows();
    return true;
}
bool SimpleDbTableModel::removeRows(int row, int count, const QModelIndex& parent) {
    if (parent.isValid()) return false;
    auto& db = rpg::Database::Get();
    beginRemoveRows(parent, row, row+count-1);
    if (mKind == Skills) db.Skills().erase(db.Skills().begin()+row, db.Skills().begin()+row+count);
    else if (mKind == Weapons) db.Weapons().erase(db.Weapons().begin()+row, db.Weapons().begin()+row+count);
    else db.Classes().erase(db.Classes().begin()+row, db.Classes().begin()+row+count);
    endRemoveRows();
    return true;
}

// ---------------------------------------------------------------------------
// Dock
// ---------------------------------------------------------------------------
QtDatabaseEditorDock::QtDatabaseEditorDock(rpg::Engine* engine, QWidget* parent)
    : QWidget(parent), mEngine(engine) {
    buildUi();
    refresh();
}

void QtDatabaseEditorDock::buildUi() {
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(4, 4, 4, 4);

    auto* tb = new QHBoxLayout();
    auto* btnSave = new QPushButton("Speichern", this);
    auto* btnReload = new QPushButton("Neu laden", this);
    auto* btnAdd = new QPushButton("Zeile +", this);
    auto* btnRem = new QPushButton("Zeile -", this);
    connect(btnSave, &QPushButton::clicked, this, &QtDatabaseEditorDock::onSave);
    connect(btnReload, &QPushButton::clicked, this, &QtDatabaseEditorDock::onReload);
    connect(btnAdd, &QPushButton::clicked, this, &QtDatabaseEditorDock::onAddRow);
    connect(btnRem, &QPushButton::clicked, this, &QtDatabaseEditorDock::onRemoveRow);
    tb->addWidget(btnSave);
    tb->addWidget(btnReload);
    tb->addStretch(1);
    tb->addWidget(btnAdd);
    tb->addWidget(btnRem);
    root->addLayout(tb);

    mTabs = new QTabWidget(this);

    mActorsModel = new ActorsTableModel(this);
    mActorsView = new QTableView(mTabs);
    mActorsView->setModel(mActorsModel);
    mActorsView->horizontalHeader()->setStretchLastSection(true);
    mActorsView->setSelectionBehavior(QAbstractItemView::SelectRows);
    mActorsView->setAlternatingRowColors(true);
    mTabs->addTab(mActorsView, "Actors");

    mItemsModel = new ItemsTableModel(this);
    mItemsView = new QTableView(mTabs);
    mItemsView->setModel(mItemsModel);
    mItemsView->horizontalHeader()->setStretchLastSection(true);
    mItemsView->setSelectionBehavior(QAbstractItemView::SelectRows);
    mItemsView->setAlternatingRowColors(true);
    mTabs->addTab(mItemsView, "Items");

    mEnemiesModel = new EnemiesTableModel(this);
    mEnemiesView = new QTableView(mTabs);
    mEnemiesView->setModel(mEnemiesModel);
    mEnemiesView->horizontalHeader()->setStretchLastSection(true);
    mEnemiesView->setSelectionBehavior(QAbstractItemView::SelectRows);
    mEnemiesView->setAlternatingRowColors(true);
    mTabs->addTab(mEnemiesView, "Enemies");

    mSkillsModel = new SimpleDbTableModel(SimpleDbTableModel::Skills, this);
    mSkillsView = new QTableView(mTabs);
    mSkillsView->setModel(mSkillsModel);
    mSkillsView->horizontalHeader()->setStretchLastSection(true);
    mSkillsView->setSelectionBehavior(QAbstractItemView::SelectRows);
    mSkillsView->setAlternatingRowColors(true);
    mTabs->addTab(mSkillsView, "Skills");

    mWeaponsModel = new SimpleDbTableModel(SimpleDbTableModel::Weapons, this);
    mWeaponsView = new QTableView(mTabs);
    mWeaponsView->setModel(mWeaponsModel);
    mWeaponsView->horizontalHeader()->setStretchLastSection(true);
    mWeaponsView->setSelectionBehavior(QAbstractItemView::SelectRows);
    mWeaponsView->setAlternatingRowColors(true);
    mTabs->addTab(mWeaponsView, "Weapons");

    mClassesModel = new SimpleDbTableModel(SimpleDbTableModel::Classes, this);
    mClassesView = new QTableView(mTabs);
    mClassesView->setModel(mClassesModel);
    mClassesView->horizontalHeader()->setStretchLastSection(true);
    mClassesView->setSelectionBehavior(QAbstractItemView::SelectRows);
    mClassesView->setAlternatingRowColors(true);
    mTabs->addTab(mClassesView, "Classes");

    auto* sys = new QWidget(mTabs);
    auto* form = new QFormLayout(sys);
    mGameTitle = new QLineEdit(sys);
    mCurrency = new QLineEdit(sys);
    mStartMap = new QSpinBox(sys); mStartMap->setRange(1, 9999);
    mStartX = new QSpinBox(sys); mStartX->setRange(0, 9999);
    mStartY = new QSpinBox(sys); mStartY->setRange(0, 9999);
    mBattleBgm = new QLineEdit(sys);
    mTitleBgm = new QLineEdit(sys);
    auto* applySys = new QPushButton("System uebernehmen", sys);
    connect(applySys, &QPushButton::clicked, this, &QtDatabaseEditorDock::onSystemApply);
    form->addRow("Spieltitel", mGameTitle);
    form->addRow("Waehrung", mCurrency);
    form->addRow("Start-Map-ID", mStartMap);
    form->addRow("Start X", mStartX);
    form->addRow("Start Y", mStartY);
    form->addRow("Battle BGM", mBattleBgm);
    form->addRow("Title BGM", mTitleBgm);
    form->addRow(applySys);
    mTabs->addTab(sys, "System");

    root->addWidget(mTabs, 1);
    root->addWidget(new QLabel(
        "Doppelklick in Zelle zum Editieren. Speichern schreibt database/*.json.", this));
}

void QtDatabaseEditorDock::refresh() {
    // Die Models binden an den globalen Database-Singleton (auch vor der
    // Engine-Init gueltig). Trotzdem einheitlich guarden, damit engineReady()
    // die Projekt-Daten nach der Initialisierung sauber nachlaedt.
    if (!mEngine || !mEngine->IsInitialized()) return;
    if (mActorsModel) mActorsModel->reload();
    if (mItemsModel) mItemsModel->reload();
    if (mEnemiesModel) mEnemiesModel->reload();
    if (mSkillsModel) mSkillsModel->reload();
    if (mWeaponsModel) mWeaponsModel->reload();
    if (mClassesModel) mClassesModel->reload();
    syncSystemForm();
}

void QtDatabaseEditorDock::syncSystemForm() {
    auto& s = rpg::Database::Get().System();
    mGameTitle->setText(QString::fromStdString(s.gameTitle));
    mCurrency->setText(QString::fromStdString(s.currencyUnit));
    mStartMap->setValue(s.startMapId);
    mStartX->setValue(s.startX);
    mStartY->setValue(s.startY);
    mBattleBgm->setText(QString::fromStdString(s.battleBgm));
    mTitleBgm->setText(QString::fromStdString(s.titleBgm));
}

void QtDatabaseEditorDock::onSystemApply() {
    auto& s = rpg::Database::Get().System();
    s.gameTitle = mGameTitle->text().toStdString();
    s.currencyUnit = mCurrency->text().toStdString();
    s.startMapId = mStartMap->value();
    s.startX = mStartX->value();
    s.startY = mStartY->value();
    s.battleBgm = mBattleBgm->text().toStdString();
    s.titleBgm = mTitleBgm->text().toStdString();
    emit logMessage("System-Daten uebernommen.");
}

void QtDatabaseEditorDock::onSave() {
    if (!mEngine) return;
    onSystemApply();
    const std::string pp = mEngine->GetProject().GetProjectPath();
    if (pp.empty()) {
        QMessageBox::information(this, "Speichern", "Kein Projekt geladen.");
        return;
    }
    if (rpg::Database::Get().Save(pp))
        emit logMessage("Database gespeichert: " + QString::fromStdString(pp) + "/database/");
    else
        emit logMessage("FEHLER: Database speichern fehlgeschlagen.");
}

void QtDatabaseEditorDock::onReload() {
    if (!mEngine) return;
    const std::string pp = mEngine->GetProject().GetProjectPath();
    if (pp.empty()) {
        rpg::Database::Get().CreateDefaults();
    } else {
        rpg::Database::Get().Load(pp);
    }
    refresh();
    emit logMessage("Database neu geladen.");
}

void QtDatabaseEditorDock::onAddRow() {
    const int tab = mTabs->currentIndex();
    QAbstractTableModel* model = nullptr;
    if (tab == 0) model = mActorsModel;
    else if (tab == 1) model = mItemsModel;
    else if (tab == 2) model = mEnemiesModel;
    else if (tab == 3) model = mSkillsModel;
    else if (tab == 4) model = mWeaponsModel;
    else if (tab == 5) model = mClassesModel;
    if (model) model->insertRows(model->rowCount(), 1);
}

void QtDatabaseEditorDock::onRemoveRow() {
    const int tab = mTabs->currentIndex();
    QTableView* view = nullptr;
    QAbstractTableModel* model = nullptr;
    if (tab == 0) { view = mActorsView; model = mActorsModel; }
    else if (tab == 1) { view = mItemsView; model = mItemsModel; }
    else if (tab == 2) { view = mEnemiesView; model = mEnemiesModel; }
    else if (tab == 3) { view = mSkillsView; model = mSkillsModel; }
    else if (tab == 4) { view = mWeaponsView; model = mWeaponsModel; }
    else if (tab == 5) { view = mClassesView; model = mClassesModel; }
    else return;
    const auto rows = view->selectionModel()->selectedRows();
    if (rows.isEmpty()) return;
    // von hinten loeschen
    QList<int> sorted;
    for (const auto& idx : rows) sorted.push_back(idx.row());
    std::sort(sorted.begin(), sorted.end(), std::greater<int>());
    for (int r : sorted) model->removeRows(r, 1);
}

} // namespace qt_editor
