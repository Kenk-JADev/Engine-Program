#include "QtEventCommandEditDialog.h"

#include "rpgmaker3d/Database.h"
#include "rpgmaker3d/EventSystem.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QSpinBox>
#include <QStackedWidget>
#include <QVBoxLayout>

namespace qt_editor {

namespace {

#ifndef QL
#define QL(x) QStringLiteral(x)
#endif

constexpr const char* kKeyProp = "argKey";
constexpr const char* kTypeProp = "argType";

QSpinBox* makeSpin(int mn, int mx, int value, QWidget* parent) {
    auto* s = new QSpinBox(parent);
    s->setRange(mn, mx);
    s->setValue(value);
    return s;
}

/// Datenbank-ComboBox "0001: Name" (Ids aus Datenbank, sonst generisch).
template <typename T>
void fillDbCombo(QComboBox* combo, const std::vector<T>& rows, int maxGeneric) {
    int count = static_cast<int>(rows.size());
    int n = count > 0 ? count : maxGeneric;
    for (int i = 1; i <= n; ++i) {
        QString name;
        if (i <= count) name = QString::fromStdString(rows[(size_t)(i - 1)].name);
        combo->addItem(QL("%1: %2").arg(i, 4, 10, QLatin1Char('0')).arg(name), i);
    }
}

void fillSwitchCombo(QComboBox* combo) {
    const auto& names = rpg::Database::Get().System().switches;
    int n = names.empty() ? 100 : static_cast<int>(names.size());
    for (int i = 1; i <= n; ++i) {
        QString name = (i <= (int)names.size()) ? QString::fromStdString(names[(size_t)(i - 1)])
                                                : QString();
        combo->addItem(QL("%1: %2").arg(i, 4, 10, QLatin1Char('0')).arg(name), i);
    }
}

void fillVariableCombo(QComboBox* combo) {
    const auto& names = rpg::Database::Get().System().variables;
    int n = names.empty() ? 100 : static_cast<int>(names.size());
    for (int i = 1; i <= n; ++i) {
        QString name = (i <= (int)names.size()) ? QString::fromStdString(names[(size_t)(i - 1)])
                                                : QString();
        combo->addItem(QL("%1: %2").arg(i, 4, 10, QLatin1Char('0')).arg(name), i);
    }
}

void setComboToData(QComboBox* combo, int dataValue) {
    int idx = combo->findData(dataValue);
    combo->setCurrentIndex(idx >= 0 ? idx : 0);
}

/// XP-Bedingung "ist AN/AUS": AN = 0, AUS = 1 (XP-Kodierung!)
QComboBox* makeOnOffCombo(QWidget* parent) {
    auto* c = new QComboBox(parent);
    c->addItem(QL("AN"), 0);
    c->addItem(QL("AUS"), 1);
    return c;
}

QComboBox* makeDirectionCombo(QWidget* parent) {
    auto* c = new QComboBox(parent);
    c->addItem(QL("Unten"), rpg::DIR_DOWN);
    c->addItem(QL("Links"), rpg::DIR_LEFT);
    c->addItem(QL("Rechts"), rpg::DIR_RIGHT);
    c->addItem(QL("Oben"), rpg::DIR_UP);
    return c;
}

int comboInt(const QComboBox* c, int def = 0) {
    return c ? c->currentData().toInt() : def;
}

} // namespace

// ---------------------------------------------------------------------------

QtEventCommandEditDialog::QtEventCommandEditDialog(const rpg::EventCommand& cmd,
                                                   int eventContext, QWidget* parent)
    : QDialog(parent), mOriginal(cmd), mResult(cmd), mEventContext(eventContext) {
    setModal(true);
    setMinimumWidth(460);

    const CommandSpec* spec = FindCommandSpec(cmd.code);
    setWindowTitle(spec ? spec->label.left(spec->label.indexOf(QLatin1String("...")) > 0
                                        ? spec->label.indexOf(QLatin1String("..."))
                                        : spec->label.length())
                        : QL("Befehl bearbeiten"));

    auto* root = new QVBoxLayout(this);

    if (cmd.code == rpg::EventCommandCode::ConditionalBranch) {
        buildConditionalBranch();
    } else if (spec) {
        if (!spec->description.isEmpty()) {
            auto* help = new QLabel(spec->description, this);
            help->setWordWrap(true);
            root->addWidget(help);
        }
        auto* formBox = new QGroupBox(spec->label, this);
        auto* form = new QFormLayout(formBox);
        for (const ArgSpec& arg : spec->args) {
            QWidget* w = buildArgRow(arg, form);
            if (w) mFields.insert(arg.key, w);
        }
        root->addWidget(formBox);
    } else {
        root->addWidget(new QLabel(QL("Dieser Befehl hat keine bearbeitbaren Parameter."), this));
    }

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel
                                         | QDialogButtonBox::Apply, this);
    buttons->button(QDialogButtonBox::Ok)->setText(QL("OK"));
    buttons->button(QDialogButtonBox::Cancel)->setText(QL("Abbrechen"));
    buttons->button(QDialogButtonBox::Apply)->setText(QL("Anwenden"));
    connect(buttons->button(QDialogButtonBox::Ok), &QPushButton::clicked,
            this, &QtEventCommandEditDialog::onOk);
    connect(buttons->button(QDialogButtonBox::Cancel), &QPushButton::clicked,
            this, &QDialog::reject);
    // Anwenden = sammeln, aber Dialog offen lassen
    connect(buttons->button(QDialogButtonBox::Apply), &QPushButton::clicked, this, [this]() {
        if (mOriginal.code == rpg::EventCommandCode::ConditionalBranch) {
            collectConditionalBranch();
        } else if (const CommandSpec* s = FindCommandSpec(mOriginal.code)) {
            collectGeneric(*s);
        }
    });
    root->addWidget(buttons);
}

