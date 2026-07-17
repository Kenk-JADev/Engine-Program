#include "QtDatabaseDialog.h"

#include "QtEventEditorDialog.h"

#include "rpgmaker3d/Engine.h"
#include "rpgmaker3d/Project.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QInputDialog>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMessageBox>
#include <QPushButton>
#include <QScrollArea>
#include <QSpinBox>
#include <QTabWidget>
#include <QVariant>
#include <QVBoxLayout>

namespace qt_editor {

#ifndef QL
#define QL(x) QStringLiteral(x)
#endif

namespace {

QSpinBox* makeSpin(int mn, int mx, int value, QWidget* parent) {
    auto* s = new QSpinBox(parent);
    s->setRange(mn, mx);
    s->setValue(value);
    return s;
}
QDoubleSpinBox* makeDSpin(double mn, double mx, double value, QWidget* parent) {
    auto* s = new QDoubleSpinBox(parent);
    s->setRange(mn, mx);
    s->setSingleStep(0.05);
    s->setDecimals(2);
    s->setValue(value);
    return s;
}
QLineEdit* makeLine(QWidget* parent, const QString& text = QString()) {
    auto* e = new QLineEdit(text, parent);
    return e;
}
QComboBox* makeCombo(QWidget* parent, const QStringList& items, int current) {
    auto* c = new QComboBox(parent);
    c->addItems(items);
    if (current >= 0 && current < items.size()) c->setCurrentIndex(current);
    return c;
}
QString IdName(int id, const QString& name) {
    return name.isEmpty() ? QString::number(id, 10).rightJustified(3, QLatin1Char('0'))
                          : QL("%1: %2").arg(id, 3, 10, QLatin1Char('0')).arg(name);
}

} // namespace

// ---------------------------------------------------------------------------
// Konstruktor / Rahmen
// ---------------------------------------------------------------------------

QtDatabaseDialog::QtDatabaseDialog(rpg::Engine* engine, QWidget* parent)
    : QDialog(parent), mEngine(engine) {
    setModal(true);
    setWindowTitle(QL("Datenbank"));
    resize(1100, 660);

    // Arbeitskopien aus der echten Datenbank ziehen
    auto& db = rpg::Database::Get();
    mActors = db.Actors();
    mClasses = db.Classes();
    mItems = db.Items();
    mWeapons = db.Weapons();
    mArmors = db.Armors();
    mSkills = db.Skills();
    mEnemies = db.Enemies();
    mTroops = db.Troops();
    mStates = db.States();
    mTilesets = db.Tilesets();
    mSystem = db.System();
    mCEs = rpg::EventSystem::Get().GetCommonEvents();

    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(6, 6, 6, 6);

    mTabs = new QTabWidget(this);
    mTabs->setUsesScrollButtons(true);
    root->addWidget(mTabs, 1);

    buildActorsTab();
    buildClassesTab();
    buildSkillsTab();
    buildItemsTab();
    buildWeaponsTab();
    buildArmorsTab();
    buildEnemiesTab();
    buildTroopsTab();
    buildStatesTab();
    buildAnimationsTab();
    buildTilesetsTab();
    buildCommonEventsTab();
    buildSystemTab();

    connect(mTabs, &QTabWidget::currentChanged, this, [this](int) {
        // Beim Tabwechsel alles sichern, damit kein Edit verloren geht
        storeAll();
    });

    mButtons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel
                                    | QDialogButtonBox::Apply, this);
    mButtons->button(QDialogButtonBox::Ok)->setText(QL("OK"));
    mButtons->button(QDialogButtonBox::Cancel)->setText(QL("Abbrechen"));
    mButtons->button(QDialogButtonBox::Apply)->setText(QL("Anwenden"));
    connect(mButtons->button(QDialogButtonBox::Ok), &QPushButton::clicked,
            this, &QtDatabaseDialog::onOk);
    connect(mButtons->button(QDialogButtonBox::Cancel), &QPushButton::clicked,
            this, &QDialog::reject);
    connect(mButtons->button(QDialogButtonBox::Apply), &QPushButton::clicked,
            this, &QtDatabaseDialog::onApply);
    root->addWidget(mButtons);
}

// ---------------------------------------------------------------------------
// Listen-Tab-Grundgerüst
// ---------------------------------------------------------------------------

QtDatabaseDialog::ListTab& QtDatabaseDialog::addListTab(const QString& title) {
    auto tab = std::make_unique<ListTab>();
    ListTab& t = *tab;

    t.page = new QWidget(mTabs);
    auto* lay = new QHBoxLayout(t.page);
    lay->setContentsMargins(8, 8, 8, 8);

    // links: Liste + Maximum-Button
    auto* leftBox = new QVBoxLayout();
    t.list = new QListWidget(t.page);
    t.list->setMinimumWidth(170);
    t.list->setMaximumWidth(230);
    leftBox->addWidget(t.list, 1);
    auto* maxBtn = new QPushButton(QL("Maximum ändern ..."), t.page);
    leftBox->addWidget(maxBtn);
    lay->addLayout(leftBox);

    // rechts: Formular-Platzhalter (vom Tab-Bauer gefüllt)
    auto* formWrap = new QScrollArea(t.page);
    formWrap->setWidgetResizable(true);
    formWrap->setFrameShape(QFrame::NoFrame);
    lay->addWidget(formWrap, 1);
    t.page->setProperty("formWrap", QVariant::fromValue((QWidget*)formWrap));

    mTabs->addTab(t.page, title);

    auto* self = this;
    ListTab* tp = &t;
    connect(t.list, &QListWidget::currentRowChanged, t.list, [self, tp](int row) {
        if (tp->loading) return;
        if (row == tp->current) return;
        if (tp->current >= 0 && tp->storeForm) tp->storeForm(tp->current);
        tp->current = row;
        if (row >= 0 && tp->loadForm) tp->loadForm(row);
    });
    connect(maxBtn, &QPushButton::clicked, maxBtn, [self, tp]() {
        if (!tp->count) return;
        bool ok = false;
        int n = QInputDialog::getInt(self, QL("Maximum ändern"),
                                     QL("Neues Maximum (1 - 999):"),
                                     tp->count(), 1, 999, 1, &ok);
        if (ok && tp->setMax) {
            tp->setMax(n);
            self->rebuildList(*tp, qMin(tp->current, n - 1));
        }
    });

    mListTabs.push_back(std::move(tab));
    return t;
}

void QtDatabaseDialog::rebuildList(ListTab& tab, int select) {
    tab.loading++;
    tab.list->clear();
    const int n = tab.count ? tab.count() : 0;
    for (int i = 0; i < n; ++i)
        tab.list->addItem(tab.nameAt(i));
    tab.loading--;
    if (n == 0) {
        tab.current = -1;
        return;
    }
    if (select < 0 || select >= n) select = 0;
    tab.list->setCurrentRow(select);
    if (tab.current != select) {
        tab.current = select;
        loadCurrent(tab);
    } else {
        loadCurrent(tab);
    }
}

void QtDatabaseDialog::loadCurrent(ListTab& tab) {
    if (tab.current >= 0 && tab.loadForm) {
        tab.loading++;
        tab.loadForm(tab.current);
        tab.loading--;
    }
}

void QtDatabaseDialog::storeCurrent(ListTab& tab) {
    if (tab.current >= 0 && tab.storeForm)
        tab.storeForm(tab.current);
}

void QtDatabaseDialog::storeAll() {
    for (auto& t : mListTabs) storeCurrent(*t);
    for (auto& fn : mExtraStore) if (fn) fn();
}

// ---------------------------------------------------------------------------
// Tabs
// ---------------------------------------------------------------------------

