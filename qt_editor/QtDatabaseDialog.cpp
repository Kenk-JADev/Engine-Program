#include "QtDatabaseDialog.h"
#include "QtEditorWindow.h" // qobject_cast fuer battleTestRequested-Verdrahtung

#include "QtEventEditorDialog.h"
#include "QtTilesetGridWidget.h"

#include <QFile>

#include "rpgmaker3d/Engine.h"
#include "rpgmaker3d/Project.h"

#include <algorithm>
#include <functional>
#include <memory>

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
#include <QMouseEvent>
#include <QPainter>
#include <QSpinBox>
#include <QTabWidget>
#include <QVariant>
#include <QVBoxLayout>

#include <cmath>

namespace qt_editor {

#ifndef QL
#define QL(x) QStringLiteral(x)
#endif

namespace {

// ---- PAKET 17: Listen-Helfer fuer Zustands-IDs / Resistenz-Raenge --------
QString JoinIds(const std::vector<int>& v) {
    QStringList l;
    for (int x : v) l << QString::number(x);
    return l.join(QL(", "));
}
std::vector<int> ParseIdsCsv(const QString& s) {
    std::vector<int> out;
    const QStringList parts = s.split(QLatin1Char(','), Qt::SkipEmptyParts);
    for (const QString& t : parts) {
        bool ok = false;
        const int n = t.trimmed().toInt(&ok);
        if (ok && n > 0) out.push_back(n);
    }
    return out;
}
// stateRanks (0..5 = A..F, Index = Zustands-ID-1) <-> "1=A,3=F"
QString RanksToText(const std::vector<int>& ranks) {
    QStringList l;
    for (size_t i = 0; i < ranks.size(); ++i)
        l << QL("%1=%2").arg(i + 1).arg(QLatin1Char((char)('A' + std::clamp(ranks[i], 0, 5))));
    return l.join(QL(", "));
}
// PAKET 18: lesbare Zeile fuer die XP-Gegner-Aktionstabelle
QString DescribeEnemyAction(const rpg::EnemyData::Action& a) {
    QString base;
    if (a.kind == 1) base = QL("Fertigkeit #%1").arg(a.skillId);
    else base = a.basic == 1 ? QL("Verteidigen") :
                a.basic == 2 ? QL("Flucht") :
                a.basic == 3 ? QL("Nichtstun") : QL("Angriff");
    QString cond;
    if (a.hpBelow < 100) cond += QL(", HP<=%1%").arg(a.hpBelow);
    if (a.turnA > 0 || a.turnB > 0) cond += QL(", Runde %1+%2x").arg(a.turnA).arg(a.turnB);
    if (a.level > 1) cond += QL(", Lv>=%1").arg(a.level);
    if (a.switchId > 0) cond += QL(", Schalter %1").arg(a.switchId);
    return QL("[R%1] %2%3").arg(a.rating).arg(base).arg(cond);
}

std::vector<int> ParseRanksText(const QString& s) {
    std::vector<int> out;
    const QStringList parts = s.split(QLatin1Char(','), Qt::SkipEmptyParts);
    for (const QString& t : parts) {
        const auto eq = t.indexOf(QLatin1Char('='));
        if (eq <= 0) continue;
        bool ok = false;
        const int id = t.left(eq).trimmed().toInt(&ok);
        if (!ok || id <= 0) continue;
        const QChar c = t.mid(eq + 1).trimmed().toUpper().isEmpty()
                        ? QLatin1Char('C') : t.mid(eq + 1).trimmed().toUpper().at(0);
        if (c < QLatin1Char('A') || c > QLatin1Char('F')) continue;
        if ((int)out.size() < id) out.resize((size_t)id, 2); // fehlende = C
        out[(size_t)(id - 1)] = c.unicode() - QL("A").at(0).unicode();
    }
    return out;
}

// XP-Animations-Canvas (Paket 5): halbe Aufloesung von 640x480,
// zeigt die Zellen des aktiven Frames, Klick legt Zelle, Rechtsklick loescht.
class QtAnimFrameCanvas : public QWidget {
public:
    explicit QtAnimFrameCanvas(QWidget* parent = nullptr) : QWidget(parent) {
        setFixedSize(320, 240);
    }

    rpg::AnimationData* anim = nullptr;
    int frameIdx = 0;
    int selectedCell = -1;           // Index in frames[frameIdx].cells
    std::function<void(int, int)> onAddCell;      // Klick: logisch x,y (640x480)
    std::function<void(int)> onSelectCell;        // Treffer-Zelle anklicken
    std::function<void(int)> onRemoveCell;        // Rechtsklick: Zelle loeschen

    void setSheet(const QImage& img) { sheet = img; update(); }

protected:
    void paintEvent(QPaintEvent*) override {
        QPainter p(this);
        p.fillRect(rect(), QColor(24, 24, 28));
        const QPointF center(160, 120);

        if (!anim || frameIdx < 0 || frameIdx >= (int)anim->frames.size()) {
            p.setPen(QColor(140, 140, 140));
            p.drawText(rect(), Qt::AlignCenter, QL("Kein Frame vorhanden\n(+ Frame anlegen)"));
            return;
        }
        const auto& fr = anim->frames[(size_t)frameIdx];

        // Mittelkreuz
        p.setPen(QPen(QColor(90, 90, 110), 1));
        p.drawLine(QPointF(0, 120), QPointF(320, 120));
        p.drawLine(QPointF(160, 0), QPointF(160, 240));

        // Zellen zeichnen (0.5-Skalierung)
        for (size_t i = 0; i < fr.cells.size(); ++i) {
            const auto& c = fr.cells[i];
            p.save();
            QPointF pos = center + QPointF(c.x * 0.5, c.y * 0.5);
            p.translate(pos);
            p.rotate(c.rotation);
            const float s = (c.scale / 100.0f) * 0.5f;
            const float w = 192.0f * s, h = 192.0f * s;
            if (!sheet.isNull()) {
                QRectF src((c.cellId % 5) * 192, (c.cellId / 5) * 192, 192, 192);
                p.setOpacity(c.opacity / 255.0);
                p.drawImage(QRectF(-w / 2, -h / 2, w, h), sheet, src);
                p.setOpacity(1.0);
            } else {
                // Platzhalter-Kreuz bei fehlendem Sheet
                p.setPen(QPen(QColor(90, 140, 220), 1));
                p.drawLine(QPointF(-6, -6), QPointF(6, 6));
                p.drawLine(QPointF(-6, 6), QPointF(6, -6));
                p.drawRect(QRectF(-w / 2, -h / 2, w, h));
            }
            if ((int)i == selectedCell) {
                p.setPen(QPen(QColor(255, 200, 60), 2));
                p.drawRect(QRectF(-w / 2, -h / 2, w, h));
            }
            p.setPen(QColor(200, 220, 255));
            p.drawText(QPointF(-w / 2 + 2, -h / 2 + 12), QString::number((int)i));
            p.restore();
        }

        // Frame/Randinfo
        p.setPen(QColor(160, 160, 160));
        p.drawText(rect().adjusted(6, 4, -6, -4),
                   QStringLiteral("Frame %1/%2 · %3 Zellen · Klick=+, Rechtsklick=−")
                       .arg(frameIdx + 1).arg((int)anim->frames.size()).arg((int)fr.cells.size()));
    }