// ---------------------------------------------------------------------------
// Generische Zeilen
// ---------------------------------------------------------------------------

QWidget* QtEventCommandEditDialog::buildArgRow(const ArgSpec& spec, QFormLayout* form) {
    QWidget* w = nullptr;
    switch (spec.type) {
        case ArgSpec::Type::Int:
        case ArgSpec::Type::SignedInt:
        case ArgSpec::Type::Digits: {
            int cur = GetCommandArgInt(mOriginal, spec.key, spec.def);
            w = makeSpin(spec.min, spec.max, cur, this);
            break;
        }
        case ArgSpec::Type::Bool: {
            auto* c = new QCheckBox(this);
            c->setChecked(GetCommandArgInt(mOriginal, spec.key, spec.def) != 0);
            w = c;
            break;
        }
        case ArgSpec::Type::Choice: {
            auto* c = new QComboBox(this);
            for (int i = 0; i < spec.options.size(); ++i)
                c->addItem(spec.options[i], spec.optionToValue(i));
            int cur = GetCommandArgInt(mOriginal, spec.key, spec.optionToValue(spec.def));
            setComboToData(c, cur);
            w = c;
            break;
        }
        case ArgSpec::Type::SwitchId: {
            auto* c = new QComboBox(this);
            fillSwitchCombo(c);
            setComboToData(c, GetCommandArgInt(mOriginal, spec.key, spec.def));
            w = c;
            break;
        }
        case ArgSpec::Type::VariableId: {
            auto* c = new QComboBox(this);
            fillVariableCombo(c);
            setComboToData(c, GetCommandArgInt(mOriginal, spec.key, spec.def));
            w = c;
            break;
        }
        case ArgSpec::Type::ItemId:
        case ArgSpec::Type::WeaponId:
        case ArgSpec::Type::ArmorId:
        case ArgSpec::Type::ActorId:
        case ArgSpec::Type::TroopId:
        case ArgSpec::Type::CommonEventId: {
            auto& db = rpg::Database::Get();
            auto* c = new QComboBox(this);
            if (spec.min <= 0 && spec.type == ArgSpec::Type::ActorId)
                c->addItem(QL("0: (ganze Gruppe)"), 0);
            else if (spec.min <= 0)
                c->addItem(QL("0: (keins)"), 0);
            switch (spec.type) {
                case ArgSpec::Type::ItemId: fillDbCombo(c, db.Items(), 50); break;
                case ArgSpec::Type::WeaponId: fillDbCombo(c, db.Weapons(), 50); break;
                case ArgSpec::Type::ArmorId: fillDbCombo(c, db.Armors(), 50); break;
                case ArgSpec::Type::ActorId: fillDbCombo(c, db.Actors(), 12); break;
                case ArgSpec::Type::TroopId: fillDbCombo(c, db.Troops(), 20); break;
                case ArgSpec::Type::CommonEventId: {
                    const auto& ces = rpg::EventSystem::Get().GetCommonEvents();
                    if (!ces.empty()) {
                        for (const auto& ce : ces)
                            c->addItem(QL("%1: %2").arg(ce.id, 4, 10, QLatin1Char('0'))
                                           .arg(QString::fromStdString(ce.name)), ce.id);
                    } else {
                        for (int i = 1; i <= 20; ++i)
                            c->addItem(QL("%1").arg(i, 4, 10, QLatin1Char('0')), i);
                    }
                    break;
                }
                default: break;
            }
            setComboToData(c, GetCommandArgInt(mOriginal, spec.key, spec.def));
            w = c;
            break;
        }
        case ArgSpec::Type::SelfSwitchChar: {
            auto* c = new QComboBox(this);
            for (const char* ch : {"A", "B", "C", "D"})
                c->addItem(QLatin1String(ch), QLatin1String(ch));
            QString cur = GetCommandArgText(mOriginal, spec.key);
            if (cur.isEmpty()) cur = spec.defText.isEmpty() ? QL("A") : spec.defText;
            int idx = c->findData(cur.left(1));
            c->setCurrentIndex(idx >= 0 ? idx : 0);
            w = c;
            break;
        }
        case ArgSpec::Type::Text:
        case ArgSpec::Type::FileName:
        case ArgSpec::Type::Route: {
            auto* e = new QLineEdit(this);
            QString cur = GetCommandArgText(mOriginal, spec.key);
            if (cur.isEmpty()) cur = spec.defText;
            e->setText(cur);
            if (!spec.hint.isEmpty()) e->setPlaceholderText(spec.hint);
            w = e;
            break;
        }
        case ArgSpec::Type::MultiText: {
            auto* e = new QPlainTextEdit(this);
            e->setPlainText(GetCommandArgText(mOriginal, spec.key));
            e->setMinimumHeight(72);
            e->setMaximumHeight(140);
            if (!spec.hint.isEmpty()) e->setPlaceholderText(spec.hint);
            w = e;
            break;
        }
    }
    if (w) {
        w->setProperty(kKeyProp, spec.key);
        w->setProperty(kTypeProp, static_cast<int>(spec.type));
        QString label = spec.label;
        if (!spec.hint.isEmpty()) {
            w->setToolTip(spec.hint);
            label += QL("  (?)");
        }
        form->addRow(label, w);
    }
    return w;
}