void QtDatabaseDialog::buildActorsTab() {
    ListTab& t = addListTab(QL("Akteure"));
    ListTab* tp = &t;

    auto* formHost = new QWidget(t.page);
    auto* form = new QFormLayout(formHost);
    auto* scroll = t.page->property("formWrap").value<QWidget*>();
    ((QScrollArea*)scroll)->setWidget(formHost);

    auto* name = makeLine(formHost);
    auto* klass = makeCombo(formHost, {}, 0);
    auto* initLv = makeSpin(1, 99, 1, formHost);
    auto* maxLv = makeSpin(1, 99, 99, formHost);
    auto* charName = makeLine(formHost);
    auto* faceName = makeLine(formHost);
    auto* battlerName = makeLine(formHost);
    auto* mhp = makeSpin(1, 99999, 100, formHost);
    auto* mmp = makeSpin(0, 99999, 30, formHost);
    auto* atk = makeSpin(0, 999, 10, formHost);
    auto* def = makeSpin(0, 999, 10, formHost);
    auto* mat = makeSpin(0, 999, 5, formHost);
    auto* mdf = makeSpin(0, 999, 5, formHost);
    auto* agi = makeSpin(0, 999, 10, formHost);
    auto* luk = makeSpin(0, 999, 10, formHost);

    form->addRow(QL("Name"), name);
    form->addRow(QL("Klasse"), klass);
    form->addRow(QL("Anfangs-Level"), initLv);
    form->addRow(QL("Max-Level"), maxLv);
    form->addRow(QL("Charakter-Grafik"), charName);
    form->addRow(QL("Face-Grafik"), faceName);
    form->addRow(QL("Battler-Grafik"), battlerName);
    form->addRow(QL("Max. HP"), mhp);
    form->addRow(QL("Max. MP"), mmp);
    form->addRow(QL("Angriff"), atk);
    form->addRow(QL("Abwehr"), def);
    form->addRow(QL("Magie"), mat);
    form->addRow(QL("Magieabwehr"), mdf);
    form->addRow(QL("Agilität"), agi);
    form->addRow(QL("Glück"), luk);

    tp->count = [this]() { return (int)mActors.size(); };
    tp->nameAt = [this](int i) {
        return IdName(i + 1, QString::fromStdString(mActors[(size_t)i].name));
    };
    tp->setMax = [this](int n) {
        mActors.resize((size_t)n);
        for (size_t i = 0; i < mActors.size(); ++i) mActors[i].id = (int)i + 1;
    };
    tp->loadForm = [this, tp, name, klass, initLv, maxLv, charName, faceName,
                    battlerName, mhp, mmp, atk, def, mat, mdf, agi, luk](int i) {
        auto& a = mActors[(size_t)i];
        name->setText(QString::fromStdString(a.name));
        klass->clear();
        for (const auto& c : mClasses)
            klass->addItem(QString::fromStdString(c.name));
        int ci = klass->findText(QString::fromStdString(a.className));
        klass->setCurrentIndex(ci >= 0 ? ci : 0);
        initLv->setValue(a.initialLevel);
        maxLv->setValue(a.maxLevel);
        charName->setText(QString::fromStdString(a.characterName));
        faceName->setText(QString::fromStdString(a.faceName));
        battlerName->setText(QString::fromStdString(a.battlerName));
        mhp->setValue(a.initialStats.mhp);
        mmp->setValue(a.initialStats.mmp);
        atk->setValue(a.initialStats.atk);
        def->setValue(a.initialStats.def);
        mat->setValue(a.initialStats.mat);
        mdf->setValue(a.initialStats.mdf);
        agi->setValue(a.initialStats.agi);
        luk->setValue(a.initialStats.luk);
    };
    tp->storeForm = [this, tp, name, klass, initLv, maxLv, charName, faceName,
                     battlerName, mhp, mmp, atk, def, mat, mdf, agi, luk](int i) {
        if ((size_t)i >= mActors.size()) return;
        auto& a = mActors[(size_t)i];
        a.name = name->text().toStdString();
        a.className = klass->currentText().toStdString();
        a.initialLevel = initLv->value();
        a.maxLevel = maxLv->value();
        a.characterName = charName->text().toStdString();
        a.faceName = faceName->text().toStdString();
        a.battlerName = battlerName->text().toStdString();
        a.initialStats.mhp = mhp->value();
        a.initialStats.mmp = mmp->value();
        a.initialStats.atk = atk->value();
        a.initialStats.def = def->value();
        a.initialStats.mat = mat->value();
        a.initialStats.mdf = mdf->value();
        a.initialStats.agi = agi->value();
        a.initialStats.luk = luk->value();
        // Listenzeile aktualisieren falls Namens-Edit
        if (!tp->loading && tp->list)
            tp->list->item(i)->setText(tp->nameAt(i));
    };

    rebuildList(t, 0);
}

void QtDatabaseDialog::buildClassesTab() {
    ListTab& t = addListTab(QL("Klassen"));
    ListTab* tp = &t;
    auto* formHost = new QWidget(t.page);
    auto* form = new QFormLayout(formHost);
    ((QScrollArea*)t.page->property("formWrap").value<QWidget*>())->setWidget(formHost);

    auto* name = makeLine(formHost);
    auto* base = makeSpin(0, 9999, 30, formHost);
    auto* extra = makeSpin(0, 9999, 20, formHost);
    auto* accA = makeDSpin(0, 999, 30, formHost);
    auto* accB = makeDSpin(0, 999, 20, formHost);
    form->addRow(QL("Name"), name);
    form->addRow(QL("EXP-Basis"), base);
    form->addRow(QL("EXP-Zuschlag"), extra);
    form->addRow(QL("EXP-Beschleunigung A"), accA);
    form->addRow(QL("EXP-Beschleunigung B"), accB);

    tp->count = [this]() { return (int)mClasses.size(); };
    tp->nameAt = [this](int i) {
        return IdName(i + 1, QString::fromStdString(mClasses[(size_t)i].name));
    };
    tp->setMax = [this](int n) {
        mClasses.resize((size_t)n);
        for (size_t i = 0; i < mClasses.size(); ++i) mClasses[i].id = (int)i + 1;
    };
    tp->loadForm = [this, name, base, extra, accA, accB](int i) {
        auto& c = mClasses[(size_t)i];
        name->setText(QString::fromStdString(c.name));
        base->setValue(c.expBase);
        extra->setValue(c.expExtra);
        accA->setValue((double)c.expAccA);
        accB->setValue((double)c.expAccB);
    };
    tp->storeForm = [this, tp, name, base, extra, accA, accB](int i) {
        if ((size_t)i >= mClasses.size()) return;
        auto& c = mClasses[(size_t)i];
        c.name = name->text().toStdString();
        c.expBase = base->value();
        c.expExtra = extra->value();
        c.expAccA = (float)accA->value();
        c.expAccB = (float)accB->value();
        if (!tp->loading && tp->list) tp->list->item(i)->setText(tp->nameAt(i));
    };
    rebuildList(t, 0);
}