    void mousePressEvent(QMouseEvent* e) override {
        if (!anim || frameIdx < 0 || frameIdx >= (int)anim->frames.size()) {
            QWidget::mousePressEvent(e); return;
        }
        // Position vom Zentrum aus, logisch (x2 fuer volle 640x480)
        const int lx = (int)std::lround((e->pos().x() - 160) * 2.0);
        const int ly = (int)std::lround((e->pos().y() - 120) * 2.0);
        const auto& fr = anim->frames[(size_t)frameIdx];

        if (e->button() == Qt::RightButton) {
            // naechstgelegene Zelle loeschen
            int best = -1; double bestD = 1e9;
            for (size_t i = 0; i < fr.cells.size(); ++i) {
                double d = std::hypot(fr.cells[i].x - lx, fr.cells[i].y - ly);
                if (d < bestD) { bestD = d; best = (int)i; }
            }
            if (best >= 0 && onRemoveCell) onRemoveCell(best);
            update(); return;
        }

        // Trefferpruefung existierende Zelle (Auswahl)
        int best = -1; double bestD = 1e9;
        for (size_t i = 0; i < fr.cells.size(); ++i) {
            double d = std::hypot(fr.cells[i].x - lx, fr.cells[i].y - ly);
            if (d < bestD) { bestD = d; best = (int)i; }
        }
        const float clickRadius = (best >= 0
            ? std::max(20.0, 96.0 * (fr.cells[(size_t)best].scale / 100.0) * 0.5) : 0.0);
        if (best >= 0 && bestD <= clickRadius * 2.0) {
            if (onSelectCell) onSelectCell(best);
        } else if (onAddCell) {
            onAddCell(lx, ly);
        }
        update();
    }

private:
    QImage sheet;
};

} // namespace (Canvas)

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
    mAnimations = db.AnimationSet();
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
    auto* equipsEdit = makeLine(formHost);
    equipsEdit->setToolTip(QL("Start-Ausrüstung als Waffen-/Rüstungs-IDs, kommagetrennt (z. B. 1,2).\n"
                              "Die erste gefundene Waffe wird angelegt, je Rüstungstyp max. 1 Stück."));
    auto* mhp = makeSpin(1, 99999, 100, formHost);
    auto* mmp = makeSpin(0, 99999, 30, formHost);
    auto* atk = makeSpin(0, 999, 10, formHost);
    auto* def = makeSpin(0, 999, 10, formHost);
    auto* mat = makeSpin(0, 999, 5, formHost);
    auto* mdf = makeSpin(0, 999, 5, formHost);
    auto* agi = makeSpin(0, 999, 10, formHost);
    auto* luk = makeSpin(0, 999, 10, formHost);
    // Endwerte (bei Max-Level) + Wachstumskurven A..E (XP-Stil)
    auto* fmhp = makeSpin(1, 999999, 100, formHost);
    auto* fmmp = makeSpin(1, 999999, 30, formHost);
    auto* fatk = makeSpin(1, 9999, 10, formHost);
    auto* fdef = makeSpin(1, 9999, 10, formHost);
    auto* fagi = makeSpin(1, 9999, 10, formHost);
    const QStringList kCurves = {QL("A (sehr schnell)"), QL("B (schnell)"),
                                 QL("C (mittel)"), QL("D (langsam)"),
                                 QL("E (sehr langsam)")};
    auto* cvHp = makeCombo(formHost, kCurves, 2);
    auto* cvMp = makeCombo(formHost, kCurves, 2);
    auto* cvAtk = makeCombo(formHost, kCurves, 2);
    auto* cvDef = makeCombo(formHost, kCurves, 2);
    auto* cvAgi = makeCombo(formHost, kCurves, 2);
    // PAKET 17: XP state_ranks — Zustands-Resistenz "ID=Rang(A..F)"
    auto* rankEdit = makeLine(formHost);
    rankEdit->setToolTip(QL("Zustands-Resistenz als ID=Rang, kommagetrennt (A..F).\n"
                            "A = 100 % Treffer, F = 0 % (immun). Fehlende Zustände = C (60 %).\n"
                            "Beispiel: 1=C, 2=A, 4=F"));

    form->addRow(QL("Name"), name);
    form->addRow(QL("Klasse"), klass);
    form->addRow(QL("Anfangs-Level"), initLv);
    form->addRow(QL("Max-Level"), maxLv);
    form->addRow(QL("Charakter-Grafik"), charName);
    form->addRow(QL("Face-Grafik"), faceName);
    form->addRow(QL("Battler-Grafik"), battlerName);
    form->addRow(QL("Start-Ausrüstung (IDs)"), equipsEdit);
    form->addRow(QL("Max. HP"), mhp);
    form->addRow(QL("Max. MP"), mmp);
    form->addRow(QL("Angriff"), atk);
    form->addRow(QL("Abwehr"), def);
    form->addRow(QL("Magie"), mat);
    form->addRow(QL("Magieabwehr"), mdf);
    form->addRow(QL("Agilität"), agi);
    form->addRow(QL("Glück"), luk);
    form->addRow(QL("Endwert Max. HP"), fmhp);
    form->addRow(QL("Endwert Max. MP"), fmmp);
    form->addRow(QL("Endwert Angriff"), fatk);
    form->addRow(QL("Endwert Abwehr"), fdef);
    form->addRow(QL("Endwert Agilität"), fagi);
    form->addRow(QL("Kurve Max. HP"), cvHp);
    form->addRow(QL("Kurve Max. MP"), cvMp);
    form->addRow(QL("Kurve Angriff"), cvAtk);
    form->addRow(QL("Kurve Abwehr"), cvDef);
    form->addRow(QL("Kurve Agilität"), cvAgi);
    form->addRow(QL("Zustands-Ränge (ID=Grad)"), rankEdit); // PAKET 17