void QtEventCommandEditDialog::collectGeneric(const CommandSpec& spec) {
    mResult = mOriginal;
    for (const ArgSpec& arg : spec.args) {
        QWidget* w = mFields.value(arg.key, nullptr);
        if (!w) continue;
        switch (arg.type) {
            case ArgSpec::Type::Int:
            case ArgSpec::Type::SignedInt:
            case ArgSpec::Type::Digits: {
                if (auto* s = qobject_cast<QSpinBox*>(w))
                    SetCommandArgInt(mResult, arg.key, s->value());
                break;
            }
            case ArgSpec::Type::Bool: {
                if (auto* c = qobject_cast<QCheckBox*>(w))
                    SetCommandArgInt(mResult, arg.key, c->isChecked() ? 1 : 0);
                break;
            }
            case ArgSpec::Type::Choice:
            case ArgSpec::Type::SwitchId:
            case ArgSpec::Type::VariableId:
            case ArgSpec::Type::ItemId:
            case ArgSpec::Type::WeaponId:
            case ArgSpec::Type::ArmorId:
            case ArgSpec::Type::ActorId:
            case ArgSpec::Type::TroopId:
            case ArgSpec::Type::CommonEventId: {
                if (auto* c = qobject_cast<QComboBox*>(w))
                    SetCommandArgInt(mResult, arg.key, c->currentData().toInt());
                break;
            }
            case ArgSpec::Type::SelfSwitchChar: {
                if (auto* c = qobject_cast<QComboBox*>(w))
                    SetCommandArgText(mResult, arg.key, c->currentData().toString());
                break;
            }
            case ArgSpec::Type::Text:
            case ArgSpec::Type::FileName:
            case ArgSpec::Type::Route: {
                if (auto* e = qobject_cast<QLineEdit*>(w))
                    SetCommandArgText(mResult, arg.key, e->text());
                break;
            }
            case ArgSpec::Type::MultiText: {
                if (auto* e = qobject_cast<QPlainTextEdit*>(w))
                    SetCommandArgText(mResult, arg.key, e->toPlainText());
                break;
            }
        }
    }
}