void QtDatabaseDialog::buildSkillsTab() {
    ListTab& t = addListTab(QL("Fertigkeiten"));
    ListTab* tp = &t;
    auto* formHost = new QWidget(t.page);
    auto* form = new QFormLayout(formHost);
    ((QScrollArea*)t.page->property("formWrap").value<QWidget*>())->setWidget(formHost);

    auto* name = makeLine(formHost);
    auto* desc = makeLine(formHost);
    auto* cost = makeSpin(0, 999, 10, formHost);
    auto* scope = makeCombo(formHost, {QL("Kein Ziel"), QL("Ein Gegner"), QL("Alle Gegner"),
                                       QL("Ein Verbündeter"), QL("Alle Verbündeten"),
                                       QL("Anwender")}, 1);
    auto* power = makeSpin(0, 9999, 100, formHost);
    auto* anim = makeLine(formHost);
    form->addRow(QL("Name"), name);
    form->addRow(QL("Beschreibung"), desc);
    form->addRow(QL("MP-Kosten"), cost);
    form->addRow(QL("Reichweite"), scope);
    form->addRow(QL("Stärke"), power);
    form->addRow(QL("Animation"), anim);

    tp->count = [this]() { return (int)mSkills.size(); };
    tp->nameAt = [this](int i) {
        return IdName(i + 1, QString::fromStdString(mSkills[(size_t)i].name));
    };
    tp->setMax = [this](int n) {
        mSkills.resize((size_t)n);
        for (size_t i = 0; i < mSkills.size(); ++i) mSkills[i].id = (int)i + 1;
    };
    tp->loadForm = [this, name, desc, cost, scope, power, anim](int i) {
        auto& s = mSkills[(size_t)i];
        name->setText(QString::fromStdString(s.name));
        desc->setText(QString::fromStdString(s.description));
        cost->setValue(s.mpCost);
        scope->setCurrentIndex(qBound(0, s.scope, 5));
        power->setValue(s.power);
        anim->setText(QString::fromStdString(s.animation));
    };
    tp->storeForm = [this, tp, name, desc, cost, scope, power, anim](int i) {
        if ((size_t)i >= mSkills.size()) return;
        auto& s = mSkills[(size_t)i];
        s.name = name->text().toStdString();
        s.description = desc->text().toStdString();
        s.mpCost = cost->value();
        s.scope = scope->currentIndex();
        s.power = power->value();
        s.animation = anim->text().toStdString();
        if (!tp->loading && tp->list) tp->list->item(i)->setText(tp->nameAt(i));
    };
    rebuildList(t, 0);
}

void QtDatabaseDialog::buildItemsTab() {
    ListTab& t = addListTab(QL("Gegenstände"));
    ListTab* tp = &t;
    auto* formHost = new QWidget(t.page);
    auto* form = new QFormLayout(formHost);
    ((QScrollArea*)t.page->property("formWrap").value<QWidget*>())->setWidget(formHost);

    auto* name = makeLine(formHost);
    auto* desc = makeLine(formHost);
    auto* price = makeSpin(0, 999999, 50, formHost);
    auto* type = makeCombo(formHost, {QL("Normal"), QL("Schlüssel"), QL("Versteckt A"),
                                      QL("Versteckt B")}, 0);
    auto* consumable = new QCheckBox(QL("Verbrauchbar"), formHost);
    consumable->setChecked(true);
    auto* scope = makeCombo(formHost, {QL("Kein Ziel"), QL("Ein Gegner"), QL("Alle Gegner"),
                                       QL("Ein Verbündeter"), QL("Alle Verbündeten"),
                                       QL("Verbündeter (tot)"), QL("Alle (tot)"),
                                       QL("Anwender")}, 3);
    auto* hpRec = makeSpin(0, 99999, 100, formHost);
    auto* mpRec = makeSpin(0, 99999, 0, formHost);
    form->addRow(QL("Name"), name);
    form->addRow(QL("Beschreibung"), desc);
    form->addRow(QL("Preis"), price);
    form->addRow(QL("Typ"), type);
    form->addRow(consumable);
    form->addRow(QL("Reichweite"), scope);
    form->addRow(QL("HP-Genesung"), hpRec);
    form->addRow(QL("MP-Genesung"), mpRec);

    tp->count = [this]() { return (int)mItems.size(); };
    tp->nameAt = [this](int i) {
        return IdName(i + 1, QString::fromStdString(mItems[(size_t)i].name));
    };
    tp->setMax = [this](int n) {
        mItems.resize((size_t)n);
        for (size_t i = 0; i < mItems.size(); ++i) mItems[i].id = (int)i + 1;
    };
    tp->loadForm = [this, name, desc, price, type, consumable, scope, hpRec, mpRec](int i) {
        auto& it = mItems[(size_t)i];
        name->setText(QString::fromStdString(it.name));
        desc->setText(QString::fromStdString(it.description));
        price->setValue(it.price);
        type->setCurrentIndex((int)it.itemType);
        consumable->setChecked(it.consumable);
        scope->setCurrentIndex(qBound(0, (int)it.scope, 7));
        hpRec->setValue(it.hpRecovery);
        mpRec->setValue(it.mpRecovery);
    };
    tp->storeForm = [this, tp, name, desc, price, type, consumable, scope,
                     hpRec, mpRec](int i) {
        if ((size_t)i >= mItems.size()) return;
        auto& it = mItems[(size_t)i];
        it.name = name->text().toStdString();
        it.description = desc->text().toStdString();
        it.price = price->value();
        it.itemType = (rpg::ItemData::Type)type->currentIndex();
        it.consumable = consumable->isChecked();
        it.scope = (rpg::ItemData::Scope)scope->currentIndex();
        it.hpRecovery = hpRec->value();
        it.mpRecovery = mpRec->value();
        if (!tp->loading && tp->list) tp->list->item(i)->setText(tp->nameAt(i));
    };
    rebuildList(t, 0);
}

void QtDatabaseDialog::buildWeaponsTab() {
    ListTab& t = addListTab(QL("Waffen"));
    ListTab* tp = &t;
    auto* formHost = new QWidget(t.page);
    auto* form = new QFormLayout(formHost);
    ((QScrollArea*)t.page->property("formWrap").value<QWidget*>())->setWidget(formHost);

    auto* name = makeLine(formHost);
    auto* desc = makeLine(formHost);
    auto* price = makeSpin(0, 999999, 100, formHost);
    auto* atk = makeSpin(0, 999, 10, formHost);
    auto* animId = makeSpin(0, 999, 0, formHost);
    form->addRow(QL("Name"), name);
    form->addRow(QL("Beschreibung"), desc);
    form->addRow(QL("Preis"), price);
    form->addRow(QL("Angriff"), atk);
    form->addRow(QL("Animations-ID"), animId);

    tp->count = [this]() { return (int)mWeapons.size(); };
    tp->nameAt = [this](int i) {
        return IdName(i + 1, QString::fromStdString(mWeapons[(size_t)i].name));
    };
    tp->setMax = [this](int n) {
        mWeapons.resize((size_t)n);
        for (size_t i = 0; i < mWeapons.size(); ++i) mWeapons[i].id = (int)i + 1;
    };
    tp->loadForm = [this, name, desc, price, atk, animId](int i) {
        auto& w = mWeapons[(size_t)i];
        name->setText(QString::fromStdString(w.name));
        desc->setText(QString::fromStdString(w.description));
        price->setValue(w.price);
        atk->setValue(w.atk);
        animId->setValue(w.animationId);
    };
    tp->storeForm = [this, tp, name, desc, price, atk, animId](int i) {
        if ((size_t)i >= mWeapons.size()) return;
        auto& w = mWeapons[(size_t)i];
        w.name = name->text().toStdString();
        w.description = desc->text().toStdString();
        w.price = price->value();
        w.atk = atk->value();
        w.animationId = animId->value();
        if (!tp->loading && tp->list) tp->list->item(i)->setText(tp->nameAt(i));
    };
    rebuildList(t, 0);
}