    tp->count = [this]() { return (int)mActors.size(); };
    tp->nameAt = [this](int i) {
        return IdName(i + 1, QString::fromStdString(mActors[(size_t)i].name));
    };
    tp->setMax = [this](int n) {
        mActors.resize((size_t)n);
        for (size_t i = 0; i < mActors.size(); ++i) mActors[i].id = (int)i + 1;
    };
    tp->loadForm = [this, tp, name, klass, initLv, maxLv, charName, faceName,
                    battlerName, equipsEdit, rankEdit, mhp, mmp, atk, def, mat, mdf, agi, luk,
                    fmhp, fmmp, fatk, fdef, fagi,
                    cvHp, cvMp, cvAtk, cvDef, cvAgi](int i) {
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
        QStringList eqs;
        for (int e : a.equips) eqs << QString::number(e);
        equipsEdit->setText(eqs.join(QLatin1String(", ")));
        rankEdit->setText(RanksToText(a.stateRanks)); // PAKET 17
        mhp->setValue(a.initialStats.mhp);
        mmp->setValue(a.initialStats.mmp);
        atk->setValue(a.initialStats.atk);
        def->setValue(a.initialStats.def);
        mat->setValue(a.initialStats.mat);
        mdf->setValue(a.initialStats.mdf);
        agi->setValue(a.initialStats.agi);
        luk->setValue(a.initialStats.luk);
        fmhp->setValue(a.finalStats.mhp > 0 ? a.finalStats.mhp : 100);
        fmmp->setValue(a.finalStats.mmp > 0 ? a.finalStats.mmp : 30);
        fatk->setValue(a.finalStats.atk > 0 ? a.finalStats.atk : 10);
        fdef->setValue(a.finalStats.def > 0 ? a.finalStats.def : 10);
        fagi->setValue(a.finalStats.agi > 0 ? a.finalStats.agi : 10);
        const auto curveIdx = [](char c) { return (c >= 'A' && c <= 'E') ? c - 'A' : 2; };
        cvHp->setCurrentIndex(curveIdx(a.curveHp));
        cvMp->setCurrentIndex(curveIdx(a.curveMp));
        cvAtk->setCurrentIndex(curveIdx(a.curveAtk));
        cvDef->setCurrentIndex(curveIdx(a.curveDef));
        cvAgi->setCurrentIndex(curveIdx(a.curveAgi));
    };
    tp->storeForm = [this, tp, name, klass, initLv, maxLv, charName, faceName,
                     battlerName, equipsEdit, rankEdit, mhp, mmp, atk, def, mat, mdf, agi, luk,
                     fmhp, fmmp, fatk, fdef, fagi,
                     cvHp, cvMp, cvAtk, cvDef, cvAgi](int i) {
        if ((size_t)i >= mActors.size()) return;
        auto& a = mActors[(size_t)i];
        a.stateRanks = ParseRanksText(rankEdit->text()); // PAKET 17
        a.name = name->text().toStdString();
        a.className = klass->currentText().toStdString();
        a.initialLevel = initLv->value();
        a.maxLevel = maxLv->value();
        a.characterName = charName->text().toStdString();
        a.faceName = faceName->text().toStdString();
        a.battlerName = battlerName->text().toStdString();
        a.equips.clear();
        const QStringList eqs = equipsEdit->text().split(QLatin1Char(','), Qt::SkipEmptyParts);
        for (const QString& t : eqs) {
            bool ok = false;
            const int v = t.trimmed().toInt(&ok);
            if (ok && v > 0) a.equips.push_back(v);
        }
        a.initialStats.mhp = mhp->value();
        a.initialStats.mmp = mmp->value();
        a.initialStats.atk = atk->value();
        a.initialStats.def = def->value();
        a.initialStats.mat = mat->value();
        a.initialStats.mdf = mdf->value();
        a.initialStats.agi = agi->value();
        a.initialStats.luk = luk->value();
        a.finalStats.mhp = fmhp->value();
        a.finalStats.mmp = fmmp->value();
        a.finalStats.atk = fatk->value();
        a.finalStats.def = fdef->value();
        a.finalStats.agi = fagi->value();
        const auto curveChar = [](int i) { return char('A' + std::max(0, std::min(4, i))); };
        a.curveHp = curveChar(cvHp->currentIndex());
        a.curveMp = curveChar(cvMp->currentIndex());
        a.curveAtk = curveChar(cvAtk->currentIndex());
        a.curveDef = curveChar(cvDef->currentIndex());
        a.curveAgi = curveChar(cvAgi->currentIndex());
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
    auto* learn = makeLine(formHost);
    learn->setToolTip(QL("Fertigkeiten, die die Klasse automatisch ab einem Level lernt.\n"
                         "Format: Level:Fertigkeits-ID, kommagetrennt, z. B. 2:2, 4:3"));
    form->addRow(QL("Name"), name);
    form->addRow(QL("EXP-Basis"), base);
    form->addRow(QL("EXP-Zuschlag"), extra);
    form->addRow(QL("EXP-Beschleunigung A"), accA);
    form->addRow(QL("EXP-Beschleunigung B"), accB);
    form->addRow(QL("Fertigkeiten ab Level"), learn);

    tp->count = [this]() { return (int)mClasses.size(); };
    tp->nameAt = [this](int i) {
        return IdName(i + 1, QString::fromStdString(mClasses[(size_t)i].name));
    };
    tp->setMax = [this](int n) {
        mClasses.resize((size_t)n);
        for (size_t i = 0; i < mClasses.size(); ++i) mClasses[i].id = (int)i + 1;
    };
    tp->loadForm = [this, name, base, extra, accA, accB, learn](int i) {
        auto& c = mClasses[(size_t)i];
        name->setText(QString::fromStdString(c.name));
        base->setValue(c.expBase);
        extra->setValue(c.expExtra);
        accA->setValue((double)c.expAccA);
        accB->setValue((double)c.expAccB);
        QStringList toks;
        for (const auto& lrn : c.learnings)
            toks << QString::number(lrn.level) + QLatin1Char(':') +
                        QString::number(lrn.skillId);
        learn->setText(toks.join(QLatin1String(", ")));
    };
    tp->storeForm = [this, tp, name, base, extra, accA, accB, learn](int i) {
        if ((size_t)i >= mClasses.size()) return;
        auto& c = mClasses[(size_t)i];
        c.name = name->text().toStdString();
        c.expBase = base->value();
        c.expExtra = extra->value();
        c.expAccA = (float)accA->value();
        c.expAccB = (float)accB->value();
        // Lern-Liste "Level:Fertigkeits-ID, ..." (robustes Parsen)
        c.learnings.clear();
        const QStringList toks = learn->text().split(QLatin1Char(','), Qt::SkipEmptyParts);
        for (const QString& t : toks) {
            const QStringList pair = t.trimmed().split(QLatin1Char(':'));
            if (pair.size() != 2) continue;
            bool okLv = false, okSk = false;
            const int lv = pair[0].trimmed().toInt(&okLv);
            const int sk = pair[1].trimmed().toInt(&okSk);
            if (okLv && okSk && lv >= 1 && sk > 0)
                c.learnings.push_back({lv, sk});
        }
        std::sort(c.learnings.begin(), c.learnings.end(),
                  [](const rpg::ClassData::Learning& a, const rpg::ClassData::Learning& b) {
                      return a.level < b.level;
                  });
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
    // PAKET 22: volle XP-Scope-Liste (0..7, wie bei Items) —
    // Wiederbelebungs-Scopes 5/6; power gilt dort als Prozent der max. HP.
    auto* scope = makeCombo(formHost, {QL("Kein Ziel"), QL("Ein Gegner"), QL("Alle Gegner"),
                                       QL("Ein Verbündeter"), QL("Alle Verbündeten"),
                                       QL("Verbündeter (tot)"), QL("Alle Verbündeten (tot)"),
                                       QL("Anwender")}, 1);
    auto* power = makeSpin(0, 9999, 100, formHost);
    auto* anim = makeLine(formHost);
    auto* animId = makeSpin(0, 999, 0, formHost); // PAKET 12: XP animation_id
    // PAKET 17: XP plus_state_set / minus_state_set (Zustands-IDs, kommagetrennt)
    auto* plusEdit = makeLine(formHost);
    plusEdit->setToolTip(QL("Zustände, die der Skill beim Treffer verhängt (XP plus_state_set).\n"
                            "IDs kommagetrennt, z. B. 1, 3. Trefferchance nach Resistenz-Rang des Ziels."));
    auto* minusEdit = makeLine(formHost);
    minusEdit->setToolTip(QL("Zustände, die der Skill beim Treffer sicher heilt (XP minus_state_set).\n"
                             "IDs kommagetrennt, z. B. 1, 2."));
    form->addRow(QL("Name"), name);
    form->addRow(QL("Beschreibung"), desc);
    form->addRow(QL("MP-Kosten"), cost);
    form->addRow(QL("Reichweite"), scope);
    form->addRow(QL("Stärke"), power);
    form->addRow(QL("Animations-ID"), animId);
    form->addRow(QL("Legacy-Animationsname"), anim);
    // PAKET 21: XP occasion (Anlass) — steuert Benutzbarkeit Kampf/Menue
    auto* occ = makeCombo(formHost, {QL("Immer"), QL("Nur im Kampf"),
                                     QL("Nur im Menü"), QL("Nie")}, 0);
    form->addRow(QL("Verhängt Zustände (IDs)"), plusEdit);
    form->addRow(QL("Heilt Zustände (IDs)"), minusEdit);
    form->addRow(QL("Anlass"), occ);

    tp->count = [this]() { return (int)mSkills.size(); };
    tp->nameAt = [this](int i) {
        return IdName(i + 1, QString::fromStdString(mSkills[(size_t)i].name));
    };
    tp->setMax = [this](int n) {
        mSkills.resize((size_t)n);
        for (size_t i = 0; i < mSkills.size(); ++i) mSkills[i].id = (int)i + 1;
    };
    tp->loadForm = [this, name, desc, cost, scope, power, anim, animId,
                    plusEdit, minusEdit, occ](int i) {
        auto& s = mSkills[(size_t)i];
        name->setText(QString::fromStdString(s.name));
        desc->setText(QString::fromStdString(s.description));
        cost->setValue(s.mpCost);
        scope->setCurrentIndex(qBound(0, s.scope, 7)); // PAKET 22 (0..7)
        power->setValue(s.power);
        anim->setText(QString::fromStdString(s.animation));
        animId->setValue(s.animationId);
        plusEdit->setText(JoinIds(s.plusStates));   // PAKET 17
        minusEdit->setText(JoinIds(s.minusStates)); // PAKET 17
        occ->setCurrentIndex(qBound(0, s.occasion, 3)); // PAKET 21
    };
    tp->storeForm = [this, tp, name, desc, cost, scope, power, anim, animId,
                     plusEdit, minusEdit, occ](int i) {
        if ((size_t)i >= mSkills.size()) return;
        auto& s = mSkills[(size_t)i];
        s.name = name->text().toStdString();
        s.description = desc->text().toStdString();
        s.mpCost = cost->value();
        s.scope = scope->currentIndex();
        s.power = power->value();
        s.animation = anim->text().toStdString();
        s.animationId = animId->value();
        s.plusStates = ParseIdsCsv(plusEdit->text());   // PAKET 17
        s.minusStates = ParseIdsCsv(minusEdit->text()); // PAKET 17
        s.occasion = occ->currentIndex();               // PAKET 21
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
    auto* animId = makeSpin(0, 999, 0, formHost); // PAKET 12: XP-Animation im Kampf
    // PAKET 20: XP plus_state_set / minus_state_set (Zustands-IDs, kommagetrennt)
    auto* plusEdit = makeLine(formHost);
    plusEdit->setToolTip(QL("Zustände, die das Item bei Benutzung verhängt (XP plus_state_set).\n"
                            "IDs kommagetrennt, z. B. 1, 3."));
    auto* minusEdit = makeLine(formHost);
    minusEdit->setToolTip(QL("Zustände, die das Item bei Benutzung heilt (XP minus_state_set).\n"
                             "IDs kommagetrennt, z. B. 1 (Antidot). Wirkt im Kampf UND im Menü."));
    form->addRow(QL("Name"), name);
    form->addRow(QL("Beschreibung"), desc);
    form->addRow(QL("Preis"), price);
    form->addRow(QL("Typ"), type);
    form->addRow(consumable);
    form->addRow(QL("Reichweite"), scope);
    form->addRow(QL("HP-Genesung"), hpRec);
    form->addRow(QL("MP-Genesung"), mpRec);
    form->addRow(QL("Animations-ID"), animId);
    form->addRow(QL("Verhängt Zustände (IDs)"), plusEdit);
    form->addRow(QL("Heilt Zustände (IDs)"), minusEdit);

    tp->count = [this]() { return (int)mItems.size(); };
    tp->nameAt = [this](int i) {
        return IdName(i + 1, QString::fromStdString(mItems[(size_t)i].name));
    };
    tp->setMax = [this](int n) {
        mItems.resize((size_t)n);
        for (size_t i = 0; i < mItems.size(); ++i) mItems[i].id = (int)i + 1;
    };
    tp->loadForm = [this, name, desc, price, type, consumable, scope, hpRec, mpRec, animId,
                    plusEdit, minusEdit](int i) {
        auto& it = mItems[(size_t)i];
        name->setText(QString::fromStdString(it.name));
        desc->setText(QString::fromStdString(it.description));
        price->setValue(it.price);
        type->setCurrentIndex((int)it.itemType);
        consumable->setChecked(it.consumable);
        scope->setCurrentIndex(qBound(0, (int)it.scope, 7));
        hpRec->setValue(it.hpRecovery);
        mpRec->setValue(it.mpRecovery);
        animId->setValue(it.animationId);
        plusEdit->setText(JoinIds(it.plusStates));   // PAKET 20
        minusEdit->setText(JoinIds(it.minusStates)); // PAKET 20
    };
    tp->storeForm = [this, tp, name, desc, price, type, consumable, scope,
                     hpRec, mpRec, animId, plusEdit, minusEdit](int i) {
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
        it.animationId = animId->value();
        it.plusStates = ParseIdsCsv(plusEdit->text());   // PAKET 20
        it.minusStates = ParseIdsCsv(minusEdit->text()); // PAKET 20
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
    // PAKET 22: XP plus_state_set / minus_state_set der Waffe
    auto* plusEdit = makeLine(formHost);
    plusEdit->setToolTip(QL("Zustände, die ein Treffer mit dieser Waffe verhängt (XP plus_state_set).\n"
                            "IDs kommagetrennt, z. B. 1 (Gift). Trefferchance nach Resistenz-Rang des Ziels."));
    auto* minusEdit = makeLine(formHost);
    minusEdit->setToolTip(QL("Zustände, die ein Treffer mit dieser Waffe heilt (XP minus_state_set).\n"
                             "IDs kommagetrennt."));
    form->addRow(QL("Name"), name);
    form->addRow(QL("Beschreibung"), desc);
    form->addRow(QL("Preis"), price);
    form->addRow(QL("Angriff"), atk);
    form->addRow(QL("Animations-ID"), animId);
    form->addRow(QL("Verhängt Zustände (IDs)"), plusEdit);
    form->addRow(QL("Heilt Zustände (IDs)"), minusEdit);

    tp->count = [this]() { return (int)mWeapons.size(); };
    tp->nameAt = [this](int i) {
        return IdName(i + 1, QString::fromStdString(mWeapons[(size_t)i].name));
    };
    tp->setMax = [this](int n) {
        mWeapons.resize((size_t)n);
        for (size_t i = 0; i < mWeapons.size(); ++i) mWeapons[i].id = (int)i + 1;
    };
    tp->loadForm = [this, name, desc, price, atk, animId, plusEdit, minusEdit](int i) {
        auto& w = mWeapons[(size_t)i];
        name->setText(QString::fromStdString(w.name));
        desc->setText(QString::fromStdString(w.description));
        price->setValue(w.price);
        atk->setValue(w.atk);
        animId->setValue(w.animationId);
        plusEdit->setText(JoinIds(w.plusStates));   // PAKET 22
        minusEdit->setText(JoinIds(w.minusStates)); // PAKET 22
    };
    tp->storeForm = [this, tp, name, desc, price, atk, animId, plusEdit, minusEdit](int i) {
        if ((size_t)i >= mWeapons.size()) return;
        auto& w = mWeapons[(size_t)i];
        w.name = name->text().toStdString();
        w.description = desc->text().toStdString();
        w.price = price->value();
        w.atk = atk->value();
        w.animationId = animId->value();
        w.plusStates = ParseIdsCsv(plusEdit->text());   // PAKET 22
        w.minusStates = ParseIdsCsv(minusEdit->text()); // PAKET 22
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
    auto* hue = makeSpin(0, 360, 0, formHost); // XP: Farbton des Battler-Bildes
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
    // PAKET 17: XP state_ranks — Zustands-Resistenz "ID=Rang(A..F)"
    auto* rankEdit = makeLine(formHost);
    rankEdit->setToolTip(QL("Zustands-Resistenz als ID=Rang, kommagetrennt (A..F).\n"
                            "A = 100 % Treffer, F = 0 % (immun). Fehlende Zustände = C (60 %).\n"
                            "Beispiel: 1=C, 2=A, 4=F"));

    form->addRow(QL("Name"), name);
    form->addRow(QL("Battler-Grafik"), battler);
    form->addRow(QL("Farbton"), hue);
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
    form->addRow(QL("Zustands-Ränge (ID=Grad)"), rankEdit); // PAKET 17

    // ------------------------------------------------------------------
    // PAKET 18: XP-Aktionstabelle (RPG::Enemy.actions) — Gegner ohne
    // Eintrag fuehren weiterhin den Standardangriff aus (Engine-Fallback).
    // ------------------------------------------------------------------
    auto actBuffer = std::make_shared<std::vector<rpg::EnemyData::Action>>();
    auto actLoading = std::make_shared<bool>(false);
    auto* actBox = new QGroupBox(QL("Aktionen (XP-Verhaltenstabelle)"), formHost);
    actBox->setToolTip(QL("Alle erfüllten Aktionen kommen in den Lostopf — XP wählt "
                          "gleichverteilt nur aus Einträgen mit Rating > Tabellenmaximum − 3.\n"
                          "Leere Tabelle = Standardangriff (bisheriges Verhalten)."));
    auto* actVL = new QVBoxLayout(actBox);
    auto* actList = new QListWidget(actBox);
    actList->setMaximumHeight(120);
    actVL->addWidget(actList);
    auto* actBtnRow = new QHBoxLayout();
    auto* actAdd = new QPushButton(QL("Hinzufügen"), actBox);
    auto* actDel = new QPushButton(QL("Entfernen"), actBox);
    actBtnRow->addWidget(actAdd);
    actBtnRow->addWidget(actDel);
    actBtnRow->addStretch(1);
    actVL->addLayout(actBtnRow);
    auto* actForm = new QFormLayout();
    auto* kindC = makeCombo(actBox, {QL("Basis-Aktion"), QL("Fertigkeit")}, 0);
    auto* basicC = makeCombo(actBox, {QL("Angriff"), QL("Verteidigen"),
                                      QL("Flucht"), QL("Nichtstun")}, 0);
    auto* skillC = new QComboBox(actBox);
    for (const auto& s : mSkills)
        skillC->addItem(IdName(s.id, QString::fromStdString(s.name)), s.id);
    auto* ratingS = makeSpin(1, 10, 5, actBox);
    auto* hpS = makeSpin(0, 100, 100, actBox);
    auto* turnAS = makeSpin(0, 99, 0, actBox);
    auto* turnBS = makeSpin(0, 99, 0, actBox);
    auto* levelS = makeSpin(1, 99, 1, actBox);
    auto* switchS = makeSpin(0, 9999, 0, actBox);
    actForm->addRow(QL("Art"), kindC);
    actForm->addRow(QL("Basis-Aktion"), basicC);
    actForm->addRow(QL("Fertigkeit"), skillC);
    actForm->addRow(QL("Rating (1..10)"), ratingS);
    actForm->addRow(QL("Bedingung: eigene HP ≤ %"), hpS);
    auto* turnRowW = new QWidget(actBox);
    auto* turnRowL = new QHBoxLayout(turnRowW);
    turnRowL->setContentsMargins(0, 0, 0, 0);
    turnRowL->addWidget(new QLabel(QL("Runde A"), turnRowW));
    turnRowL->addWidget(turnAS);
    turnRowL->addWidget(new QLabel(QL("+ B·x"), turnRowW));
    turnRowL->addWidget(turnBS);
    turnRowL->addStretch(1);
    actForm->addRow(QL("Bedingung: Runde"), turnRowW);
    actForm->addRow(QL("Bedingung: Party-Level ≥"), levelS);
    actForm->addRow(QL("Bedingung: Schalter-ID (0=keiner)"), switchS);
    actVL->addLayout(actForm);
    form->addRow(actBox);

    // Felder einer Tabellenzeile laden/schreiben (Schreiben = write-through)
    auto loadActFields = [=](int row) {
        *actLoading = true;
        if (row >= 0 && row < (int)actBuffer->size()) {
            const auto& a = (*actBuffer)[(size_t)row];
            kindC->setCurrentIndex(qBound(0, a.kind, 1));
            basicC->setCurrentIndex(qBound(0, a.basic, 3));
            const int si = skillC->findData(a.skillId);
            skillC->setCurrentIndex(si >= 0 ? si : 0);
            ratingS->setValue(qBound(1, a.rating, 10));
            hpS->setValue(qBound(0, a.hpBelow, 100));
            turnAS->setValue(a.turnA);
            turnBS->setValue(a.turnB);
            levelS->setValue(a.level);
            switchS->setValue(a.switchId);
        }
        const bool isSkill = row >= 0 && (*actBuffer)[(size_t)row].kind == 1;
        basicC->setEnabled(!isSkill);
        skillC->setEnabled(isSkill);
        *actLoading = false;
    };
    std::function<void(int)> refreshActs = [=](int select) {
        *actLoading = true;
        actList->clear();
        for (const auto& a : *actBuffer) actList->addItem(DescribeEnemyAction(a));
        *actLoading = false;
        if (!actBuffer->empty()) {
            const int s = qBound(0, select, (int)actBuffer->size() - 1);
            actList->setCurrentRow(s);
        }
        loadActFields(actBuffer->empty() ? -1 : actList->currentRow());
    };
    std::function<void()> applyActFields = [=]() {
        if (*actLoading) return;
        const int row = actList->currentRow();
        if (row < 0 || row >= (int)actBuffer->size()) return;
        auto& a = (*actBuffer)[(size_t)row];
        a.kind = kindC->currentIndex();
        a.basic = basicC->currentIndex();
        a.skillId = skillC->currentData().toInt();
        a.rating = ratingS->value();
        a.hpBelow = hpS->value();
        a.turnA = turnAS->value();
        a.turnB = turnBS->value();
        a.level = levelS->value();
        a.switchId = switchS->value();
        basicC->setEnabled(a.kind == 0);
        skillC->setEnabled(a.kind == 1);
        if (auto* it = actList->item(row)) it->setText(DescribeEnemyAction(a));
    };
    QObject::connect(actList, &QListWidget::currentRowChanged, actList,
                     [loadActFields](int r) { loadActFields(r); });
    QObject::connect(actAdd, &QPushButton::clicked, actAdd, [=]() {
        actBuffer->push_back({}); // Defaults: Angriff, Rating 5
        refreshActs((int)actBuffer->size() - 1);
    });
    QObject::connect(actDel, &QPushButton::clicked, actDel, [=]() {
        const int r = actList->currentRow();
        if (r < 0 || r >= (int)actBuffer->size()) return;
        actBuffer->erase(actBuffer->begin() + r);
        refreshActs(qMin(r, (int)actBuffer->size() - 1));
    });
    for (auto* c : {kindC, basicC, skillC})
        QObject::connect(c, QOverload<int>::of(&QComboBox::currentIndexChanged),
                         c, [applyActFields](int) { applyActFields(); });
    for (auto* s : {ratingS, hpS, turnAS, turnBS, levelS, switchS})
        QObject::connect(s, QOverload<int>::of(&QSpinBox::valueChanged),
                         s, [applyActFields](int) { applyActFields(); });

    tp->count = [this]() { return (int)mEnemies.size(); };
    tp->nameAt = [this](int i) {
        return IdName(i + 1, QString::fromStdString(mEnemies[(size_t)i].name));
    };
    tp->setMax = [this](int n) {
        mEnemies.resize((size_t)n);
        for (size_t i = 0; i < mEnemies.size(); ++i) mEnemies[i].id = (int)i + 1;
    };
    tp->loadForm = [this, name, battler, hue, mhp, mmp, atk, def, mat, mdf, agi, luk,
                    exp, gold, drops, rankEdit, actBuffer, refreshActs](int i) {
        auto& e = mEnemies[(size_t)i];
        *actBuffer = e.actions;           // PAKET 18: Tabelle uebernehmen
        refreshActs(0);
        name->setText(QString::fromStdString(e.name));
        battler->setText(QString::fromStdString(e.battlerName));
        rankEdit->setText(RanksToText(e.stateRanks)); // PAKET 17
        hue->setValue(e.battlerHue);
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
    tp->storeForm = [this, tp, name, battler, hue, mhp, mmp, atk, def, mat, mdf, agi,
                     luk, exp, gold, drops, rankEdit, actBuffer](int i) {
        if ((size_t)i >= mEnemies.size()) return;
        auto& e = mEnemies[(size_t)i];
        e.actions = *actBuffer;                        // PAKET 18
        e.stateRanks = ParseRanksText(rankEdit->text()); // PAKET 17
        e.name = name->text().toStdString();
        e.battlerName = battler->text().toStdString();
        e.battlerHue = hue->value();
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

    // XP: „Kampftest"-Knopf im Trupps-Tab - startet den Player direkt im
    // Kampf gegen den aktuell gewaehlten Trupp (--battletest=<id>).
    auto* battleTestBtn = new QPushButton(QL("Kampftest"), formHost);
    battleTestBtn->setToolTip(QL("Startet die Player-exe direkt im Kampf gegen diesen Trupp.\n"
                                 "Die Anfangsgruppe kommt aus dem System-Tab."));
    form->addRow(battleTestBtn);
    connect(battleTestBtn, &QPushButton::clicked, formHost, [this, tp]() {
        const int troopId = (tp->list && tp->list->currentRow() >= 0)
            ? tp->list->currentRow() + 1 : 1;
        emit battleTestRequested(troopId);
    });

    // --- XP-Kampfereignisse (Seiten) ---
    // Pro Trupp bis zu 12 Seiten: Bedingungen (alle muessen erfuellt sein),
    // Spanne (Kampf/Runde/Moment) und das auszufuehrende Gem. Event.
    auto* pagesLabel = new QLabel(
        QL("Kampf-Ereignisse (wie im XP): Die Befehle einer Seite laufen als\n"
           "Gemeinsames Ereignis (Tab \"Gem. Events\"). Alle angekreuzten\n"
           "Bedingungen müssen erfüllt sein. Der Kampf pausiert währenddessen."), formHost);
    pagesLabel->setWordWrap(true);
    auto* pageList = new QListWidget(formHost);
    pageList->setMaximumHeight(100);
    auto* pageBtnRow = new QWidget(formHost);
    auto* pageBtnLay = new QHBoxLayout(pageBtnRow);
    pageBtnLay->setContentsMargins(0, 0, 0, 0);
    auto* pageAddBtn = new QPushButton(QL("Seite hinzufügen"), pageBtnRow);
    auto* pageRemBtn = new QPushButton(QL("Seite entfernen"), pageBtnRow);
    pageBtnLay->addWidget(pageAddBtn);
    pageBtnLay->addWidget(pageRemBtn);
    auto* span = makeCombo(formHost, {QL("Kampf (einmal je Kampf)"),
                                      QL("Runde (einmal je Runde)"),
                                      QL("Moment (sofort bei Erfüllung)")}, 0);
    auto* ceCombo = new QComboBox(formHost);
    auto* swCheck = new QCheckBox(QL("Schalter AN"), formHost);
    auto* swId = makeSpin(1, 9999, 1, formHost);
    auto* turnCheck = new QCheckBox(QL("Runde erreicht"), formHost);
    auto* turnA = makeSpin(0, 999, 0, formHost);
    auto* turnB = makeSpin(0, 999, 0, formHost);
    auto* actorCheck = new QCheckBox(QL("Akteur HP <="), formHost);
    auto* actorIdx = makeSpin(1, 8, 1, formHost);
    auto* actorPct = makeSpin(1, 100, 50, formHost);
    auto* enemyCheck = new QCheckBox(QL("Gegner HP <="), formHost);
    auto* enemyIdx = makeSpin(1, 8, 1, formHost);
    auto* enemyPct = makeSpin(1, 100, 50, formHost);

    form->addRow(pagesLabel);
    form->addRow(QL("Seiten"), pageList);
    form->addRow(pageBtnRow);
    form->addRow(QL("Spanne"), span);
    form->addRow(QL("Gem. Event"), ceCombo);
    // Bedingungs-Reihen: Checkbox links, Parameter rechts in einer Zeile
    const auto rowOf = [formHost](QWidget* a, QWidget* b, const QString& sep) {
        auto* w = new QWidget(formHost);
        auto* lay = new QHBoxLayout(w);
        lay->setContentsMargins(0, 0, 0, 0);
        lay->addWidget(a);
        if (!sep.isEmpty()) lay->addWidget(new QLabel(sep, w));
        if (b) lay->addWidget(b);
        lay->addStretch(1);
        return w;
    };
    form->addRow(swCheck, rowOf(swId, nullptr, QString()));
    form->addRow(turnCheck, rowOf(turnA, turnB, QL("+ n x")));
    auto* actorPctL = new QWidget(formHost);
    {
        auto* lay = new QHBoxLayout(actorPctL);
        lay->setContentsMargins(0, 0, 0, 0);
        lay->addWidget(actorIdx);
        lay->addWidget(new QLabel(QL("%:"), actorPctL));
        lay->addWidget(actorPct);
        lay->addStretch(1);
    }
    form->addRow(actorCheck, actorPctL);
    auto* enemyPctL = new QWidget(formHost);
    {
        auto* lay = new QHBoxLayout(enemyPctL);
        lay->setContentsMargins(0, 0, 0, 0);
        lay->addWidget(enemyIdx);
        lay->addWidget(new QLabel(QL("%:"), enemyPctL));
        lay->addWidget(enemyPct);
        lay->addStretch(1);
    }
    form->addRow(enemyCheck, enemyPctL);

    // gemeinsamer Seiten-Status + Lade-/Speicher-Helfer
    auto curPage = std::make_shared<int>(-1);
    std::function<void(int)> loadPage;
    std::function<void()> storeCurrentPage;
    storeCurrentPage = [this, tp, curPage, span, ceCombo, swCheck, swId,
                        turnCheck, turnA, turnB, actorCheck, actorIdx, actorPct,
                        enemyCheck, enemyIdx, enemyPct]() {
        if (tp->current < 0 || (size_t)tp->current >= mTroops.size()) return;
        auto& tr = mTroops[(size_t)tp->current];
        if (*curPage < 0 || (size_t)*curPage >= tr.pages.size()) return;
        auto& p = tr.pages[(size_t)*curPage];
        p.span = span->currentIndex();
        p.commonEventId = ceCombo->currentData().toInt();
        p.switchValid = swCheck->isChecked();
        p.switchId = swId->value();
        p.turnValid = turnCheck->isChecked();
        p.turnA = turnA->value();
        p.turnB = turnB->value();
        p.actorValid = actorCheck->isChecked();
        p.actorIndex = actorIdx->value();
        p.actorHpBelow = actorPct->value();
        p.enemyValid = enemyCheck->isChecked();
        p.enemyIndex = enemyIdx->value();
        p.enemyHpBelow = enemyPct->value();
    };
    loadPage = [this, tp, curPage, pageList, span, ceCombo, swCheck, swId,
                turnCheck, turnA, turnB, actorCheck, actorIdx, actorPct,
                enemyCheck, enemyIdx, enemyPct](int idx) {
        *curPage = idx;
        // Gem.-Events immer aktuell anbieten
        ceCombo->clear();
        ceCombo->addItem(QL("(Kein)"), 0);
        for (const auto& ce : mCEs)
            ceCombo->addItem(QL("%1: %2").arg(ce.id, 3, 10, QLatin1Char('0'))
                                 .arg(QString::fromStdString(ce.name)), ce.id);
        const bool valid = (tp->current >= 0 && (size_t)tp->current < mTroops.size() &&
                            idx >= 0 && (size_t)idx < mTroops[(size_t)tp->current].pages.size());
        if (!valid) {
            swId->setValue(1); turnA->setValue(0); turnB->setValue(0);
            actorIdx->setValue(1); actorPct->setValue(50);
            enemyIdx->setValue(1); enemyPct->setValue(50);
            swCheck->setChecked(false); turnCheck->setChecked(false);
            actorCheck->setChecked(false); enemyCheck->setChecked(false);
            span->setCurrentIndex(0);
            ceCombo->setCurrentIndex(0);
            return;
        }
        const auto& p = mTroops[(size_t)tp->current].pages[(size_t)idx];
        span->setCurrentIndex(qBound(0, p.span, 2));
        const int ci = ceCombo->findData(p.commonEventId);
        ceCombo->setCurrentIndex(ci >= 0 ? ci : 0);
        swCheck->setChecked(p.switchValid);
        swId->setValue(qBound(1, p.switchId, 9999));
        turnCheck->setChecked(p.turnValid);
        turnA->setValue(qBound(0, p.turnA, 999));
        turnB->setValue(qBound(0, p.turnB, 999));
        actorCheck->setChecked(p.actorValid);
        actorIdx->setValue(qBound(1, p.actorIndex, 8));
        actorPct->setValue(qBound(1, p.actorHpBelow, 100));
        enemyCheck->setChecked(p.enemyValid);
        enemyIdx->setValue(qBound(1, p.enemyIndex, 8));
        enemyPct->setValue(qBound(1, p.enemyHpBelow, 100));
    };
    auto refillPages = [this, tp, curPage, pageList]() {
        pageList->clear();
        *curPage = -1;
        if (tp->current < 0 || (size_t)tp->current >= mTroops.size()) return;
        const auto& tr = mTroops[(size_t)tp->current];
        for (size_t k = 0; k < tr.pages.size(); ++k)
            pageList->addItem(QL("Seite %1").arg((int)k + 1));
        if (!tr.pages.empty()) pageList->setCurrentRow(0);
    };
    connect(pageList, &QListWidget::currentRowChanged, formHost,
            [storeCurrentPage, loadPage](int row) {
                storeCurrentPage(); // alte Seite sichern
                loadPage(row);      // neue Seite laden
            });
    connect(pageAddBtn, &QPushButton::clicked, formHost,
            [this, tp, storeCurrentPage, pageList]() {
                if (tp->current < 0 || (size_t)tp->current >= mTroops.size()) return;
                auto& tr = mTroops[(size_t)tp->current];
                if (tr.pages.size() >= 12) return;
                storeCurrentPage();
                tr.pages.push_back({});
                pageList->addItem(QL("Seite %1").arg((int)tr.pages.size()));
                pageList->setCurrentRow((int)tr.pages.size() - 1);
            });
    connect(pageRemBtn, &QPushButton::clicked, formHost,
            [this, tp, storeCurrentPage, pageList]() {
                const int row = pageList->currentRow();
                if (row < 0 || tp->current < 0 || (size_t)tp->current >= mTroops.size()) return;
                auto& tr = mTroops[(size_t)tp->current];
                storeCurrentPage();
                if ((size_t)row >= tr.pages.size()) return;
                tr.pages.erase(tr.pages.begin() + (ptrdiff_t)row);
                delete pageList->takeItem(row);
                if (!tr.pages.empty())
                    pageList->setCurrentRow(row < (int)tr.pages.size() ? row : (int)tr.pages.size() - 1);
            });

    tp->count = [this]() { return (int)mTroops.size(); };
    tp->nameAt = [this](int i) {
        return IdName(i + 1, QString::fromStdString(mTroops[(size_t)i].name));
    };
    tp->setMax = [this](int n) {
        mTroops.resize((size_t)n);
        for (size_t i = 0; i < mTroops.size(); ++i) mTroops[i].id = (int)i + 1;
    };
    tp->loadForm = [this, name, memberList, enemyCombo, refillPages](int i) {
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
        refillPages(); // Kampfereignis-Seiten des neuen Trupps zeigen
    };
    tp->storeForm = [this, tp, name, memberList, storeCurrentPage](int i) {
        if ((size_t)i >= mTroops.size()) return;
        auto& tr = mTroops[(size_t)i];
        tr.name = name->text().toStdString();
        tr.members.clear();
        for (int r = 0; r < memberList->count(); ++r)
            tr.members.push_back(memberList->item(r)->data(Qt::UserRole).toInt());
        storeCurrentPage(); // offene Kampfereignis-Seite mitsichern
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
    ListTab& tabRef = addListTab(QL("Animationen"));
    ListTab* tp = &tabRef;
    auto* formHost = new QWidget(tabRef.page);
    ((QScrollArea*)tabRef.page->property("formWrap").value<QWidget*>())->setWidget(formHost);

    auto* vbox = new QVBoxLayout(formHost);
    vbox->setContentsMargins(0, 0, 0, 0);

    // ------- Kopf: Name / Grafik / Position -------
    auto* form = new QFormLayout();
    vbox->addLayout(form);
    auto* name = makeLine(formHost);
    auto* file = makeLine(formHost);
    auto* posCombo = makeCombo(formHost, {QL("Oben"), QL("Mitte"), QL("Unten")}, 2);
    form->addRow(QL("Name"), name);
    form->addRow(QL("Grafik (Graphics/Animations/)"), file);
    form->addRow(QL("Position (XP)"), posCombo);

    // ------- Frames: Navigator + Tools -------
    auto* frameRow = new QHBoxLayout();
    vbox->addLayout(frameRow);
    frameRow->addWidget(new QLabel(QL("Frame:"), formHost));
    auto* framePrevBtn = new QPushButton(QL("◀"), formHost);
    auto* frameNoLbl = new QLabel(QL("1/1"), formHost);
    auto* frameNextBtn = new QPushButton(QL("▶"), formHost);
    auto* frameAddBtn = new QPushButton(QL("+ dahinter neuer Frame"), formHost);
    auto* frameDelBtn = new QPushButton(QL("Frame löschen"), formHost);
    frameRow->addWidget(framePrevBtn);
    frameRow->addWidget(frameNoLbl);
    frameRow->addWidget(frameNextBtn);
    frameRow->addSpacing(12);
    frameRow->addWidget(frameAddBtn);
    frameRow->addWidget(frameDelBtn);
    frameRow->addStretch(1);

    // ------- Zellen-Canvas -------
    auto* canvas = new QtAnimFrameCanvas(formHost);
    vbox->addWidget(canvas);

    // ------- Zellen-Eigenschaften (ausgewaehlte Zelle) -------
    auto* cellGrp = new QGroupBox(QL("Ausgewählte Zelle"), formHost);
    auto* cform = new QFormLayout(cellGrp);
    vbox->addWidget(cellGrp);
    auto* cellIdSpin = makeSpin(0, 95, 0, cellGrp);
    auto* cellXSpin = makeSpin(-320, 320, 0, cellGrp);
    auto* cellYSpin = makeSpin(-240, 240, 0, cellGrp);
    auto* cellScaleSpin = makeSpin(1, 400, 100, cellGrp);
    auto* cellRotSpin = makeSpin(0, 360, 0, cellGrp);
    auto* cellOpSpin = makeSpin(0, 255, 255, cellGrp);
    cform->addRow(QL("Bildzelle (0-95)"), cellIdSpin);
    cform->addRow(QL("X"), cellXSpin);
    cform->addRow(QL("Y"), cellYSpin);
    cform->addRow(QL("Skalierung %"), cellScaleSpin);
    cform->addRow(QL("Rotation °"), cellRotSpin);
    cform->addRow(QL("Deckkraft"), cellOpSpin);

    // ------- Frame-Timing: SE + Flash -------
    auto* timingGrp = new QGroupBox(QL("Frame-Timing (SE / Flash)"), formHost);
    auto* tform = new QFormLayout(timingGrp);
    vbox->addWidget(timingGrp);
    auto* seName = makeLine(timingGrp);
    auto* seVol = makeSpin(0, 100, 100, timingGrp);
    auto* sePitch = makeSpin(50, 150, 100, timingGrp);
    tform->addRow(QL("SE-Datei (Audio/SE/)"), seName);
    tform->addRow(QL("SE-Lautstärke"), seVol);
    tform->addRow(QL("SE-Pitch"), sePitch);
    auto* flashScope = makeCombo(timingGrp,
        {QL("Keiner"), QL("Ziel"), QL("Bildschirm")}, 0);
    auto* flashR = makeSpin(0, 255, 255, timingGrp);
    auto* flashG = makeSpin(0, 255, 255, timingGrp);
    auto* flashB = makeSpin(0, 255, 255, timingGrp);
    auto* flashDur = makeSpin(1, 60, 5, timingGrp);
    tform->addRow(QL("Blitz-Bereich"), flashScope);
    tform->addRow(QL("Blitz R / G / B"), flashR);
    tform->addRow(QL(""), flashG);
    tform->addRow(QL(""), flashB);
    tform->addRow(QL("Blitz-Dauer (Frames)"), flashDur);

    vbox->addStretch(1);

    // ------- gemeinsamer Zugriff auf die aktuelle Animation/Frame -------
    // tp->current ist die ausgewaehlte Zeile der linken Liste
    auto curAnim = [this, tp]() -> rpg::AnimationData* {
        if (tp->current < 0 || (size_t)tp->current >= mAnimations.size()) return nullptr;
        return &mAnimations[(size_t)tp->current];
    };
    auto curFrame = [&curAnim, canvas]() -> rpg::AnimFrame* {
        auto* a = curAnim();
        if (!a || a->frames.empty()) return nullptr;
        int& fi = canvas->frameIdx;
        if (fi < 0 || fi >= (int)a->frames.size()) fi = 0;
        return &a->frames[(size_t)fi];
    };
    auto refreshFrameLabel = [canvas, frameNoLbl, this, tp]() {
        auto* a = tp->current >= 0 && (size_t)tp->current < mAnimations.size()
                      ? &mAnimations[(size_t)tp->current] : nullptr;
        const int total = a ? (int)a->frames.size() : 0;
        if (total > 0)
            frameNoLbl->setText(QStringLiteral("%1/%2").arg(canvas->frameIdx + 1).arg(total));
        else
            frameNoLbl->setText(QL("0/0"));
    };
    auto loadCellForm = [canvas, cellIdSpin, cellXSpin, cellYSpin,
                         cellScaleSpin, cellRotSpin, cellOpSpin]() {
        auto& cells = [&]() -> std::vector<rpg::AnimCell>& {
            static std::vector<rpg::AnimCell> dummy;
            auto* a = canvas->anim;
            if (!a || a->frames.empty()) return dummy;
            return a->frames[(size_t)std::max(0, std::min(canvas->frameIdx,
                (int)a->frames.size() - 1))].cells;
        }();
        int sc = canvas->selectedCell;
        if (sc < 0 || sc >= (int)cells.size()) return;
        const auto& c = cells[(size_t)sc];
        cellIdSpin->setValue(c.cellId);
        cellXSpin->setValue(c.x);
        cellYSpin->setValue(c.y);
        cellScaleSpin->setValue(c.scale);
        cellRotSpin->setValue(c.rotation);
        cellOpSpin->setValue(c.opacity);
    };

    // ------- Sheet-Datei aus dem Projekt aufloesen -------
    auto resolveSheet = [this](const std::string& fn) -> QString {
        if (fn.empty() || !mEngine) return QString();
        const std::string pp = mEngine->GetProject().GetProjectPath();
        QStringList cands = {
            QString::fromStdString(pp + "/Graphics/Animations/" + fn),
            QL("assets/Graphics/Animations/") + QString::fromStdString(fn)
        };
        QString s;
        for (const QString& c : cands)
            if (QFile::exists(c)) { s = c; break; }
        return s;
    };
    auto reloadSheet = [canvas, tp, this, resolveSheet]() {
        if (tp->current < 0 || (size_t)tp->current >= mAnimations.size()) return;
        QImage img(resolveSheet(mAnimations[(size_t)tp->current].file));
        canvas->setSheet(img);
    };

    // ------- Frame-Navigation -------
    connect(framePrevBtn, &QPushButton::clicked, formHost, [canvas, tp, this, refreshFrameLabel, loadCellForm]() {
        auto* a = tp->current >= 0 && (size_t)tp->current < mAnimations.size()
                      ? &mAnimations[(size_t)tp->current] : nullptr;
        if (!a || a->frames.empty()) return;
        canvas->frameIdx = std::max(0, canvas->frameIdx - 1);
        canvas->selectedCell = -1;
        refreshFrameLabel(); loadCellForm(); canvas->update();
    });
    connect(frameNextBtn, &QPushButton::clicked, formHost, [canvas, tp, this, refreshFrameLabel, loadCellForm]() {
        auto* a = tp->current >= 0 && (size_t)tp->current < mAnimations.size()
                      ? &mAnimations[(size_t)tp->current] : nullptr;
        if (!a || a->frames.empty()) return;
        canvas->frameIdx = std::min((int)a->frames.size() - 1, canvas->frameIdx + 1);
        canvas->selectedCell = -1;
        refreshFrameLabel(); loadCellForm(); canvas->update();
    });
    connect(frameAddBtn, &QPushButton::clicked, formHost, [canvas, curAnim, refreshFrameLabel]() {
        auto* a = curAnim();
        if (!a) return;
        if (a->frames.empty()) {
            a->frames.push_back(rpg::AnimFrame{});
            canvas->frameIdx = 0;
        } else {
            // neue Kopie des aktuellen Frames dahinter einfuegen (XP-Stil: duplizieren)
            a->frames.insert(a->frames.begin() + canvas->frameIdx + 1,
                             a->frames[(size_t)canvas->frameIdx]);
            canvas->frameIdx++;
        }
        canvas->selectedCell = -1;
        refreshFrameLabel(); canvas->update();
    });
    connect(frameDelBtn, &QPushButton::clicked, formHost, [canvas, curAnim, refreshFrameLabel]() {
        auto* a = curAnim();
        if (!a || a->frames.empty()) return;
        a->frames.erase(a->frames.begin() + canvas->frameIdx);
        if (canvas->frameIdx >= (int)a->frames.size())
            canvas->frameIdx = (int)a->frames.size() - 1;
        if (canvas->frameIdx < 0) canvas->frameIdx = 0;
        canvas->selectedCell = -1;
        refreshFrameLabel(); canvas->update();
    });

    // ------- Canvas-Klicks: Zelle anlegen/waehlen/loeschen -------
    canvas->onAddCell = [canvas, curFrame, loadCellForm](int lx, int ly) {
        auto* fr = curFrame();
        if (!fr) return;
        rpg::AnimCell c;
        c.x = lx; c.y = ly;
        fr->cells.push_back(c);
        canvas->selectedCell = (int)fr->cells.size() - 1;
        loadCellForm();
    };
    canvas->onSelectCell = [canvas, loadCellForm](int idx) {
        canvas->selectedCell = idx;
        loadCellForm();
    };
    canvas->onRemoveCell = [canvas, curFrame](int idx) {
        auto* fr = curFrame();
        if (!fr) return;
        if (idx >= 0 && idx < (int)fr->cells.size()) {
            fr->cells.erase(fr->cells.begin() + idx);
            if (canvas->selectedCell >= (int)fr->cells.size())
                canvas->selectedCell = (int)fr->cells.size() - 1;
        }
    };

    // ------- Zellen-Formular auf die ausgewaehlte Zelle anwenden -------
    auto applyCell = [canvas]() {
        auto* a = canvas->anim;
        if (!a || a->frames.empty()) return;
        auto& fr = a->frames[(size_t)std::max(0, std::min(canvas->frameIdx,
            (int)a->frames.size() - 1))];
        int sc = canvas->selectedCell;
        if (sc < 0 || sc >= (int)fr.cells.size()) return;
        // Werte werden von jedem Spin-Signal gelesen (siehe unten)
    };
    (void)applyCell;
    auto writeCell = [canvas, cellIdSpin, cellXSpin, cellYSpin,
                      cellScaleSpin, cellRotSpin, cellOpSpin]() {
        if (!canvas->anim || canvas->anim->frames.empty()) return;
        auto& fr = canvas->anim->frames[(size_t)std::max(0, std::min(canvas->frameIdx,
            (int)canvas->anim->frames.size() - 1))];
        int sc = canvas->selectedCell;
        if (sc < 0 || sc >= (int)fr.cells.size()) return;
        auto& c = fr.cells[(size_t)sc];
        c.cellId = cellIdSpin->value();
        c.x = cellXSpin->value();
        c.y = cellYSpin->value();
        c.scale = cellScaleSpin->value();
        c.rotation = cellRotSpin->value();
        c.opacity = cellOpSpin->value();
        canvas->update();
    };
    for (auto* s : {cellIdSpin, cellXSpin, cellYSpin,
                    cellScaleSpin, cellRotSpin, cellOpSpin})
        connect(s, QOverload<int>::of(&QSpinBox::valueChanged), formHost,
                [writeCell]() { writeCell(); });

    // ------- ListTab-Verdrahtung -------
    tp->count = [this]() { return (int)mAnimations.size(); };
    tp->nameAt = [this](int i) {
        return IdName(mAnimations[(size_t)i].id > 0 ? mAnimations[(size_t)i].id : (i + 1),
                      QString::fromStdString(mAnimations[(size_t)i].name));
    };
    tp->setMax = [this](int n) {
        mAnimations.resize((size_t)n);
        for (size_t i = 0; i < mAnimations.size(); ++i) {
            if (mAnimations[i].id <= 0) mAnimations[i].id = (int)i + 1;
            if (mAnimations[i].name.empty())
                mAnimations[i].name = "Animation " + std::to_string(i + 1);
            if (mAnimations[i].frames.empty())
                mAnimations[i].frames.push_back(rpg::AnimFrame{});
        }
    };
    tp->loadForm = [this, name, file, posCombo, canvas, seName, seVol, sePitch,
                    flashScope, flashR, flashG, flashB, flashDur,
                    refreshFrameLabel, reloadSheet](int i) {
        auto& a = mAnimations[(size_t)i];
        name->setText(QString::fromStdString(a.name));
        file->setText(QString::fromStdString(a.file));
        posCombo->setCurrentIndex(std::clamp(a.position, 0, 2));
        canvas->anim = &a;
        canvas->frameIdx = 0;
        canvas->selectedCell = -1;
        if (a.frames.empty()) a.frames.push_back(rpg::AnimFrame{});
        const auto& fr = a.frames[0];
        seName->setText(QString::fromStdString(fr.seName));
        seVol->setValue(fr.seVolume);
        sePitch->setValue(fr.sePitch);
        flashScope->setCurrentIndex(std::clamp(fr.flashScope, 0, 2));
        flashR->setValue(fr.flashR);
        flashG->setValue(fr.flashG);
        flashB->setValue(fr.flashB);
        flashDur->setValue(std::max(1, fr.flashDuration));
        refreshFrameLabel();
        reloadSheet();
        canvas->update();
    };
    tp->storeForm = [this, tp, name, file, posCombo](int i) {
        if ((size_t)i >= mAnimations.size()) return;
        auto& a = mAnimations[(size_t)i];
        a.name = name->text().toStdString();
        a.file = file->text().toStdString();
        a.position = posCombo->currentIndex();
        a.id = i + 1;
        if (!tp->loading && tp->list) tp->list->item(i)->setText(tp->nameAt(i));
    };

    // Frame-Timing-Felder schreiben auf den AKTIVEN Frame (sofort, nicht erst beim Tab-Wechsel)
    auto writeTiming = [canvas, seName, seVol, sePitch,
                        flashScope, flashR, flashG, flashB, flashDur]() {
        auto* a = canvas->anim;
        if (!a || a->frames.empty()) return;
        int fi = std::max(0, std::min(canvas->frameIdx, (int)a->frames.size() - 1));
        auto& fr = a->frames[(size_t)fi];
        fr.seName = seName->text().toStdString();
        fr.seVolume = seVol->value();
        fr.sePitch = sePitch->value();
        fr.flashScope = flashScope->currentIndex();
        fr.flashR = flashR->value();
        fr.flashG = flashG->value();
        fr.flashB = flashB->value();
        fr.flashDuration = flashDur->value();
    };
    connect(seName, &QLineEdit::editingFinished, formHost, [writeTiming]() { writeTiming(); });
    for (auto* s : {seVol, sePitch, flashR, flashG, flashB, flashDur})
        connect(s, QOverload<int>::of(&QSpinBox::valueChanged), formHost,
                [writeTiming]() { writeTiming(); });
    connect(flashScope, QOverload<int>::of(&QComboBox::currentIndexChanged), formHost,
            [writeTiming](int) { writeTiming(); });

    connect(file, &QLineEdit::editingFinished, formHost,
            [tp, file, reloadSheet, this]() {
        if (tp->current < 0) return;
        storeCurrent(*tp);
        reloadSheet();
    });

    rebuildList(tabRef, 0);
}

void QtDatabaseDialog::buildTilesetsTab() {
    ListTab& t = addListTab(QL("Tilesets"));
    ListTab* tp = &t;
    auto* formHost = new QWidget(t.page);
    ((QScrollArea*)t.page->property("formWrap").value<QWidget*>())->setWidget(formHost);

    auto* vbox = new QVBoxLayout(formHost);
    vbox->setContentsMargins(0, 0, 0, 0);

    auto* form = new QFormLayout();
    vbox->addLayout(form);

    auto* name = makeLine(formHost);
    auto* file = makeLine(formHost);
    form->addRow(QL("Name"), name);
    form->addRow(QL("Tileset-Grafik"), file);
    auto* note = new QLabel(QL("Datei aus dem Projektordner "
                               "textures/ oder Graphics/Tilesets/"), formHost);
    note->setStyleSheet(QL("color:#9aa;"));
    form->addRow(note);

    // ---- XP-Modus-Leiste (Durchgang | 4-Dir | Prioritaet | Busch | Tresen | Terrain)
    auto* modeRow = new QHBoxLayout();
    vbox->addLayout(modeRow);
    modeRow->addWidget(new QLabel(QL("Modus:"), formHost));
    QPushButton* modeBtns[6];
    const char* modeNames[6] = {
        "Durchgang", "4-Dir", "Priorität", "Busch", "Tresen", "Terrain-Tag"
    };
    auto* grid = new QtTilesetGridWidget(formHost);
    QPushButton* btnsCopy[6];
    for (int m = 0; m < 6; ++m) {
        modeBtns[m] = new QPushButton(QString::fromUtf8(modeNames[m]), formHost);
        modeBtns[m]->setCheckable(true);
        btnsCopy[m] = modeBtns[m];
        modeRow->addWidget(modeBtns[m]);
    }
    modeBtns[0]->setChecked(true);
    modeRow->addStretch(1);
    for (int m = 0; m < 6; ++m) {
        connect(modeBtns[m], &QPushButton::clicked, formHost, [grid, btnsCopy, m]() {
            for (int k = 0; k < 6; ++k) btnsCopy[k]->setChecked(k == m);
            grid->setMode((QtTilesetGridWidget::Mode)m);
        });
    }

    // ---- XP-Flag-Raster (Paket 2)
    grid->setMinimumHeight(300);
    vbox->addWidget(grid, 1);

    auto* hint = new QLabel(QL("Linksklick = Flag ändern · Rechtsklick = zurücksetzen\n"
                               "Durchgang: grüner Kreis = frei, rotes X = blockiert\n"
                               "Busch = B · Tresen = C · Priorität/Terrain = Zahl"),
                              formHost);
    hint->setStyleSheet(QL("color:#9aa;"));
    vbox->addWidget(hint);

    // ---- Grafik-Zuordnungen (XP)
    auto* grp = new QGroupBox(QL("Grafik-Zuordnung (XP)"), formHost);
    auto* gform = new QFormLayout(grp);
    vbox->addWidget(grp);
    QLineEdit* autotile[7];
    for (int a = 0; a < 7; ++a) {
        autotile[a] = makeLine(grp);
        gform->addRow(QL("Autotile %1").arg(a + 1), autotile[a]);
    }
    auto* panorama = makeLine(grp);
    auto* fog = makeLine(grp);
    auto* battleback = makeLine(grp);
    gform->addRow(QL("Panorama"), panorama);
    gform->addRow(QL("Nebel"), fog);
    gform->addRow(QL("Kampfhintergrund"), battleback);

    // loest das Tileset-Bild aus dem Projekt auf
    auto resolveImage = [this](const std::string& fn) -> QString {
        if (fn.empty() || !mEngine) return QString();
        const std::string pp = mEngine->GetProject().GetProjectPath();
        QStringList cands = {
            QString::fromStdString(pp + "/textures/" + fn),
            QString::fromStdString(pp + "/Graphics/Tilesets/" + fn),
            QL("assets/textures/") + QString::fromStdString(fn)
        };
        QString s;
        for (int ci = 0; ci < cands.size(); ++ci)
            if (QFile::exists(cands[ci])) { s = cands[ci]; break; }
        return s;
    };

    tp->count = [this]() { return (int)mTilesets.size(); };
    tp->nameAt = [this](int i) {
        return IdName(i + 1, QString::fromStdString(mTilesets[(size_t)i].name));
    };
    tp->setMax = [this](int n) {
        mTilesets.resize((size_t)n);
        for (size_t i = 0; i < mTilesets.size(); ++i) mTilesets[i].id = (int)i + 1;
    };
    tp->loadForm = [this, name, file, autotile, panorama, fog, battleback,
                    grid, resolveImage](int i) {
        auto& ts = mTilesets[(size_t)i];
        name->setText(QString::fromStdString(ts.name));
        file->setText(QString::fromStdString(ts.tilesetName));
        for (int a = 0; a < 7; ++a)
            autotile[a]->setText(QString::fromStdString(ts.autotileNames[a]));
        panorama->setText(QString::fromStdString(ts.panoramaName));
        fog->setText(QString::fromStdString(ts.fogName));
        battleback->setText(QString::fromStdString(ts.battlebackName));
        grid->setData(&ts);
        grid->loadImage(resolveImage(ts.tilesetName));
    };
    tp->storeForm = [this, tp, name, file, autotile, panorama, fog, battleback](int i) {
        if ((size_t)i >= mTilesets.size()) return;
        auto& ts = mTilesets[(size_t)i];
        ts.name = name->text().toStdString();
        ts.tilesetName = file->text().toStdString();
        for (int a = 0; a < 7; ++a)
            ts.autotileNames[a] = autotile[a]->text().toStdString();
        ts.panoramaName = panorama->text().toStdString();
        ts.fogName = fog->text().toStdString();
        ts.battlebackName = battleback->text().toStdString();
        if (!tp->loading && tp->list) tp->list->item(i)->setText(tp->nameAt(i));
    };
    // Aendern der Grafik-Datei -> Raster neu laden
    connect(file, &QLineEdit::editingFinished, formHost, [tp, file, grid, resolveImage]() {
        if (tp->current < 0) return;
        grid->loadImage(resolveImage(file->text().toStdString()));
    });
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
    db.AnimationSet() = mAnimations;
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
    // „Kampftest"-Knopf im Trupps-Tab -> Editor startet Player-exe.
    // (bewusst hier verdrahtet: Der Dialog kennt das Fenster nicht direkt;
    // der Cast schlaegt fehl = Knopf meldet einfach nichts)
    QObject::connect(&dlg, &QtDatabaseDialog::battleTestRequested, parent,
        [parent](int troopId) {
            if (auto* w = qobject_cast<QtEditorWindow*>(parent))
                w->StartBattleTest(troopId);
        });
    return dlg.exec() == QDialog::Accepted;
}

} // namespace qt_editor