// ---------------------------------------------------------------------------
// Bedingung (111): 13 Typen, XP-Kodierung
// ---------------------------------------------------------------------------

void QtEventCommandEditDialog::buildConditionalBranch() {
    auto* root = qobject_cast<QVBoxLayout*>(layout());
    (void)root;

    const rpg::EventCommand& cmd = mOriginal;

    static const QStringList kTypes = {
        QL("Schalter"), QL("Variable"), QL("Selbstschalter"), QL("Timer"),
        QL("Akteur"), QL("Gegner"), QL("Event"), QL("Geld"),
        QL("Gegenstand"), QL("Waffe"), QL("Rüstung"), QL("Taste"), QL("Skript")
    };

    auto* headForm = new QFormLayout();
    mBranchType = new QComboBox(this);
    mBranchType->addItems(kTypes);
    int typ = (cmd.param1 >= 0 && cmd.param1 <= 12) ? cmd.param1 : 0;
    mBranchType->setCurrentIndex(typ);
    headForm->addRow(QL("Bedingungsart"), mBranchType);
    if (auto* v = qobject_cast<QVBoxLayout*>(layout()))
        v->addLayout(headForm);
    connect(mBranchType, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &QtEventCommandEditDialog::onBranchTypeChanged);

    mBranchStack = new QStackedWidget(this);
    auto& db = rpg::Database::Get();

    // -- 0: Schalter -------------------------------------------------------
    {
        auto* page = new QWidget(this);
        auto* f = new QFormLayout(page);
        auto* sw = new QComboBox(page);
        sw->setObjectName(QL("sw"));
        fillSwitchCombo(sw);
        setComboToData(sw, cmd.param2 > 0 ? cmd.param2 : 1);
        auto* onoff = makeOnOffCombo(page);
        onoff->setObjectName(QL("onoff"));
        setComboToData(onoff, cmd.param3 == 1 ? 1 : 0);
        f->addRow(QL("Schalter"), sw);
        f->addRow(QL("ist"), onoff);
        mBranchStack->addWidget(page);
    }
    // -- 1: Variable -------------------------------------------------------
    {
        auto* page = new QWidget(this);
        auto* f = new QFormLayout(page);
        auto* var = new QComboBox(page);
        var->setObjectName(QL("var"));
        fillVariableCombo(var);
        setComboToData(var, cmd.param2 > 0 ? cmd.param2 : 1);

        auto* op = new QComboBox(page);
        op->setObjectName(QL("op"));
        for (const char* o : {"==", ">=", "<=", ">", "<", "!="})
            op->addItem(QLatin1String(o));
        op->setCurrentIndex(cmd.param3 >= 0 && cmd.param3 <= 5 ? cmd.param3 : 0);

        auto* kind = new QComboBox(page);
        kind->setObjectName(QL("kind"));
        kind->addItem(QL("Konstante"), 0);
        kind->addItem(QL("Aus Variable"), 1);
        kind->addItem(QL("Zufall (Min..Max)"), 2);
        int kindVal = GetCommandArgInt(cmd, QL("a0"), 0);
        setComboToData(kind, kindVal);

        mVarOperandStack = new QStackedWidget(page);
        auto* constSpin = makeSpin(-999999, 999999, GetCommandArgInt(cmd, QL("a1"), 1), page);
        constSpin->setObjectName(QL("constV"));
        mVarOperandStack->addWidget(constSpin);
        auto* var2 = new QComboBox(page);
        var2->setObjectName(QL("varV"));
        fillVariableCombo(var2);
        setComboToData(var2, GetCommandArgInt(cmd, QL("a1"), 1));
        mVarOperandStack->addWidget(var2);
        auto* randW = new QWidget(page);
        auto* randLay = new QHBoxLayout(randW);
        randLay->setContentsMargins(0, 0, 0, 0);
        auto* mn = makeSpin(-999999, 999999, GetCommandArgInt(cmd, QL("a1"), 0), randW);
        mn->setObjectName(QL("minV"));
        auto* mx = makeSpin(-999999, 999999, GetCommandArgInt(cmd, QL("a2"), 10), randW);
        mx->setObjectName(QL("maxV"));
        randLay->addWidget(new QLabel(QL("Min"), randW));
        randLay->addWidget(mn, 1);
        randLay->addWidget(new QLabel(QL("Max"), randW));
        randLay->addWidget(mx, 1);
        mVarOperandStack->addWidget(randW);
        onVarOperandKindChanged(kindVal);

        connect(kind, QOverload<int>::of(&QComboBox::currentIndexChanged),
                this, &QtEventCommandEditDialog::onVarOperandKindChanged);

        f->addRow(QL("Variable"), var);
        f->addRow(QL("Vergleich"), op);
        f->addRow(QL("Wert-Art"), kind);
        f->addRow(QL("Wert"), mVarOperandStack);
        mBranchStack->addWidget(page);
    }
    // -- 2: Selbstschalter --------------------------------------------------
    {
        auto* page = new QWidget(this);
        auto* f = new QFormLayout(page);
        auto* ch = new QComboBox(page);
        ch->setObjectName(QL("selfch"));
        for (const char* c : {"A", "B", "C", "D"})
            ch->addItem(QLatin1String(c));
        QString cur = QString::fromStdString(cmd.text);
        int ci = cur.isEmpty() ? 0 : ch->findText(cur.left(1));
        ch->setCurrentIndex(ci >= 0 ? ci : 0);
        auto* onoff = makeOnOffCombo(page);
        onoff->setObjectName(QL("onoff"));
        setComboToData(onoff, cmd.param3 == 1 ? 1 : 0);
        f->addRow(QL("Selbstschalter"), ch);
        f->addRow(QL("ist"), onoff);
        mBranchStack->addWidget(page);
    }
    // -- 3: Timer ------------------------------------------------------------
    {
        auto* page = new QWidget(this);
        auto* f = new QFormLayout(page);
        auto* sec = makeSpin(0, 99999, cmd.param2, page);
        sec->setObjectName(QL("sec"));
        auto* cmp = new QComboBox(page);
        cmp->setObjectName(QL("cmp"));
        cmp->addItem(QL("oder mehr"), 0);
        cmp->addItem(QL("oder weniger"), 1);
        setComboToData(cmp, cmd.param3);
        f->addRow(QL("Sekunden"), sec);
        f->addRow(QL("Bedingung"), cmp);
        mBranchStack->addWidget(page);
    }
    // -- 4: Akteur -----------------------------------------------------------
    {
        auto* page = new QWidget(this);
        auto* f = new QFormLayout(page);
        auto* actor = new QComboBox(page);
        actor->setObjectName(QL("actor"));
        fillDbCombo(actor, db.Actors(), 12);
        setComboToData(actor, cmd.param2 > 0 ? cmd.param2 : 1);

        auto* kind = new QComboBox(page);
        kind->setObjectName(QL("akind"));
        for (const char* k : {"ist in der Gruppe", "Name ist", "Fertigkeit",
                              "Waffe angelegt", "Rüstung angelegt", "hat Status"})
            kind->addItem(QLatin1String(k));
        int kindIdx = (cmd.param3 >= 0 && cmd.param3 <= 5) ? cmd.param3 : 0;
        kind->setCurrentIndex(kindIdx);

        mActorValueStack = new QStackedWidget(page);
        mActorValueStack->addWidget(new QLabel(QL("(kein weiterer Wert)"), page)); // 0 Party
        auto* nameEdit = new QLineEdit(page);                                      // 1 Name
        nameEdit->setObjectName(QL("aname"));
        nameEdit->setText(GetCommandArgText(cmd, QL("a0")));
        mActorValueStack->addWidget(nameEdit);
        auto* skillSpin = makeSpin(1, 9999, GetCommandArgInt(cmd, QL("a0"), 1), page); // 2
        skillSpin->setObjectName(QL("askill"));
        mActorValueStack->addWidget(skillSpin);
        auto* wpn = new QComboBox(page);                                             // 3
        wpn->setObjectName(QL("aweapon"));
        fillDbCombo(wpn, db.Weapons(), 50);
        setComboToData(wpn, GetCommandArgInt(cmd, QL("a0"), 1));
        mActorValueStack->addWidget(wpn);
        auto* arm = new QComboBox(page);                                             // 4
        arm->setObjectName(QL("aarmor"));
        fillDbCombo(arm, db.Armors(), 50);
        setComboToData(arm, GetCommandArgInt(cmd, QL("a0"), 1));
        mActorValueStack->addWidget(arm);
        auto* stateSpin = makeSpin(1, 999, GetCommandArgInt(cmd, QL("a0"), 1), page);  // 5
        stateSpin->setObjectName(QL("astate"));
        mActorValueStack->addWidget(stateSpin);

        connect(kind, QOverload<int>::of(&QComboBox::currentIndexChanged),
                this, &QtEventCommandEditDialog::onActorKindChanged);
        onActorKindChanged(kindIdx);

        f->addRow(QL("Akteur"), actor);
        f->addRow(QL("Bedingung"), kind);
        f->addRow(QL("Wert"), mActorValueStack);
        mBranchStack->addWidget(page);
    }
    // -- 5: Gegner ------------------------------------------------------------
    {
        auto* page = new QWidget(this);
        auto* f = new QFormLayout(page);
        auto* idx = makeSpin(1, 8, cmd.param2 > 0 ? cmd.param2 : 1, page);
        idx->setObjectName(QL("eidx"));
        auto* what = new QComboBox(page);
        what->setObjectName(QL("ewhat"));
        what->addItem(QL("ist erschienen"), 0);
        what->addItem(QL("hat Status"), 1);
        setComboToData(what, cmd.param3);
        f->addRow(QL("Gegner-Nr."), idx);
        f->addRow(QL("Bedingung"), what);
        mBranchStack->addWidget(page);
    }
    // -- 6: Event-Richtung -----------------------------------------------------
    {
        auto* page = new QWidget(this);
        auto* f = new QFormLayout(page);
        auto* eid = makeSpin(0, 9999, cmd.param2, page);
        eid->setObjectName(QL("eid"));
        eid->setSpecialValueText(QL("Dieses Event"));
        auto* dir = makeDirectionCombo(page);
        dir->setObjectName(QL("dir"));
        setComboToData(dir, cmd.param3 >= 2 ? cmd.param3 : rpg::DIR_DOWN);
        f->addRow(QL("Event"), eid);
        f->addRow(QL("blickt nach"), dir);
        mBranchStack->addWidget(page);
    }
    // -- 7: Geld --------------------------------------------------------------
    {
        auto* page = new QWidget(this);
        auto* f = new QFormLayout(page);
        auto* amount = makeSpin(0, 99999999, cmd.param2, page);
        amount->setObjectName(QL("gold"));
        auto* cmp = new QComboBox(page);
        cmp->setObjectName(QL("cmp"));
        cmp->addItem(QL("oder mehr"), 0);
        cmp->addItem(QL("oder weniger"), 1);
        setComboToData(cmp, cmd.param3);
        f->addRow(QL("Betrag"), amount);
        f->addRow(QL("Bedingung"), cmp);
        mBranchStack->addWidget(page);
    }
    // -- 8/9/10: Gegenstand / Waffe / Rüstung --------------------------------
    {
        struct Def { const char* obj; const char* row; int kind; };
        const Def defs[3] = {
            {"item", "Gegenstand", 8}, {"wpn", "Waffe", 9}, {"arm", "Rüstung", 10}
        };
        for (const Def& d : defs) {
            auto* page = new QWidget(this);
            auto* f = new QFormLayout(page);
            auto* combo = new QComboBox(page);
            combo->setObjectName(QLatin1String(d.obj));
            if (d.kind == 8) fillDbCombo(combo, db.Items(), 50);
            else if (d.kind == 9) fillDbCombo(combo, db.Weapons(), 50);
            else fillDbCombo(combo, db.Armors(), 50);
            setComboToData(combo, cmd.param2 > 0 ? cmd.param2 : 1);
            f->addRow(QLatin1String(d.row), combo);
            mBranchStack->addWidget(page);
        }
    }
    // -- 11: Taste -------------------------------------------------------------
    {
        auto* page = new QWidget(this);
        auto* f = new QFormLayout(page);
        auto* btn = new QComboBox(page);
        btn->setObjectName(QL("btn"));
        const QStringList labels = XpButtonLabels();
        for (int i = 0; i < labels.size(); ++i)
            btn->addItem(labels[i], XpButtonCodeAt(i));
        setComboToData(btn, cmd.param2 > 0 ? cmd.param2 : 13);
        f->addRow(QL("Taste"), btn);
        mBranchStack->addWidget(page);
    }
    // -- 12: Skript -------------------------------------------------------------
    {
        auto* page = new QWidget(this);
        auto* f = new QFormLayout(page);
        auto* edit = new QPlainTextEdit(page);
        edit->setObjectName(QL("script"));
        edit->setPlainText(QString::fromStdString(cmd.text));
        edit->setMaximumHeight(80);
        f->addRow(QL("Ruby-Ausdruck"), edit);
        mBranchStack->addWidget(page);
    }

    layout()->addWidget(mBranchStack);
    onBranchTypeChanged(typ);

    mBranchElse = new QCheckBox(QL("Sonst-Zweig anlegen (\"Else\")"), this);
    mBranchElse->setChecked(GetCommandArgInt(cmd, QL("a5"), 0) == 1);
    layout()->addWidget(mBranchElse);
}