void QtDatabaseDialog::buildArmorsTab() {
    ListTab& t = addListTab(QL("Rüstungen"));
    ListTab* tp = &t;
    auto* formHost = new QWidget(t.page);
    auto* form = new QFormLayout(formHost);
    ((QScrollArea*)t.page->property("formWrap").value<QWidget*>())->setWidget(formHost);

    auto* name = makeLine(formHost);
    auto* desc = makeLine(formHost);
    auto* price = makeSpin(0, 999999, 100, formHost);
    auto* def = makeSpin(0, 999, 10, formHost);
    auto* mdf = makeSpin(0, 999, 5, formHost);
    auto* type = makeCombo(formHost, {QL("Schild"), QL("Helm"), QL("Körper"),
                                      QL("Accessoire")}, 0);
    form->addRow(QL("Name"), name);
    form->addRow(QL("Beschreibung"), desc);
    form->addRow(QL("Preis"), price);
    form->addRow(QL("Abwehr"), def);
    form->addRow(QL("Magieabwehr"), mdf);
    form->addRow(QL("Rüstungsart"), type);

    tp->count = [this]() { return (int)mArmors.size(); };
    tp->nameAt = [this](int i) {
        return IdName(i + 1, QString::fromStdString(mArmors[(size_t)i].name));
    };
    tp->setMax = [this](int n) {
        mArmors.resize((size_t)n);
        for (size_t i = 0; i < mArmors.size(); ++i) mArmors[i].id = (int)i + 1;
    };
    tp->loadForm = [this, name, desc, price, def, mdf, type](int i) {
        auto& a = mArmors[(size_t)i];
        name->setText(QString::fromStdString(a.name));
        desc->setText(QString::fromStdString(a.description));
        price->setValue(a.price);
        def->setValue(a.def);
        mdf->setValue(a.mdf);
        type->setCurrentIndex((int)a.armorType);
    };
    tp->storeForm = [this, tp, name, desc, price, def, mdf, type](int i) {
        if ((size_t)i >= mArmors.size()) return;
        auto& a = mArmors[(size_t)i];
        a.name = name->text().toStdString();
        a.description = desc->text().toStdString();
        a.price = price->value();
        a.def = def->value();
        a.mdf = mdf->value();
        a.armorType = (rpg::ArmorData::Type)type->currentIndex();
        if (!tp->loading && tp->list) tp->list->item(i)->setText(tp->nameAt(i));
    };
    rebuildList(t, 0);
}

void QtDatabaseDialog::buildEnemiesTab() {
    ListTab& t = addListTab(QL("Gegner"));
    ListTab* tp = &t;
    auto* formHost = new QWidget(t.page);
    auto* form = new QFormLayout(formHost);
    ((QScrollArea*)t.page->property("formWrap").value<QWidget*>())->setWidget(formHost);

    auto* name = makeLine(formHost);
    auto* battler = makeLine(formHost);
    auto* mhp = makeSpin(1, 999999, 100, formHost);
    auto* mmp = makeSpin(0, 99999, 10, formHost);
    auto* atk = makeSpin(0, 999, 15, formHost);
    auto* def = makeSpin(0, 999, 5, formHost);
    auto* mat = makeSpin(0, 999, 5, formHost);
    auto* mdf = makeSpin(0, 999, 5, formHost);
    auto* agi = makeSpin(0, 999, 8, formHost);
    auto* luk = makeSpin(0, 999, 5, formHost);
    auto* exp = makeSpin(0, 999999, 10, formHost);
    auto* gold = makeSpin(0, 999999, 5, formHost);
    auto* drops = makeLine(formHost);
    drops->setPlaceholderText(QL("Gegenstands-IDs, Komma-getrennt (z.B. 1,2)"));

    form->addRow(QL("Name"), name);
    form->addRow(QL("Battler-Grafik"), battler);
    form->addRow(QL("Max. HP"), mhp);
    form->addRow(QL("Max. MP"), mmp);
    form->addRow(QL("Angriff"), atk);
    form->addRow(QL("Abwehr"), def);
    form->addRow(QL("Magie"), mat);
    form->addRow(QL("Magieabwehr"), mdf);
    form->addRow(QL("Agilität"), agi);
    form->addRow(QL("Glück"), luk);
    form->addRow(QL("EXP"), exp);
    form->addRow(QL("Gold"), gold);
    form->addRow(QL("Beute"), drops);

    tp->count = [this]() { return (int)mEnemies.size(); };
    tp->nameAt = [this](int i) {
        return IdName(i + 1, QString::fromStdString(mEnemies[(size_t)i].name));
    };
    tp->setMax = [this](int n) {
        mEnemies.resize((size_t)n);
        for (size_t i = 0; i < mEnemies.size(); ++i) mEnemies[i].id = (int)i + 1;
    };
    tp->loadForm = [this, name, battler, mhp, mmp, atk, def, mat, mdf, agi, luk,
                    exp, gold, drops](int i) {
        auto& e = mEnemies[(size_t)i];
        name->setText(QString::fromStdString(e.name));
        battler->setText(QString::fromStdString(e.battlerName));
        mhp->setValue(e.maxHp);
        mmp->setValue(e.maxMp);
        atk->setValue(e.atk);
        def->setValue(e.def);
        mat->setValue(e.mat);
        mdf->setValue(e.mdf);
        agi->setValue(e.agi);
        luk->setValue(e.luk);
        exp->setValue(e.exp);
        gold->setValue(e.gold);
        QStringList ids;
        for (int d : e.dropItems) ids << QString::number(d);
        drops->setText(ids.join(QLatin1Char(',')));
    };
    tp->storeForm = [this, tp, name, battler, mhp, mmp, atk, def, mat, mdf, agi,
                     luk, exp, gold, drops](int i) {
        if ((size_t)i >= mEnemies.size()) return;
        auto& e = mEnemies[(size_t)i];
        e.name = name->text().toStdString();
        e.battlerName = battler->text().toStdString();
        e.maxHp = mhp->value();
        e.maxMp = mmp->value();
        e.atk = atk->value();
        e.def = def->value();
        e.mat = mat->value();
        e.mdf = mdf->value();
        e.agi = agi->value();
        e.luk = luk->value();
        e.exp = exp->value();
        e.gold = gold->value();
        e.dropItems.clear();
        const QStringList parts = drops->text().split(QLatin1Char(','), Qt::SkipEmptyParts);
        for (const QString& p : parts) {
            bool ok = false;
            int v = p.trimmed().toInt(&ok);
            if (ok && v > 0) e.dropItems.push_back(v);
        }
        if (!tp->loading && tp->list) tp->list->item(i)->setText(tp->nameAt(i));
    };
    rebuildList(t, 0);
}

void QtDatabaseDialog::buildTroopsTab() {
    ListTab& t = addListTab(QL("Trupps"));
    ListTab* tp = &t;
    auto* formHost = new QWidget(t.page);
    auto* form = new QFormLayout(formHost);
    ((QScrollArea*)t.page->property("formWrap").value<QWidget*>())->setWidget(formHost);

    auto* name = makeLine(formHost);
    auto* memberList = new QListWidget(formHost);
    memberList->setMaximumHeight(140);
    auto* addRow = new QWidget(formHost);
    auto* addLay = new QHBoxLayout(addRow);
    addLay->setContentsMargins(0, 0, 0, 0);
    auto* enemyCombo = new QComboBox(addRow);
    auto* addBtn = new QPushButton(QL("Hinzufügen"), addRow);
    auto* remBtn = new QPushButton(QL("Entfernen"), addRow);
    addLay->addWidget(enemyCombo, 1);
    addLay->addWidget(addBtn);
    addLay->addWidget(remBtn);

    form->addRow(QL("Name"), name);
    form->addRow(QL("Mitglieder"), memberList);
    form->addRow(addRow);

    tp->count = [this]() { return (int)mTroops.size(); };
    tp->nameAt = [this](int i) {
        return IdName(i + 1, QString::fromStdString(mTroops[(size_t)i].name));
    };
    tp->setMax = [this](int n) {
        mTroops.resize((size_t)n);
        for (size_t i = 0; i < mTroops.size(); ++i) mTroops[i].id = (int)i + 1;
    };
    tp->loadForm = [this, name, memberList, enemyCombo](int i) {
        auto& tr = mTroops[(size_t)i];
        name->setText(QString::fromStdString(tr.name));
        memberList->clear();
        for (int eid : tr.members) {
            QString en = QL("? %1").arg(eid);
            if ((size_t)eid <= mEnemies.size() && eid > 0)
                en = QString::fromStdString(mEnemies[(size_t)(eid - 1)].name);
            auto* item = new QListWidgetItem(QL("%1: %2").arg(eid, 3, 10, QLatin1Char('0')).arg(en));
            item->setData(Qt::UserRole, eid);
            memberList->addItem(item);
        }
        enemyCombo->clear();
        for (const auto& e : mEnemies)
            enemyCombo->addItem(QL("%1: %2").arg(e.id, 3, 10, QLatin1Char('0'))
                                    .arg(QString::fromStdString(e.name)), e.id);
    };
    tp->storeForm = [this, tp, name, memberList](int i) {
        if ((size_t)i >= mTroops.size()) return;
        auto& tr = mTroops[(size_t)i];
        tr.name = name->text().toStdString();
        tr.members.clear();
        for (int r = 0; r < memberList->count(); ++r)
            tr.members.push_back(memberList->item(r)->data(Qt::UserRole).toInt());
        if (!tp->loading && tp->list) tp->list->item(i)->setText(tp->nameAt(i));
    };

    connect(addBtn, &QPushButton::clicked, formHost, [memberList, enemyCombo, this]() {
        const int eid = enemyCombo->currentData().toInt();
        if (eid <= 0) return;
        QString en = QL("? %1").arg(eid);
        if ((size_t)eid <= mEnemies.size())
            en = QString::fromStdString(mEnemies[(size_t)(eid - 1)].name);
        auto* item = new QListWidgetItem(QL("%1: %2").arg(eid, 3, 10, QLatin1Char('0')).arg(en));
        item->setData(Qt::UserRole, eid);
        memberList->addItem(item);
    });
    connect(remBtn, &QPushButton::clicked, formHost, [memberList]() {
        delete memberList->takeItem(memberList->currentRow());
    });

    rebuildList(t, 0);
}

void QtDatabaseDialog::buildStatesTab() {
    ListTab& t = addListTab(QL("Status"));
    ListTab* tp = &t;
    auto* formHost = new QWidget(t.page);
    auto* form = new QFormLayout(formHost);
    ((QScrollArea*)t.page->property("formWrap").value<QWidget*>())->setWidget(formHost);

    auto* name = makeLine(formHost);
    auto* restr = makeCombo(formHost, {QL("Keine"), QL("Angriff (Gegner)"),
                                       QL("Angriff (beliebig)"), QL("Angriff (Verbündeter)"),
                                       QL("Kann sich nicht bewegen")}, 0);
    auto* prio = makeSpin(0, 100, 50, formHost);
    auto* removeEnd = new QCheckBox(QL("Nach Kampf entfernen"), formHost);
    removeEnd->setChecked(true);
    auto* timing = makeCombo(formHost, {QL("Nie"), QL("Nach Aktion"), QL("Rundenende")}, 0);
    auto* hold = makeSpin(0, 99, 0, formHost);
    auto* drain = makeDSpin(0, 1, 0, formHost);
    form->addRow(QL("Name"), name);
    form->addRow(QL("Einschränkung"), restr);
    form->addRow(QL("Priorität"), prio);
    form->addRow(removeEnd);
    form->addRow(QL("Auto-Entfernung"), timing);
    form->addRow(QL("Haltezeit (Runden)"), hold);
    form->addRow(QL("HP-Verlustrate/Zug"), drain);

    tp->count = [this]() { return (int)mStates.size(); };
    tp->nameAt = [this](int i) {
        return IdName(i + 1, QString::fromStdString(mStates[(size_t)i].name));
    };
    tp->setMax = [this](int n) {
        mStates.resize((size_t)n);
        for (size_t i = 0; i < mStates.size(); ++i) mStates[i].id = (int)i + 1;
    };
    tp->loadForm = [this, name, restr, prio, removeEnd, timing, hold, drain](int i) {
        auto& s = mStates[(size_t)i];
        name->setText(QString::fromStdString(s.name));
        restr->setCurrentIndex(qBound(0, s.restriction, 4));
        prio->setValue(s.priority);
        removeEnd->setChecked(s.removeAtBattleEnd);
        timing->setCurrentIndex(qBound(0, s.autoRemovalTiming, 2));
        hold->setValue(s.holdTurn);
        drain->setValue((double)s.hpDrainRate);
    };
    tp->storeForm = [this, tp, name, restr, prio, removeEnd, timing, hold, drain](int i) {
        if ((size_t)i >= mStates.size()) return;
        auto& s = mStates[(size_t)i];
        s.name = name->text().toStdString();
        s.restriction = restr->currentIndex();
        s.priority = prio->value();
        s.removeAtBattleEnd = removeEnd->isChecked();
        s.autoRemovalTiming = timing->currentIndex();
        s.holdTurn = hold->value();
        s.hpDrainRate = (float)drain->value();
        if (!tp->loading && tp->list) tp->list->item(i)->setText(tp->nameAt(i));
    };
    rebuildList(t, 0);
}

void QtDatabaseDialog::buildAnimationsTab() {
    ListTab& t = addListTab(QL("Animationen"));
    ListTab* tp = &t;
    auto* formHost = new QWidget(t.page);
    auto* form = new QFormLayout(formHost);
    ((QScrollArea*)t.page->property("formWrap").value<QWidget*>())->setWidget(formHost);

    auto* info = new QLabel(QL("Animationen werden per ID referenziert "
                               "(z.B. Waffen oder der Event-Befehl 'Animation zeigen').\n"
                               "Die Grafik/Datei wird am Befehl angegeben."), formHost);
    info->setWordWrap(true);
    auto* name = makeLine(formHost);
    form->addRow(info);
    form->addRow(QL("Animationsname"), name);

    tp->count = [this]() { return (int)mSystem.animations.size(); };
    tp->nameAt = [this](int i) {
        return IdName(i + 1, QString::fromStdString(mSystem.animations[(size_t)i]));
    };
    tp->setMax = [this](int n) {
        mSystem.animations.resize((size_t)n);
        for (size_t i = 0; i < mSystem.animations.size(); ++i)
            if (mSystem.animations[i].empty())
                mSystem.animations[i] = "Animation " + std::to_string(i + 1);
    };
    tp->loadForm = [this, name](int i) {
        name->setText(QString::fromStdString(mSystem.animations[(size_t)i]));
    };
    tp->storeForm = [this, tp, name](int i) {
        if ((size_t)i >= mSystem.animations.size()) return;
        mSystem.animations[(size_t)i] = name->text().toStdString();
        if (!tp->loading && tp->list) tp->list->item(i)->setText(tp->nameAt(i));
    };
    rebuildList(t, 0);
}

void QtDatabaseDialog::buildTilesetsTab() {
    ListTab& t = addListTab(QL("Tilesets"));
    ListTab* tp = &t;
    auto* formHost = new QWidget(t.page);
    auto* form = new QFormLayout(formHost);
    ((QScrollArea*)t.page->property("formWrap").value<QWidget*>())->setWidget(formHost);

    auto* name = makeLine(formHost);
    auto* file = makeLine(formHost);
    form->addRow(QL("Name"), name);
    form->addRow(QL("Tileset-Grafik"), file);

    tp->count = [this]() { return (int)mTilesets.size(); };
    tp->nameAt = [this](int i) {
        return IdName(i + 1, QString::fromStdString(mTilesets[(size_t)i].name));
    };
    tp->setMax = [this](int n) {
        mTilesets.resize((size_t)n);
        for (size_t i = 0; i < mTilesets.size(); ++i) mTilesets[i].id = (int)i + 1;
    };
    tp->loadForm = [this, name, file](int i) {
        auto& ts = mTilesets[(size_t)i];
        name->setText(QString::fromStdString(ts.name));
        file->setText(QString::fromStdString(ts.tilesetName));
    };
    tp->storeForm = [this, tp, name, file](int i) {
        if ((size_t)i >= mTilesets.size()) return;
        auto& ts = mTilesets[(size_t)i];
        ts.name = name->text().toStdString();
        ts.tilesetName = file->text().toStdString();
        if (!tp->loading && tp->list) tp->list->item(i)->setText(tp->nameAt(i));
    };
    rebuildList(t, 0);
}