void QtEventCommandEditDialog::onBranchTypeChanged(int index) {
    if (mBranchStack && index >= 0 && index < mBranchStack->count())
        mBranchStack->setCurrentIndex(index);
}

void QtEventCommandEditDialog::onActorKindChanged(int index) {
    if (mActorValueStack && index >= 0 && index < mActorValueStack->count())
        mActorValueStack->setCurrentIndex(index);
}

void QtEventCommandEditDialog::onVarOperandKindChanged(int index) {
    if (mVarOperandStack && index >= 0 && index < mVarOperandStack->count())
        mVarOperandStack->setCurrentIndex(index);
}

namespace {
template <typename T>
T* findChildByName(QWidget* root, const QString& name) {
    return root->findChild<T*>(name);
}
} // namespace

void QtEventCommandEditDialog::collectConditionalBranch() {
    mResult = mOriginal;
    mResult.parameters.clear();
    mResult.text.clear();
    const int typ = mBranchType ? mBranchType->currentIndex() : 0;
    mResult.param1 = typ;
    mResult.param2 = 0;
    mResult.param3 = 0;

    QWidget* page = mBranchStack ? mBranchStack->widget(typ) : nullptr;
    if (!page) return;

    switch (typ) {
        case 0: { // Schalter
            mResult.param2 = comboInt(findChildByName<QComboBox>(page, QL("sw")), 1);
            mResult.param3 = comboInt(findChildByName<QComboBox>(page, QL("onoff")), 0);
            break;
        }
        case 1: { // Variable
            mResult.param2 = comboInt(findChildByName<QComboBox>(page, QL("var")), 1);
            if (auto* op = findChildByName<QComboBox>(page, QL("op")))
                mResult.param3 = op->currentIndex();
            const int kind = comboInt(findChildByName<QComboBox>(page, QL("kind")), 0);
            SetCommandArgInt(mResult, QL("a0"), kind);
            if (kind == 0) {
                SetCommandArgInt(mResult, QL("a1"),
                    findChildByName<QSpinBox>(page, QL("constV"))->value());
            } else if (kind == 1) {
                SetCommandArgInt(mResult, QL("a1"),
                    comboInt(findChildByName<QComboBox>(page, QL("varV")), 1));
            } else {
                SetCommandArgInt(mResult, QL("a1"),
                    findChildByName<QSpinBox>(page, QL("minV"))->value());
                SetCommandArgInt(mResult, QL("a2"),
                    findChildByName<QSpinBox>(page, QL("maxV"))->value());
            }
            break;
        }
        case 2: { // Selbstschalter
            if (auto* ch = findChildByName<QComboBox>(page, QL("selfch")))
                mResult.text = ch->currentText().toStdString();
            mResult.param3 = comboInt(findChildByName<QComboBox>(page, QL("onoff")), 0);
            break;
        }
        case 3: { // Timer
            mResult.param2 = findChildByName<QSpinBox>(page, QL("sec"))->value();
            mResult.param3 = comboInt(findChildByName<QComboBox>(page, QL("cmp")), 0);
            break;
        }
        case 4: { // Akteur
            mResult.param2 = comboInt(findChildByName<QComboBox>(page, QL("actor")), 1);
            if (auto* kind = findChildByName<QComboBox>(page, QL("akind"))) {
                mResult.param3 = kind->currentIndex();
                switch (kind->currentIndex()) {
                    case 0: break; // Party
                    case 1:
                        if (auto* e = findChildByName<QLineEdit>(page, QL("aname")))
                            SetCommandArgText(mResult, QL("a0"), e->text());
                        break;
                    case 2:
                        SetCommandArgInt(mResult, QL("a0"),
                            findChildByName<QSpinBox>(page, QL("askill"))->value());
                        break;
                    case 3:
                        SetCommandArgInt(mResult, QL("a0"),
                            comboInt(findChildByName<QComboBox>(page, QL("aweapon")), 1));
                        break;
                    case 4:
                        SetCommandArgInt(mResult, QL("a0"),
                            comboInt(findChildByName<QComboBox>(page, QL("aarmor")), 1));
                        break;
                    case 5:
                        SetCommandArgInt(mResult, QL("a0"),
                            findChildByName<QSpinBox>(page, QL("astate"))->value());
                        break;
                }
            }
            break;
        }
        case 5: { // Gegner
            mResult.param2 = findChildByName<QSpinBox>(page, QL("eidx"))->value();
            mResult.param3 = comboInt(findChildByName<QComboBox>(page, QL("ewhat")), 0);
            break;
        }
        case 6: { // Event-Richtung
            mResult.param2 = findChildByName<QSpinBox>(page, QL("eid"))->value();
            mResult.param3 = comboInt(findChildByName<QComboBox>(page, QL("dir")), rpg::DIR_DOWN);
            break;
        }
        case 7: { // Geld
            mResult.param2 = findChildByName<QSpinBox>(page, QL("gold"))->value();
            mResult.param3 = comboInt(findChildByName<QComboBox>(page, QL("cmp")), 0);
            break;
        }
        case 8:
        case 9:
        case 10: { // Item/Waffe/Rüstung
            const char* obj = typ == 8 ? "item" : (typ == 9 ? "wpn" : "arm");
            mResult.param2 = comboInt(findChildByName<QComboBox>(page, QLatin1String(obj)), 1);
            break;
        }
        case 11: { // Taste
            mResult.param2 = comboInt(findChildByName<QComboBox>(page, QL("btn")), 13);
            break;
        }
        case 12: { // Skript
            if (auto* e = findChildByName<QPlainTextEdit>(page, QL("script")))
                mResult.text = e->toPlainText().toStdString();
            break;
        }
    }

    // Sonst-Zweig-Merker (transient, von BuildCommandBlock gelesen)
    SetCommandArgInt(mResult, QL("a5"), (mBranchElse && mBranchElse->isChecked()) ? 1 : 0);
}

// ---------------------------------------------------------------------------

void QtEventCommandEditDialog::onOk() {
    if (mOriginal.code == rpg::EventCommandCode::ConditionalBranch) {
        collectConditionalBranch();
    } else if (const CommandSpec* spec = FindCommandSpec(mOriginal.code)) {
        collectGeneric(*spec);
    }
    FinalizeEventCommand(mResult);
    accept();
}

bool QtEventCommandEditDialog::EditCommand(QWidget* parent, rpg::EventCommand& inOut,
                                           int eventContext) {
    rpg::EventCommand decoded = inOut;
    DecodeCommandForEdit(decoded);
    QtEventCommandEditDialog dlg(decoded, eventContext, parent);
    if (dlg.exec() != QDialog::Accepted) return false;
    inOut = dlg.command();
    return true;
}

} // namespace qt_editor