void QtDatabaseDialog::buildCommonEventsTab() {
    auto& dbTab = addListTab(QL("Gem. Events"));
    ListTab* tp = &dbTab;

    auto* formHost = new QWidget(dbTab.page);
    auto* form = new QFormLayout(formHost);
    ((QScrollArea*)dbTab.page->property("formWrap").value<QWidget*>())->setWidget(formHost);

    auto* name = makeLine(formHost);
    auto* trigger = makeCombo(formHost, {QL("Kein"), QL("Automatisch"), QL("Parallel")}, 0);
    auto* switchCombo = new QComboBox(formHost);
    for (int i = 1; i <= (int)mSystem.switches.size(); ++i)
        switchCombo->addItem(QL("%1: %2").arg(i, 4, 10, QLatin1Char('0'))
            .arg(QString::fromStdString(mSystem.switches[(size_t)(i - 1)])), i);
    auto* editList = new QPushButton(QL("Befehlsliste bearbeiten ..."), formHost);
    auto* hint = new QLabel(QL("Automatisch/Parallel läuft, wenn der Schalter AN ist."),
                            formHost);
    hint->setWordWrap(true);
    form->addRow(QL("Name"), name);
    form->addRow(QL("Auslöser"), trigger);
    form->addRow(QL("Start-Schalter"), switchCombo);
    form->addRow(editList);
    form->addRow(hint);

    tp->count = [this]() { return (int)mCEs.size(); };
    tp->nameAt = [this](int i) {
        return IdName((int)i + 1, QString::fromStdString(mCEs[(size_t)i].name));
    };
    tp->setMax = [this](int n) {
        mCEs.resize((size_t)n);
        for (size_t i = 0; i < mCEs.size(); ++i)
            if (mCEs[i].id <= 0) mCEs[i].id = (int)i + 1;
    };
    tp->loadForm = [this, name, trigger, switchCombo](int i) {
        auto& ce = mCEs[(size_t)i];
        name->setText(QString::fromStdString(ce.name));
        int ti = 0;
        if (ce.trigger == rpg::EventTrigger::Autorun) ti = 1;
        else if (ce.trigger == rpg::EventTrigger::Parallel) ti = 2;
        trigger->setCurrentIndex(ti);
        int si = switchCombo->findData(ce.switchId > 0 ? ce.switchId : 1);
        switchCombo->setCurrentIndex(si >= 0 ? si : 0);
    };
    tp->storeForm = [this, tp, name, trigger, switchCombo](int i) {
        if ((size_t)i >= mCEs.size()) return;
        auto& ce = mCEs[(size_t)i];
        ce.name = name->text().toStdString();
        ce.id = i + 1;
        switch (trigger->currentIndex()) {
            case 1: ce.trigger = rpg::EventTrigger::Autorun; break;
            case 2: ce.trigger = rpg::EventTrigger::Parallel; break;
            default: ce.trigger = rpg::EventTrigger::None; break;
        }
        ce.switchId = switchCombo->currentData().toInt();
        if (!tp->loading && tp->list) tp->list->item(i)->setText(tp->nameAt(i));
    };

    connect(editList, &QPushButton::clicked, formHost, [this, tp]() {
        storeCurrent(*tp);
        if (tp->current < 0 || (size_t)tp->current >= mCEs.size()) return;
        auto& ce = mCEs[(size_t)tp->current];
        // XP-artig: als Ein-Seiten-MapEvent durch den Event-Dialog schleifen
        rpg::MapEvent wrapper;
        wrapper.id = ce.id;
        wrapper.name = ce.name;
        rpg::EventPage page;
        page.trigger = rpg::EventTrigger::ActionButton;
        page.list = ce.list;
        wrapper.pages.push_back(page);
        if (QtEventEditorDialog::EditEvent(this, wrapper) && !wrapper.pages.empty()) {
            ce.list = wrapper.pages[0].list;
        }
    });

    // CE-Liste mit vorhandenen EventSystem-CEs synchronisieren (IDs lückenfrei)
    rebuildList(dbTab, 0);
}

// ---------------------------------------------------------------------------
// System-Tab (wie XP-Screenshot)
// ---------------------------------------------------------------------------

struct QtDatabaseDialog::Impl {
    QListWidget* partyList = nullptr;
    QListWidget* elementList = nullptr;
    QLineEdit* elementName = nullptr;
    // Grafiken
    QLineEdit* gfxWindowskin = nullptr;
    QLineEdit* gfxTitle = nullptr;
    QLineEdit* gfxGameover = nullptr;
    QLineEdit* gfxBattleTransition = nullptr;
    // BGM / ME / SE
    QLineEdit* bgmTitle = nullptr;
    QLineEdit* bgmBattle = nullptr;
    QLineEdit* meBattleEnd = nullptr;
    QLineEdit* meGameover = nullptr;
    QLineEdit* seCursor = nullptr;
    QLineEdit* seDecision = nullptr;
    QLineEdit* seCancel = nullptr;
    QLineEdit* seBuzzer = nullptr;
    QLineEdit* seEquip = nullptr;
    QLineEdit* seShop = nullptr;
    QLineEdit* seSave = nullptr;
    QLineEdit* seLoad = nullptr;
    QLineEdit* seBattleStart = nullptr;
    QLineEdit* seEscape = nullptr;
    QLineEdit* seActorCollapse = nullptr;
    QLineEdit* seEnemyCollapse = nullptr;
    // Words
    QLineEdit* wCurrency = nullptr;
    QLineEdit* wHp = nullptr;
    QLineEdit* wSp = nullptr;
    QLineEdit* wStr = nullptr;
    QLineEdit* wDex = nullptr;
    QLineEdit* wAgi = nullptr;
    QLineEdit* wInt = nullptr;
    QLineEdit* wAtk = nullptr;
    QLineEdit* wPdef = nullptr;
    QLineEdit* wMdef = nullptr;
    QLineEdit* wWeapon = nullptr;
    QLineEdit* wShield = nullptr;
    QLineEdit* wHelmet = nullptr;
    QLineEdit* wBodyArmor = nullptr;
    QLineEdit* wAccessory = nullptr;
    QLineEdit* wAttack = nullptr;
    QLineEdit* wSkill = nullptr;
    QLineEdit* wDefend = nullptr;
    QLineEdit* wItem = nullptr;
    QLineEdit* wEquip = nullptr;
    int selElement = -1;
};

void QtDatabaseDialog::buildSystemTab() {
    auto* page = new QWidget(mTabs);
    mTabs->addTab(page, QL("System"));
    mImpl = std::make_unique<Impl>();
    Impl& s = *mImpl;

    auto* scroll = new QScrollArea(page);
    scroll->setWidgetResizable(true);
    auto* outer = new QHBoxLayout(page);
    outer->setContentsMargins(0, 0, 0, 0);
    outer->addWidget(scroll);
    auto* host = new QWidget();
    scroll->setWidget(host);
    auto* cols = new QHBoxLayout(host);

    // --- Spalte 1: Initial Party + Elemente ------------------------------
    auto* col1 = new QVBoxLayout();
    auto* partyBox = new QGroupBox(QL("Anfangsgruppe"), host);
    auto* partyLay = new QVBoxLayout(partyBox);
    s.partyList = new QListWidget(partyBox);
    s.partyList->setMaximumHeight(140);
    for (const auto& a : mActors) {
        auto* item = new QListWidgetItem(
            QL("%1: %2").arg(a.id, 3, 10, QLatin1Char('0'))
                        .arg(QString::fromStdString(a.name)), s.partyList);
        item->setFlags(item->flags() | Qt::ItemIsUserCheckable);
        bool inParty = false;
        for (int id : mSystem.initialParty) if (id == a.id) inParty = true;
        item->setCheckState(inParty ? Qt::Checked : Qt::Unchecked);
    }
    partyLay->addWidget(s.partyList);
    col1->addWidget(partyBox);

    auto* elemBox = new QGroupBox(QL("Elemente"), host);
    auto* elemLay = new QVBoxLayout(elemBox);
    s.elementList = new QListWidget(elemBox);
    for (size_t i = 0; i < mSystem.elements.size(); ++i)
        s.elementList->addItem(IdName((int)i + 1,
            QString::fromStdString(mSystem.elements[i])));
    elemLay->addWidget(s.elementList, 1);
    s.elementName = makeLine(elemBox);
    elemLay->addWidget(s.elementName);
    auto* elemMaxBtn = new QPushButton(QL("Maximum ändern ..."), elemBox);
    elemLay->addWidget(elemMaxBtn);
    col1->addWidget(elemBox, 1);
    cols->addLayout(col1, 1);

    connect(s.elementList, &QListWidget::currentRowChanged, elemBox,
            [this, &s](int row) {
        Impl* sp = mImpl.get();
        if (sp->selElement >= 0
            && (size_t)sp->selElement < mSystem.elements.size()) {
            mSystem.elements[(size_t)sp->selElement] =
                s.elementName->text().toStdString();
        }
        sp->selElement = row;
        if (row >= 0 && (size_t)row < mSystem.elements.size())
            s.elementName->setText(
                QString::fromStdString(mSystem.elements[(size_t)row]));
    });
    connect(s.elementName, &QLineEdit::editingFinished, elemBox, [this, &s]() {
        Impl* sp = mImpl.get();
        if (sp->selElement >= 0
            && (size_t)sp->selElement < mSystem.elements.size()) {
            mSystem.elements[(size_t)sp->selElement] =
                s.elementName->text().toStdString();
            if (auto* li = s.elementList->item(sp->selElement))
                li->setText(IdName(sp->selElement + 1, s.elementName->text()));
        }
    });
    connect(elemMaxBtn, &QPushButton::clicked, elemBox, [this, &s]() {
        Impl* sp = mImpl.get();
        bool ok = false;
        int n = QInputDialog::getInt(this, QL("Maximum ändern"),
                                     QL("Anzahl der Elemente:"),
                                     (int)mSystem.elements.size(), 1, 99, 1, &ok);
        if (!ok) return;
        mSystem.elements.resize((size_t)n);
        for (size_t i = 0; i < mSystem.elements.size(); ++i)
            if (mSystem.elements[i].empty())
                mSystem.elements[i] = "Element " + std::to_string(i + 1);
        s.elementList->clear();
        for (size_t i = 0; i < mSystem.elements.size(); ++i)
            s.elementList->addItem(IdName((int)i + 1,
                QString::fromStdString(mSystem.elements[i])));
        sp->selElement = -1;
    });

    // --- Spalte 2: Grafiken + Audio -----------------------------------------
    auto* col2 = new QVBoxLayout();
    auto* gfxBox = new QGroupBox(QL("Systemgrafiken"), host);
    auto* gfxForm = new QFormLayout(gfxBox);
    s.gfxWindowskin = makeLine(gfxBox, QString::fromStdString(mSystem.windowskinName));
    s.gfxTitle = makeLine(gfxBox, QString::fromStdString(mSystem.titleGraphicName));
    s.gfxGameover = makeLine(gfxBox, QString::fromStdString(mSystem.gameoverGraphicName));
    s.gfxBattleTransition = makeLine(gfxBox, QString::fromStdString(mSystem.battleTransitionName));
    gfxForm->addRow(QL("Windowskin-Grafik"), s.gfxWindowskin);
    gfxForm->addRow(QL("Titelgrafik"), s.gfxTitle);
    gfxForm->addRow(QL("Game-Over-Grafik"), s.gfxGameover);
    gfxForm->addRow(QL("Kampf-Übergang"), s.gfxBattleTransition);
    col2->addWidget(gfxBox);

    auto* musikBox = new QGroupBox(QL("BGM / ME"), host);
    auto* musikForm = new QFormLayout(musikBox);
    s.bgmTitle = makeLine(musikBox, QString::fromStdString(mSystem.titleBgm));
    s.bgmBattle = makeLine(musikBox, QString::fromStdString(mSystem.battleBgm));
    s.meBattleEnd = makeLine(musikBox, QString::fromStdString(mSystem.battleEndMe));
    s.meGameover = makeLine(musikBox, QString::fromStdString(mSystem.gameoverMe));
    musikForm->addRow(QL("Titel-BGM"), s.bgmTitle);
    musikForm->addRow(QL("Kampf-BGM"), s.bgmBattle);
    musikForm->addRow(QL("Kampfende-ME"), s.meBattleEnd);
    musikForm->addRow(QL("Game-Over-ME"), s.meGameover);
    col2->addWidget(musikBox);

    auto* seBox = new QGroupBox(QL("Soundeffekte (SE)"), host);
    auto* seForm = new QFormLayout(seBox);
    s.seCursor = makeLine(seBox, QString::fromStdString(mSystem.cursorSe));
    s.seDecision = makeLine(seBox, QString::fromStdString(mSystem.decisionSe));
    s.seCancel = makeLine(seBox, QString::fromStdString(mSystem.cancelSe));
    s.seBuzzer = makeLine(seBox, QString::fromStdString(mSystem.buzzerSe));
    s.seEquip = makeLine(seBox, QString::fromStdString(mSystem.equipSe));
    s.seShop = makeLine(seBox, QString::fromStdString(mSystem.shopSe));
    s.seSave = makeLine(seBox, QString::fromStdString(mSystem.saveSe));
    s.seLoad = makeLine(seBox, QString::fromStdString(mSystem.loadSe));
    s.seBattleStart = makeLine(seBox, QString::fromStdString(mSystem.battleStartSe));
    s.seEscape = makeLine(seBox, QString::fromStdString(mSystem.escapeSe));
    s.seActorCollapse = makeLine(seBox, QString::fromStdString(mSystem.actorCollapseSe));
    s.seEnemyCollapse = makeLine(seBox, QString::fromStdString(mSystem.enemyCollapseSe));
    seForm->addRow(QL("Cursor-SE"), s.seCursor);
    seForm->addRow(QL("Bestätigen-SE"), s.seDecision);
    seForm->addRow(QL("Abbrechen-SE"), s.seCancel);
    seForm->addRow(QL("Buzzer-SE"), s.seBuzzer);
    seForm->addRow(QL("Ausrüsten-SE"), s.seEquip);
    seForm->addRow(QL("Shop-SE"), s.seShop);
    seForm->addRow(QL("Speichern-SE"), s.seSave);
    seForm->addRow(QL("Laden-SE"), s.seLoad);
    seForm->addRow(QL("Kampfbeginn-SE"), s.seBattleStart);
    seForm->addRow(QL("Flucht-SE"), s.seEscape);
    seForm->addRow(QL("Akteur besiegt-SE"), s.seActorCollapse);
    seForm->addRow(QL("Gegner besiegt-SE"), s.seEnemyCollapse);
    col2->addWidget(seBox, 1);
    cols->addLayout(col2, 1);

    // --- Spalte 3: Words -----------------------------------------------------
    auto* wordsBox = new QGroupBox(QL("Begriffe (Words)"), host);
    auto* wForm = new QFormLayout(wordsBox);
    s.wCurrency = makeLine(wordsBox, QString::fromStdString(mSystem.currencyUnit));
    s.wHp = makeLine(wordsBox, QString::fromStdString(mSystem.wordHp));
    s.wSp = makeLine(wordsBox, QString::fromStdString(mSystem.wordSp));
    s.wStr = makeLine(wordsBox, QString::fromStdString(mSystem.wordStr));
    s.wDex = makeLine(wordsBox, QString::fromStdString(mSystem.wordDex));
    s.wAgi = makeLine(wordsBox, QString::fromStdString(mSystem.wordAgi));
    s.wInt = makeLine(wordsBox, QString::fromStdString(mSystem.wordInt));
    s.wAtk = makeLine(wordsBox, QString::fromStdString(mSystem.wordAtk));
    s.wPdef = makeLine(wordsBox, QString::fromStdString(mSystem.wordPdef));
    s.wMdef = makeLine(wordsBox, QString::fromStdString(mSystem.wordMdef));
    s.wWeapon = makeLine(wordsBox, QString::fromStdString(mSystem.wordWeapon));
    s.wShield = makeLine(wordsBox, QString::fromStdString(mSystem.wordShield));
    s.wHelmet = makeLine(wordsBox, QString::fromStdString(mSystem.wordHelmet));
    s.wBodyArmor = makeLine(wordsBox, QString::fromStdString(mSystem.wordBodyArmor));
    s.wAccessory = makeLine(wordsBox, QString::fromStdString(mSystem.wordAccessory));
    s.wAttack = makeLine(wordsBox, QString::fromStdString(mSystem.wordAttack));
    s.wSkill = makeLine(wordsBox, QString::fromStdString(mSystem.wordSkill));
    s.wDefend = makeLine(wordsBox, QString::fromStdString(mSystem.wordDefend));
    s.wItem = makeLine(wordsBox, QString::fromStdString(mSystem.wordItem));
    s.wEquip = makeLine(wordsBox, QString::fromStdString(mSystem.wordEquip));
    wForm->addRow(QL("Währung (G)"), s.wCurrency);
    wForm->addRow(QL("HP"), s.wHp);
    wForm->addRow(QL("SP"), s.wSp);
    wForm->addRow(QL("STR"), s.wStr);
    wForm->addRow(QL("DEX"), s.wDex);
    wForm->addRow(QL("AGI"), s.wAgi);
    wForm->addRow(QL("INT"), s.wInt);
    wForm->addRow(QL("ATK"), s.wAtk);
    wForm->addRow(QL("PDEF"), s.wPdef);
    wForm->addRow(QL("MDEF"), s.wMdef);
    wForm->addRow(QL("Waffe"), s.wWeapon);
    wForm->addRow(QL("Schild"), s.wShield);
    wForm->addRow(QL("Helm"), s.wHelmet);
    wForm->addRow(QL("Körperrüstung"), s.wBodyArmor);
    wForm->addRow(QL("Accessoire"), s.wAccessory);
    wForm->addRow(QL("Angriff"), s.wAttack);
    wForm->addRow(QL("Fertigkeit"), s.wSkill);
    wForm->addRow(QL("Verteidigen"), s.wDefend);
    wForm->addRow(QL("Gegenstand"), s.wItem);
    wForm->addRow(QL("Ausrüsten"), s.wEquip);
    cols->addWidget(wordsBox, 1);

    // Writeback registrieren
    mExtraStore.push_back([this, &s]() {
        // Anfangsgruppe aus Checkboxes
        mSystem.initialParty.clear();
        for (int i = 0; i < s.partyList->count(); ++i) {
            auto* item = s.partyList->item(i);
            if (item->checkState() == Qt::Checked) {
                bool ok = false;
                int id = item->text().left(3).toInt(&ok);
                if (ok) mSystem.initialParty.push_back(id);
            }
        }
        // aktuelles Element-Feld sichern
        if (s.selElement >= 0 && (size_t)s.selElement < mSystem.elements.size())
            mSystem.elements[(size_t)s.selElement] =
                s.elementName->text().toStdString();
        mSystem.windowskinName = s.gfxWindowskin->text().toStdString();
        mSystem.titleGraphicName = s.gfxTitle->text().toStdString();
        mSystem.gameoverGraphicName = s.gfxGameover->text().toStdString();
        mSystem.battleTransitionName = s.gfxBattleTransition->text().toStdString();
        mSystem.titleBgm = s.bgmTitle->text().toStdString();
        mSystem.battleBgm = s.bgmBattle->text().toStdString();
        mSystem.battleEndMe = s.meBattleEnd->text().toStdString();
        mSystem.gameoverMe = s.meGameover->text().toStdString();
        mSystem.cursorSe = s.seCursor->text().toStdString();
        mSystem.decisionSe = s.seDecision->text().toStdString();
        mSystem.cancelSe = s.seCancel->text().toStdString();
        mSystem.buzzerSe = s.seBuzzer->text().toStdString();
        mSystem.equipSe = s.seEquip->text().toStdString();
        mSystem.shopSe = s.seShop->text().toStdString();
        mSystem.saveSe = s.seSave->text().toStdString();
        mSystem.loadSe = s.seLoad->text().toStdString();
        mSystem.battleStartSe = s.seBattleStart->text().toStdString();
        mSystem.escapeSe = s.seEscape->text().toStdString();
        mSystem.actorCollapseSe = s.seActorCollapse->text().toStdString();
        mSystem.enemyCollapseSe = s.seEnemyCollapse->text().toStdString();
        mSystem.currencyUnit = s.wCurrency->text().toStdString();
        mSystem.wordHp = s.wHp->text().toStdString();
        mSystem.wordSp = s.wSp->text().toStdString();
        mSystem.wordStr = s.wStr->text().toStdString();
        mSystem.wordDex = s.wDex->text().toStdString();
        mSystem.wordAgi = s.wAgi->text().toStdString();
        mSystem.wordInt = s.wInt->text().toStdString();
        mSystem.wordAtk = s.wAtk->text().toStdString();
        mSystem.wordPdef = s.wPdef->text().toStdString();
        mSystem.wordMdef = s.wMdef->text().toStdString();
        mSystem.wordWeapon = s.wWeapon->text().toStdString();
        mSystem.wordShield = s.wShield->text().toStdString();
        mSystem.wordHelmet = s.wHelmet->text().toStdString();
        mSystem.wordBodyArmor = s.wBodyArmor->text().toStdString();
        mSystem.wordAccessory = s.wAccessory->text().toStdString();
        mSystem.wordAttack = s.wAttack->text().toStdString();
        mSystem.wordSkill = s.wSkill->text().toStdString();
        mSystem.wordDefend = s.wDefend->text().toStdString();
        mSystem.wordItem = s.wItem->text().toStdString();
        mSystem.wordEquip = s.wEquip->text().toStdString();
    });
}

// ---------------------------------------------------------------------------
// Anwenden / OK
// ---------------------------------------------------------------------------

void QtDatabaseDialog::onApply() {
    storeAll();
    auto& db = rpg::Database::Get();
    db.Actors() = mActors;
    db.Classes() = mClasses;
    db.Items() = mItems;
    db.Weapons() = mWeapons;
    db.Armors() = mArmors;
    db.Skills() = mSkills;
    db.Enemies() = mEnemies;
    db.Troops() = mTroops;
    db.States() = mStates;
    db.Tilesets() = mTilesets;
    db.System() = mSystem;

    // Gemeinsame Events zurückschreiben (IDs/Members ersetzen)
    auto& ces = rpg::EventSystem::Get().GetCommonEvents();
    ces.assign(mCEs.begin(), mCEs.end());

    // Persistieren (DB + Common Events landen in events_<map>.json)
    if (mEngine) {
        const std::string pp = mEngine->GetProject().GetProjectPath();
        if (!pp.empty()) {
            db.Save(pp);
            const int mapId = rpg::Database::Get().System().startMapId;
            rpg::EventSystem::Get().SaveMapEvents(mapId > 0 ? mapId : 1, pp);
        }
    }
}

void QtDatabaseDialog::onOk() {
    onApply();
    accept();
}

bool QtDatabaseDialog::EditDatabase(QWidget* parent, rpg::Engine* engine) {
    QtDatabaseDialog dlg(engine, parent);
    return dlg.exec() == QDialog::Accepted;
}

} // namespace qt_editor
