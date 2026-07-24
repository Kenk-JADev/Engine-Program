#include "QtEventEditorDialog.h"

#include "QtEventCommandCatalog.h"
#include "QtEventCommandEditDialog.h"
#include "QtEventCommandsDialog.h"

#include "rpgmaker3d/Database.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMessageBox>
#include <QPushButton>
#include <QSpinBox>
#include <QSplitter>
#include <QTabWidget>
#include <QTreeWidget>
#include <QTreeWidgetItem>
#include <QVBoxLayout>
#include <algorithm>
#include <map>

namespace qt_editor {

#ifndef QL
#define QL(x) QStringLiteral(x)
#endif

namespace {

constexpr int kIndexRole = Qt::UserRole;

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

QStringList RouteStepLabels() {
    // PAKET 16: XP-Vervollstaendigung (MRC-Codes in Klammern) — Tokens sind
    // die Serialisierungsform des Engines (EventSystem_ParseMoveRouteText).
    return {QL("Nach unten gehen"), QL("Nach links gehen"), QL("Nach rechts gehen"),
            QL("Nach oben gehen"), QL("Geradeaus gehen"), QL("Zum Spieler gehen"),
            QL("Vom Spieler weg"), QL("Zufällig gehen"),
            QL("Warten (Frames):"),
            QL("Blicke nach unten"), QL("Blicke nach links"),
            QL("Blicke nach rechts"), QL("Blicke nach oben"),
            QL("Rückwärts gehen"),
            QL("Springen (dx,dz):"),
            QL("90° nach rechts drehen"), QL("90° nach links drehen"),
            QL("180° drehen"), QL("Zufällig drehen"),
            QL("Zum Spieler blicken"), QL("Vom Spieler wegblicken"),
            QL("Schalter AN (ID):"), QL("Schalter AUS (ID):"),
            QL("Geschwindigkeit ändern (1..6):"),
            QL("Häufigkeit ändern (1..6):"),
            QL("Durchgehbar AN"), QL("Durchgehbar AUS"),
            QL("Transparent AN"), QL("Transparent AUS"),
            QL("Grafik wechseln (Name[,Index]):"),
            QL("SE abspielen (Name):"),
            QL("Script (Rest der Route):")};
}
QStringList RouteStepTokens() {
    return {QL("D"), QL("L"), QL("R"), QL("U"), QL("F"), QL("T"), QL("A"), QL("X"),
            QL("W"), QL("TD"), QL("TL"), QL("TR"), QL("TU"),
            QL("B"), QL("J"), QL("R90"), QL("L90"), QL("T180"), QL("TX"),
            QL("TT"), QL("TA"), QL("S+"), QL("S-"), QL("V"), QL("Q"),
            QL("H1"), QL("H0"), QL("P1"), QL("P0"), QL("G"), QL("E"), QL("SC")};
}

// Volltoken (mit evtl. Argument) -> lesbare Beschreibung.
QString DescribeRouteStep(const QString& fullToken) {
    const QStringList labels = RouteStepLabels();
    const QStringList tokens = RouteStepTokens();
    const QString base = fullToken.section(QLatin1Char(' '), 0, 0);
    const int exact = tokens.indexOf(base);
    if (fullToken.startsWith(QLatin1Char('W')) && fullToken.size() > 1
        && fullToken.mid(1).toInt() > 0)
        return QL("Warten: %1 Frames").arg(fullToken.mid(1).toInt());
    if (base.startsWith(QLatin1Char('J')))
        return QL("Springen: %1").arg(base.mid(1));
    if (base.startsWith(QL("S+")))
        return QL("Schalter %1 AN").arg(base.mid(2).toInt());
    if (base.startsWith(QL("S-")))
        return QL("Schalter %1 AUS").arg(base.mid(2).toInt());
    if (base.startsWith(QLatin1Char('V')) && base.size() > 1)
        return QL("Geschwindigkeit: %1").arg(base.mid(1).toInt());
    if (base.startsWith(QLatin1Char('Q')) && base.size() > 1)
        return QL("Häufigkeit: %1").arg(base.mid(1).toInt());
    if (base == QL("G") && fullToken.size() > 2)
        return QL("Grafik wechseln: %1").arg(fullToken.section(QLatin1Char(' '), 1));
    if (base == QL("E") && fullToken.size() > 2)
        return QL("SE abspielen: %1").arg(fullToken.section(QLatin1Char(' '), 1));
    if (base == QL("SC") && fullToken.size() > 3)
        return QL("Script: %1").arg(fullToken.section(QLatin1Char(' '), 1));
    if (exact >= 0) return labels[exact];
    return fullToken;
}

// Basis-Token braucht ein freies Argument-Feld (QLineEdit)?
bool RouteStepNeedsArg(const QString& baseToken) {
    static const QStringList argTokens = {
        QL("J"), QL("S+"), QL("S-"), QL("V"), QL("Q"), QL("G"), QL("E"), QL("SC")};
    return argTokens.contains(baseToken);
}

// Aus Basis-Token + Argument das kanonische Volltoken bauen (Serialisierung).
QString BuildRouteFullToken(const QString& base, const QString& argIn) {
    QString arg = argIn;
    if (base == QL("J")) {
        arg.remove(QLatin1Char('(')).remove(QLatin1Char(')')).remove(QLatin1Char(' '));
        return QL("J(%1)").arg(arg.isEmpty() ? QL("0,0") : arg);
    }
    if (base == QL("S+") || base == QL("S-"))
        return base + QString::number(arg.toInt());
    if (base == QL("V") || base == QL("Q"))
        return base + QString::number(arg.toInt());
    if (base == QL("G") || base == QL("E")) {          // Namen ohne Leerzeichen
        arg.remove(QLatin1Char(' '));
        return base + QLatin1Char(' ') + arg;
    }
    if (base == QL("SC"))
        return QL("SC ") + arg;                        // Script frisst Rest
    return base;
}

// QListWidget-Eintrag aus Volltoken (Token sicher in UserRole ablegen,
// damit die Rueck-Serialisierung nicht am Anzeigetext haengt).
QListWidgetItem* MakeRouteItem(const QString& fullToken) {
    auto* item = new QListWidgetItem(DescribeRouteStep(fullToken));
    item->setData(kIndexRole, fullToken);
    return item;
}

} // namespace

// ---------------------------------------------------------------------------
// Bewegungsroute-Dialog
// ---------------------------------------------------------------------------

bool EditMoveRoute(QWidget* parent, std::string& routeText, bool& repeat, bool& skippable) {
    QDialog dlg(parent);
    dlg.setWindowTitle(QL("Bewegungsroute"));
    dlg.setModal(true);
    dlg.resize(600, 460); // PAKET 16: breiter (Argument-Feld in der Add-Zeile)
    auto* root = new QVBoxLayout(&dlg);

    auto* info = new QLabel(QL("Schritte der benutzerdefinierten Bewegungsroute "
                               "(wird der Reihe nach abgearbeitet)."), &dlg);
    info->setWordWrap(true);
    root->addWidget(info);

    auto* list = new QListWidget(&dlg);
    // Bestehende Route in Schritte zerlegen — PAKET 16: G/E fressen das
    // naechste Token, SC den ganzen Rest (wie der Engine-Parser).
    {
        const QString text = QString::fromStdString(routeText);
        const QStringList raw = text.split(QLatin1Char(' '), Qt::SkipEmptyParts);
        for (int i = 0; i < raw.size(); ++i) {
            const QString up = raw[i].toUpper();
            QString full = raw[i];
            if ((up == QL("G") || up == QL("E")) && i + 1 < raw.size())
                full = raw[i] + QLatin1Char(' ') + raw[++i];
            else if (up == QL("SC")) {
                const QString scTok = raw[i];           // vor ++i sichern
                QStringList rest;
                while (++i < raw.size()) rest << raw[i]; // SC frisst den Rest
                full = scTok + QLatin1Char(' ') + rest.join(QLatin1Char(' '));
            }
            list->addItem(MakeRouteItem(full));
        }
    }
    root->addWidget(list, 1);

    auto* addRow = new QHBoxLayout();
    auto* stepCombo = new QComboBox(&dlg);
    const QStringList labels = RouteStepLabels();
    const QStringList tokens = RouteStepTokens();
    for (const QString& l : labels) stepCombo->addItem(l);
    auto* waitSpin = new QSpinBox(&dlg);
    waitSpin->setRange(1, 999);
    waitSpin->setValue(20);
    waitSpin->setEnabled(false);
    // PAKET 16: generisches Argumentfeld (Sprung/Schalter/Tempo/Grafik/SE/Script)
    auto* argEdit = new QLineEdit(&dlg);
    argEdit->setPlaceholderText(QL("Argument (dx,dz / ID / 1..6 / Name)"));
    argEdit->setEnabled(false);
    auto* addBtn = new QPushButton(QL("Hinzufügen"), &dlg);
    const int waitIdx = tokens.indexOf(QL("W"));
    QObject::connect(stepCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
                     &dlg, [waitSpin, argEdit, tokens, waitIdx](int idx) {
        waitSpin->setEnabled(idx == waitIdx);
        argEdit->setEnabled(idx >= 0 && RouteStepNeedsArg(tokens.value(idx)));
    });
    QObject::connect(addBtn, &QPushButton::clicked, &dlg, [&]() {
        const int idx = stepCombo->currentIndex();
        const QString base = tokens.value(idx, QL("F"));
        QString full = base;
        if (idx == waitIdx) full = base + QString::number(waitSpin->value());
        else if (RouteStepNeedsArg(base)) full = BuildRouteFullToken(base, argEdit->text());
        list->addItem(MakeRouteItem(full));
    });
    addRow->addWidget(stepCombo, 1);
    addRow->addWidget(waitSpin);
    addRow->addWidget(argEdit, 1);
    addRow->addWidget(addBtn);
    root->addLayout(addRow);

    auto* btnRow = new QHBoxLayout();
    auto* upBtn = new QPushButton(QL("▲"), &dlg);
    auto* dnBtn = new QPushButton(QL("▼"), &dlg);
    auto* rmBtn = new QPushButton(QL("Entfernen"), &dlg);
    QObject::connect(upBtn, &QPushButton::clicked, &dlg, [&]() {
        int r = list->currentRow();
        if (r <= 0) return;
        auto* item = list->takeItem(r);
        list->insertItem(r - 1, item);
        list->setCurrentRow(r - 1);
    });
    QObject::connect(dnBtn, &QPushButton::clicked, &dlg, [&]() {
        int r = list->currentRow();
        if (r < 0 || r >= list->count() - 1) return;
        auto* item = list->takeItem(r);
        list->insertItem(r + 1, item);
        list->setCurrentRow(r + 1);
    });
    QObject::connect(rmBtn, &QPushButton::clicked, &dlg, [&]() {
        delete list->takeItem(list->currentRow());
    });
    btnRow->addWidget(upBtn);
    btnRow->addWidget(dnBtn);
    btnRow->addWidget(rmBtn);
    btnRow->addStretch(1);
    root->addLayout(btnRow);

    auto* repeatCheck = new QCheckBox(QL("Wiederholen"), &dlg);
    repeatCheck->setChecked(repeat);
    auto* skipCheck = new QCheckBox(QL("Überspringbar"), &dlg);
    skipCheck->setChecked(skippable);
    root->addWidget(repeatCheck);
    root->addWidget(skipCheck);

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dlg);
    buttons->button(QDialogButtonBox::Ok)->setText(QL("OK"));
    buttons->button(QDialogButtonBox::Cancel)->setText(QL("Abbrechen"));
    QObject::connect(buttons, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
    QObject::connect(buttons, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);
    root->addWidget(buttons);

    if (dlg.exec() != QDialog::Accepted) return false;

    // Liste zurück in Routentext — PAKET 16: das kanonische Volltoken steht
    // in der UserRole des Eintrags (kein Rueck-Mapping ueber Anzeigetexte).
    QStringList outTokens;
    for (int i = 0; i < list->count(); ++i) {
        const QString full = list->item(i)->data(kIndexRole).toString();
        if (!full.isEmpty()) outTokens << full;
    }
    routeText = outTokens.join(QLatin1Char(' ')).toStdString();
    repeat = repeatCheck->isChecked();
    skippable = skipCheck->isChecked();
    return true;
}

// ---------------------------------------------------------------------------
// Konstruktor / UI
// ---------------------------------------------------------------------------

QtEventEditorDialog::QtEventEditorDialog(const rpg::MapEvent& ev, QWidget* parent)
    : QDialog(parent), mEvent(ev) {
    setModal(true);
    setWindowTitle(QL("Event - ID:%1").arg(ev.id, 3, 10, QLatin1Char('0')));
    resize(1060, 640);
    if (mEvent.pages.empty()) {
        rpg::EventPage p;
        mEvent.pages.push_back(p);
    }
    buildUi();
    rebuildPageTabs();
    loadPageWidgets();
}

void QtEventEditorDialog::buildUi() {
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(8, 8, 8, 8);

    // ---- Kopfzeile: ID/Name + Seiten-Buttons ------------------------------
    auto* head = new QHBoxLayout();
    head->addWidget(new QLabel(QL("Name:"), this));
    mNameEdit = new QLineEdit(QString::fromStdString(mEvent.name), this);
    mNameEdit->setMaximumWidth(200);
    head->addWidget(mNameEdit);
    head->addSpacing(16);

    const struct { const char* text; void (QtEventEditorDialog::*slot)(); } pageBtns[] = {
        {"Neue Seite", &QtEventEditorDialog::onNewPage},
        {"Seite kopieren", &QtEventEditorDialog::onCopyPage},
        {"Seite einfügen", &QtEventEditorDialog::onPastePage},
        {"Seite löschen", &QtEventEditorDialog::onDeletePage},
        {"Seite leeren", &QtEventEditorDialog::onClearPage},
    };
    for (const auto& b : pageBtns) {
        auto* btn = new QPushButton(QLatin1String(b.text), this);
        connect(btn, &QPushButton::clicked, this, b.slot);
        head->addWidget(btn);
    }
    head->addStretch(1);
    root->addLayout(head);

    // ---- Hauptbereich ------------------------------------------------------
    auto* split = new QSplitter(Qt::Horizontal, this);

    // Linke Spalte: Seiten-Tabs + Bedingungen/Grafik/Bewegung/Optionen/Trigger
    auto* leftWrap = new QWidget(split);
    auto* left = new QHBoxLayout(leftWrap);
    left->setContentsMargins(0, 0, 0, 0);

    mPageTabs = new QTabWidget(leftWrap);
    mPageTabs->setTabPosition(QTabWidget::West);
    mPageTabs->setMinimumWidth(64);
    mPageTabs->setMaximumWidth(90);
    // Dummy-Inhalt; die eigentlichen Widgets liegen daneben (wie XP)
    left->addWidget(mPageTabs);
    connect(mPageTabs, &QTabWidget::currentChanged,
            this, &QtEventEditorDialog::onPageTabChanged);

    auto* leftForm = new QVBoxLayout();
    left->addLayout(leftForm, 1);

    // Bedingungen
    auto* condBox = new QGroupBox(QL("Bedingungen (Seite aktiv, wenn alle zutreffen)"), leftWrap);
    auto* condGrid = new QGridLayout(condBox);
    mSwitch1Check = new QCheckBox(QL("Schalter 1"), condBox);
    mSwitch1Combo = new QComboBox(condBox); fillSwitchCombo(mSwitch1Combo);
    mSwitch2Check = new QCheckBox(QL("Schalter 2"), condBox);
    mSwitch2Combo = new QComboBox(condBox); fillSwitchCombo(mSwitch2Combo);
    mVarCheck = new QCheckBox(QL("Variable"), condBox);
    mVarCombo = new QComboBox(condBox); fillVariableCombo(mVarCombo);
    mVarValue = new QSpinBox(condBox);
    mVarValue->setRange(-999999, 999999);
    mSelfCheck = new QCheckBox(QL("Selbstschalter"), condBox);
    mSelfCombo = new QComboBox(condBox);
    mSelfCombo->addItems({QL("A"), QL("B"), QL("C"), QL("D")});

    condGrid->addWidget(mSwitch1Check, 0, 0);
    condGrid->addWidget(mSwitch1Combo, 0, 1, 1, 2);
    condGrid->addWidget(mSwitch2Check, 1, 0);
    condGrid->addWidget(mSwitch2Combo, 1, 1, 1, 2);
    condGrid->addWidget(mVarCheck, 2, 0);
    condGrid->addWidget(mVarCombo, 2, 1);
    condGrid->addWidget(mVarValue, 2, 2);
    auto* varHint = new QLabel(QL("oder höher"), condBox);
    condGrid->addWidget(varHint, 3, 1, 1, 2);
    condGrid->addWidget(mSelfCheck, 4, 0);
    condGrid->addWidget(mSelfCombo, 4, 1);
    leftForm->addWidget(condBox);

    // Grafik
    auto* gfxBox = new QGroupBox(QL("Grafik"), leftWrap);
    auto* gfxForm = new QFormLayout(gfxBox);
    mGraphicEdit = new QLineEdit(gfxBox);
    mGraphicEdit->setPlaceholderText(QL("z.B. npc_villager (leer = Standard)"));
    mGraphicIndex = new QSpinBox(gfxBox);
    mGraphicIndex->setRange(0, 7);
    gfxForm->addRow(QL("Datei/Modell"), mGraphicEdit);
    gfxForm->addRow(QL("Index"), mGraphicIndex);
    leftForm->addWidget(gfxBox);

    // Autonome Bewegung
    auto* moveBox = new QGroupBox(QL("Autonome Bewegung"), leftWrap);
    auto* moveForm = new QFormLayout(moveBox);
    mMoveType = new QComboBox(moveBox);
    mMoveType->addItem(QL("Fest"), 0);
    mMoveType->addItem(QL("Zufällig"), 1);
    mMoveType->addItem(QL("Annähern"), 2);
    mMoveType->addItem(QL("Benutzerdefiniert"), 3);
    mMoveSpeed = new QComboBox(moveBox);
    mMoveFreq = new QComboBox(moveBox);
    for (int i = 1; i <= 6; ++i) {
        mMoveSpeed->addItem(QString::number(i), i);
        mMoveFreq->addItem(QString::number(i), i);
    }
    auto* routeBtn = new QPushButton(QL("Bewegungsroute ..."), moveBox);
    connect(routeBtn, &QPushButton::clicked, this, &QtEventEditorDialog::onEditMoveRoute);
    moveForm->addRow(QL("Typ"), mMoveType);
    moveForm->addRow(QL("Tempo"), mMoveSpeed);
    moveForm->addRow(QL("Häufigkeit"), mMoveFreq);
    moveForm->addRow(routeBtn);
    leftForm->addWidget(moveBox);

    // Optionen
    auto* optBox = new QGroupBox(QL("Optionen"), leftWrap);
    auto* optForm = new QVBoxLayout(optBox);
    mWalkAnime = new QCheckBox(QL("Bewegungsanimation"), optBox);
    mStepAnime = new QCheckBox(QL("Stopp-Animation"), optBox);
    mDirFix = new QCheckBox(QL("Richtung fixieren"), optBox);
    mThrough = new QCheckBox(QL("Durchgehbar"), optBox);
    mAlwaysTop = new QCheckBox(QL("Immer im Vordergrund"), optBox);
    optForm->addWidget(mWalkAnime);
    optForm->addWidget(mStepAnime);
    optForm->addWidget(mDirFix);
    optForm->addWidget(mThrough);
    optForm->addWidget(mAlwaysTop);
    leftForm->addWidget(optBox);

    // Trigger
    auto* trigBox = new QGroupBox(QL("Auslöser"), leftWrap);
    auto* trigForm = new QFormLayout(trigBox);
    mTrigger = new QComboBox(trigBox);
    mTrigger->addItem(QL("Aktionstaste"), (int)rpg::EventTrigger::ActionButton);
    mTrigger->addItem(QL("Spieler berührt"), (int)rpg::EventTrigger::PlayerTouch);
    mTrigger->addItem(QL("Event berührt"), (int)rpg::EventTrigger::EventTouch);
    mTrigger->addItem(QL("Automatisch"), (int)rpg::EventTrigger::Autorun);
    mTrigger->addItem(QL("Parallel"), (int)rpg::EventTrigger::Parallel);
    trigForm->addRow(mTrigger);
    leftForm->addWidget(trigBox);
    leftForm->addStretch(1);

    split->addWidget(leftWrap);

    // Rechte Seite: Befehlsliste
    auto* rightWrap = new QWidget(split);
    auto* right = new QVBoxLayout(rightWrap);
    right->setContentsMargins(0, 0, 0, 0);
    right->addWidget(new QLabel(QL("Liste der Event-Befehle "
                                   "(Doppelklick zum Bearbeiten / Einfügen)"), rightWrap));

    mCommandTree = new QTreeWidget(rightWrap);
    mCommandTree->setHeaderHidden(true);
    mCommandTree->setRootIsDecorated(false);
    mCommandTree->setSelectionMode(QAbstractItemView::SingleSelection);
    mCommandTree->setIndentation(0);
    connect(mCommandTree, &QTreeWidget::itemDoubleClicked,
            this, &QtEventEditorDialog::onEditCommandItem);
    right->addWidget(mCommandTree, 1);

    auto* cmdBtns = new QHBoxLayout();
    const struct { const char* text; void (QtEventEditorDialog::*slot)(); } btns[] = {
        {"Einfügen ...", &QtEventEditorDialog::onAddCommand},
        {"Bearbeiten ...", &QtEventEditorDialog::onEditCommand},
        {"Entfernen", &QtEventEditorDialog::onRemoveCommand},
        {"▲", &QtEventEditorDialog::onCommandUp},
        {"▼", &QtEventEditorDialog::onCommandDown},
    };
    for (const auto& b : btns) {
        auto* btn = new QPushButton(QLatin1String(b.text), rightWrap);
        connect(btn, &QPushButton::clicked, this, b.slot);
        cmdBtns->addWidget(btn);
    }
    cmdBtns->addStretch(1);
    right->addLayout(cmdBtns);

    mHintLabel = new QLabel(QL("Tipp: Neue Befehle werden VOR der markierten Zeile "
                               "eingefügt (gleiche Einrückung). Ohne Markierung ans Ende."), rightWrap);
    mHintLabel->setWordWrap(true);
    right->addWidget(mHintLabel);

    split->addWidget(rightWrap);
    split->setStretchFactor(0, 0);
    split->setStretchFactor(1, 1);
    root->addWidget(split, 1);

    // ---- OK / Abbrechen / Anwenden -----------------------------------------
    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel
                                         | QDialogButtonBox::Apply, this);
    buttons->button(QDialogButtonBox::Ok)->setText(QL("OK"));
    buttons->button(QDialogButtonBox::Cancel)->setText(QL("Abbrechen"));
    buttons->button(QDialogButtonBox::Apply)->setText(QL("Anwenden"));
    connect(buttons->button(QDialogButtonBox::Ok), &QPushButton::clicked,
            this, &QtEventEditorDialog::onOk);
    connect(buttons->button(QDialogButtonBox::Cancel), &QPushButton::clicked,
            this, &QDialog::reject);
    connect(buttons->button(QDialogButtonBox::Apply), &QPushButton::clicked,
            this, &QtEventEditorDialog::onApply);
    root->addWidget(buttons);
}

// ---------------------------------------------------------------------------
// Seiten-Verwaltung
// ---------------------------------------------------------------------------

void QtEventEditorDialog::rebuildPageTabs() {
    mSyncing = true;
    mPageTabs->clear();
    for (size_t i = 0; i < mEvent.pages.size(); ++i)
        mPageTabs->addTab(new QWidget(mPageTabs), QString::number(i + 1));
    if (mPage >= (int)mEvent.pages.size()) mPage = (int)mEvent.pages.size() - 1;
    if (mPage < 0) mPage = 0;
    mPageTabs->setCurrentIndex(mPage);
    mSyncing = false;
}

void QtEventEditorDialog::onPageTabChanged(int index) {
    if (mSyncing || index < 0 || index == mPage
        || index >= (int)mEvent.pages.size()) return;
    savePageWidgets();      // alte Seite sichern
    mPage = index;
    loadPageWidgets();      // neue Seite laden
}

void QtEventEditorDialog::savePageWidgets() {
    if (mPage < 0 || mPage >= (int)mEvent.pages.size()) return;
    rpg::EventPage& p = page(mPage);
    p.condition.switch1Valid = mSwitch1Check->isChecked();
    p.condition.switch1Id = mSwitch1Combo->currentData().toInt();
    p.condition.switch2Valid = mSwitch2Check->isChecked();
    p.condition.switch2Id = mSwitch2Combo->currentData().toInt();
    p.condition.variableValid = mVarCheck->isChecked();
    p.condition.variableId = mVarCombo->currentData().toInt();
    p.condition.variableValue = mVarValue->value();
    p.condition.selfSwitchValid = mSelfCheck->isChecked();
    p.condition.selfSwitchCh = mSelfCombo->currentText().isEmpty()
        ? 'A' : mSelfCombo->currentText().at(0).toLatin1();
    p.graphicName = mGraphicEdit->text().trimmed().toStdString();
    p.graphicIndex = mGraphicIndex->value();
    p.moveType = mMoveType->currentData().toInt();
    p.moveSpeed = mMoveSpeed->currentData().toInt();
    p.moveFrequency = mMoveFreq->currentData().toInt();
    p.walkAnime = mWalkAnime->isChecked();
    p.stepAnime = mStepAnime->isChecked();
    p.directionFix = mDirFix->isChecked();
    p.through = mThrough->isChecked();
    p.alwaysOnTop = mAlwaysTop->isChecked();
    p.trigger = static_cast<rpg::EventTrigger>(mTrigger->currentData().toInt());
}

void QtEventEditorDialog::loadPageWidgets() {
    if (mPage < 0 || mPage >= (int)mEvent.pages.size()) return;
    const rpg::EventPage& p = page(mPage);
    mSwitch1Check->setChecked(p.condition.switch1Valid);
    setComboToData(mSwitch1Combo, p.condition.switch1Id);
    mSwitch2Check->setChecked(p.condition.switch2Valid);
    setComboToData(mSwitch2Combo, p.condition.switch2Id);
    mVarCheck->setChecked(p.condition.variableValid);
    setComboToData(mVarCombo, p.condition.variableId);
    mVarValue->setValue(p.condition.variableValue);
    mSelfCheck->setChecked(p.condition.selfSwitchValid);
    int si = mSelfCombo->findText(QString(QChar(p.condition.selfSwitchCh)));
    mSelfCombo->setCurrentIndex(si >= 0 ? si : 0);
    mGraphicEdit->setText(QString::fromStdString(p.graphicName));
    mGraphicIndex->setValue(p.graphicIndex);
    setComboToData(mMoveType, p.moveType);
    setComboToData(mMoveSpeed, p.moveSpeed);
    setComboToData(mMoveFreq, p.moveFrequency);
    mWalkAnime->setChecked(p.walkAnime);
    mStepAnime->setChecked(p.stepAnime);
    mDirFix->setChecked(p.directionFix);
    mThrough->setChecked(p.through);
    mAlwaysTop->setChecked(p.alwaysOnTop);
    setComboToData(mTrigger, (int)p.trigger);
    rebuildCommandList();
}

void QtEventEditorDialog::onNewPage() {
    savePageWidgets();
    rpg::EventPage p;
    p.id = (int)mEvent.pages.size();
    mEvent.pages.push_back(p);
    mPage = (int)mEvent.pages.size() - 1;
    rebuildPageTabs();
    loadPageWidgets();
}

void QtEventEditorDialog::onCopyPage() {
    savePageWidgets();
    mClipboard = page(mPage);
    mHasClipboard = true;
}

void QtEventEditorDialog::onPastePage() {
    if (!mHasClipboard) return;
    const auto oldListClear = mClipboard;
    page(mPage) = oldListClear; // XP: Einfügen ersetzt die aktuelle Seite
    page(mPage).id = mPage;
    loadPageWidgets();
}

void QtEventEditorDialog::onDeletePage() {
    if (mEvent.pages.size() <= 1) {
        QMessageBox::information(this, QL("Seite löschen"),
                                 QL("Das Event braucht mindestens eine Seite."));
        return;
    }
    mEvent.pages.erase(mEvent.pages.begin() + mPage);
    for (size_t i = 0; i < mEvent.pages.size(); ++i)
        mEvent.pages[i].id = (int)i;
    if (mPage >= (int)mEvent.pages.size()) mPage = (int)mEvent.pages.size() - 1;
    rebuildPageTabs();
    loadPageWidgets();
}

void QtEventEditorDialog::onClearPage() {
    page(mPage).list.clear();
    rebuildCommandList();
}

void QtEventEditorDialog::onEditMoveRoute() {
    savePageWidgets();
    rpg::EventPage& p = page(mPage);
    std::string route = p.customRoute;
    bool rep = p.routeRepeat;
    bool skip = p.routeSkippable;
    if (EditMoveRoute(this, route, rep, skip)) {
        p.customRoute = route;
        p.routeRepeat = rep;
        p.routeSkippable = skip;
        p.moveType = 3; // Benutzerdefiniert
        loadPageWidgets();
    }
}

// ---------------------------------------------------------------------------
// Befehlsliste
// ---------------------------------------------------------------------------

void QtEventEditorDialog::rebuildCommandList() {
    mCommandTree->clear();
    if (mPage < 0 || mPage >= (int)mEvent.pages.size()) return;
    const auto& list = page(mPage).list;
    for (size_t i = 0; i < list.size(); ++i) {
        const rpg::EventCommand& c = list[i];
        QString indent2(c.indent * 2, QLatin1Char(' '));
        auto* item = new QTreeWidgetItem(mCommandTree);
        item->setText(0, indent2 + QL("@>") + FormatEventCommand(c, mEvent.id));
        item->setData(0, kIndexRole, (int)i);
        item->setToolTip(0, QL("Code %1, Einzug %2").arg((int)c.code).arg(c.indent));
    }
}

int QtEventEditorDialog::currentCommandIndex() const {
    auto* item = mCommandTree->currentItem();
    return item ? item->data(0, kIndexRole).toInt() : -1;
}

int QtEventEditorDialog::insertIndent() const {
    const auto& list = page(mPage).list;
    const int idx = currentCommandIndex();
    if (idx >= 0 && idx < (int)list.size())
        return list[(size_t)idx].indent;
    return 0;
}

void QtEventEditorDialog::onAddCommand() {
    rpg::EventCommandCode code = rpg::EventCommandCode::None;
    if (!QtEventCommandsDialog::ChooseCommand(this, code)) return;

    rpg::EventCommand c;
    c.code = code;
    // Sinnvolle Verzuwerte je Befehl über den Parameterdialog holen
    if (!QtEventCommandEditDialog::EditCommand(this, c, mEvent.id)) return;

    auto& list = page(mPage).list;
    std::vector<rpg::EventCommand> block = BuildCommandBlock(c, insertIndent());
    const int idx = currentCommandIndex();
    const int pos = (idx >= 0 && idx <= (int)list.size()) ? idx : (int)list.size();
    list.insert(list.begin() + pos, block.begin(), block.end());
    rebuildCommandList();
    // Eingefügten Kopf markieren
    if (pos < mCommandTree->topLevelItemCount())
        mCommandTree->setCurrentItem(mCommandTree->topLevelItem(pos));
}

void QtEventEditorDialog::onEditCommandItem(QTreeWidgetItem* item, int column) {
    (void)column;
    if (item) {
        mCommandTree->setCurrentItem(item);
        onEditCommand();
    }
}

void QtEventEditorDialog::onEditCommand() {
    auto& list = page(mPage).list;
    const int idx = currentCommandIndex();
    if (idx < 0 || idx >= (int)list.size()) return;

    rpg::EventCommand& target = list[(size_t)idx];
    using CC = rpg::EventCommandCode;

    // Struktur-Zeilen ohne Parameterdialog
    switch (target.code) {
        case CC::WhenChoice:
        case CC::WhenCancel:
        case CC::ChoicesEnd:
        case CC::Else:
        case CC::BranchEnd:
        case CC::RepeatAbove:
        case CC::IfWin:
        case CC::IfEscape:
        case CC::IfLose:
        case CC::TextLine:
        case CC::CommentLine:
        case CC::ScriptLine:
            return; // werden über ihren Kopfbefehl bearbeitet
        default: break;
    }

    // Mehrzeilige Köpfe: Folgezeilen einsammeln (401/408/655)
    if (target.code == CC::ShowText || target.code == CC::Comment || target.code == CC::Script) {
        const int cont = ContinuationLineCount(list, idx);
        const std::string origText = target.text;
        target.text = JoinContinuationLines(list, idx).toStdString();
        rpg::EventCommand edited = target;
        if (!QtEventCommandEditDialog::EditCommand(this, edited, mEvent.id)) {
            target.text = origText; // Abbruch: nichts verändern
            rebuildCommandList();
            return;
        }
        replaceCommandBlock(idx, BuildCommandBlock(edited, target.indent), 1 + cont);
        return;
    }

    rpg::EventCommand edited = target;

    // Bei Bedingung: a5 (Sonst-Zweig) aus vorhandener Struktur vorbelegen
    if (target.code == CC::ConditionalBranch) {
        const int h = target.indent;
        for (size_t i = (size_t)idx + 1; i < list.size(); ++i) {
            if (list[i].indent != h) continue;
            if (list[i].code == CC::Else) SetCommandArgInt(edited, QL("a5"), 1);
            if (list[i].code == CC::BranchEnd) break;
        }
    }

    if (!QtEventCommandEditDialog::EditCommand(this, edited, mEvent.id)) return;
    edited.indent = target.indent;

    if (edited.code == CC::ShowChoices) {
        FinalizeEventCommand(edited);
        reconcileChoices(idx, edited);
    } else if (edited.code == CC::ConditionalBranch) {
        reconcileBranch(idx, edited);
    } else {
        FinalizeEventCommand(edited);
        target = edited;
    }
    savePageWidgets();
    rebuildCommandList();
}

void QtEventEditorDialog::replaceCommandBlock(int index,
                                              const std::vector<rpg::EventCommand>& block,
                                              int removeCount) {
    auto& list = page(mPage).list;
    if (index < 0 || index >= (int)list.size()) return;
    list.erase(list.begin() + index, list.begin() + std::min((int)list.size(), index + removeCount));
    list.insert(list.begin() + index, block.begin(), block.end());
    rebuildCommandList();
}

void QtEventEditorDialog::reconcileChoices(int headerIndex, const rpg::EventCommand& edited) {
    auto& list = page(mPage).list;
    const int h = list[(size_t)headerIndex].indent;

    // Blockende (404 mit gleichem Einzug) suchen
    int end = headerIndex + 1;
    while (end < (int)list.size()
           && !(list[(size_t)end].code == rpg::EventCommandCode::ChoicesEnd
                && list[(size_t)end].indent == h)) ++end;

    // Inhalte der alten Zweige sichern: Key 0..3, Key 90 = Abbruch
    std::map<int, std::vector<rpg::EventCommand>> bodies;
    {
        int key = -1;
        for (int i = headerIndex + 1; i < end; ++i) {
            const rpg::EventCommand& c = list[(size_t)i];
            if (c.indent == h && c.code == rpg::EventCommandCode::WhenChoice) {
                key = c.param1;
                continue;
            }
            if (c.indent == h && c.code == rpg::EventCommandCode::WhenCancel) {
                key = 90;
                continue;
            }
            if (key >= 0) bodies[key].push_back(c);
            else bodies[0].push_back(c);
        }
    }

    // Neuen Block aus den editierten Daten aufbauen
    std::vector<rpg::EventCommand> fresh = BuildCommandBlock(edited, h);
    std::vector<rpg::EventCommand> merged;
    int key = -1;
    auto flushBody = [&]() {
        if (key < 0) return;
        auto it = bodies.find(key);
        if (it != bodies.end())
            merged.insert(merged.end(), it->second.begin(), it->second.end());
    };
    for (const rpg::EventCommand& c : fresh) {
        if (c.code == rpg::EventCommandCode::WhenChoice) key = c.param1;
        else if (c.code == rpg::EventCommandCode::WhenCancel) key = 90;
        else if (c.code == rpg::EventCommandCode::ChoicesEnd) key = -2; // kein Body dahinter
        merged.push_back(c);
        // Body des soeben geschriebenen Zweigkopfs direkt anhängen
        if (c.code == rpg::EventCommandCode::WhenChoice
            || c.code == rpg::EventCommandCode::WhenCancel)
            flushBody();
        if (c.code != rpg::EventCommandCode::WhenChoice
            && c.code != rpg::EventCommandCode::WhenCancel)
            key = -1;
    }

    replaceCommandBlock(headerIndex, merged, (end < (int)list.size() ? end - headerIndex + 1
                                                                     : end - headerIndex));
    savePageWidgets();
}

void QtEventEditorDialog::reconcileBranch(int headerIndex, const rpg::EventCommand& edited) {
    auto& list = page(mPage).list;
    const int h = list[(size_t)headerIndex].indent;

    // Ende des Blocks: BranchEnd (412) mit gleichem Einzug
    int end = headerIndex + 1;
    int elseAt = -1;
    while (end < (int)list.size()) {
        const rpg::EventCommand& c = list[(size_t)end];
        if (c.indent == h && c.code == rpg::EventCommandCode::Else) elseAt = end;
        if (c.indent == h && c.code == rpg::EventCommandCode::BranchEnd) break;
        ++end;
    }

    // Header ersetzen (a5 nur fuer den Editor)
    rpg::EventCommand head = edited;
    head.indent = h;
    list[(size_t)headerIndex] = head;

    const bool wantElse = GetCommandArgInt(edited, QL("a5"), 0) == 1;
    if (wantElse && elseAt < 0 && end < (int)list.size()) {
        rpg::EventCommand e;
        e.code = rpg::EventCommandCode::Else;
        e.indent = h;
        list.insert(list.begin() + end, e);
    } else if (!wantElse && elseAt >= 0 && end < (int)list.size()) {
        list.erase(list.begin() + elseAt, list.begin() + end);
    }
    savePageWidgets();
    rebuildCommandList();
}

void QtEventEditorDialog::onRemoveCommand() {
    auto& list = page(mPage).list;
    const int idx = currentCommandIndex();
    if (idx < 0 || idx >= (int)list.size()) return;
    using CC = rpg::EventCommandCode;

    // Ganze Blöcke entfernen (Kopf + Struktur)
    int removeCount = 1;
    const int h = list[(size_t)idx].indent;
    if (list[(size_t)idx].code == CC::ShowChoices) {
        int end = idx + 1;
        while (end < (int)list.size()
               && !(list[(size_t)end].code == CC::ChoicesEnd && list[(size_t)end].indent == h))
            ++end;
        removeCount = (end < (int)list.size()) ? end - idx + 1 : end - idx;
        if (QMessageBox::question(this, QL("Befehl entfernen"),
                QL("Die gesamte Auswahl mit allen Zweigen entfernen?"))
            != QMessageBox::Yes) return;
    } else if (list[(size_t)idx].code == CC::ConditionalBranch) {
        int end = idx + 1;
        while (end < (int)list.size()
               && !(list[(size_t)end].code == CC::BranchEnd && list[(size_t)end].indent == h))
            ++end;
        removeCount = (end < (int)list.size()) ? end - idx + 1 : end - idx;
        if (QMessageBox::question(this, QL("Befehl entfernen"),
                QL("Die gesamte Bedingung mit Inhalt entfernen?"))
            != QMessageBox::Yes) return;
    } else if (list[(size_t)idx].code == CC::Loop) {
        int end = idx + 1;
        while (end < (int)list.size()
               && !(list[(size_t)end].code == CC::RepeatAbove && list[(size_t)end].indent == h))
            ++end;
        removeCount = (end < (int)list.size()) ? end - idx + 1 : end - idx;
    } else if (list[(size_t)idx].code == CC::ShowText
               || list[(size_t)idx].code == CC::Comment
               || list[(size_t)idx].code == CC::Script) {
        removeCount = 1 + ContinuationLineCount(list, idx);
    } else {
        // Struktur-Zeilen nicht einzeln löschen
        const CC c = list[(size_t)idx].code;
        if (c == CC::WhenChoice || c == CC::WhenCancel || c == CC::ChoicesEnd
            || c == CC::Else || c == CC::BranchEnd || c == CC::RepeatAbove
            || c == CC::IfWin || c == CC::IfEscape || c == CC::IfLose
            || c == CC::TextLine || c == CC::CommentLine || c == CC::ScriptLine) {
            QMessageBox::information(this, QL("Befehl entfernen"),
                QL("Diese Zeile gehört zu einem Block und kann nur über den "
                   "Block-Kopf (Auswahl/Bedingung/...) entfernt werden."));
            return;
        }
    }

    list.erase(list.begin() + idx, list.begin() + std::min((int)list.size(), idx + removeCount));
    rebuildCommandList();
    if (idx < mCommandTree->topLevelItemCount())
        mCommandTree->setCurrentItem(mCommandTree->topLevelItem(std::max(0, idx - 1)));
    savePageWidgets();
}

void QtEventEditorDialog::onCommandUp() {
    auto& list = page(mPage).list;
    const int idx = currentCommandIndex();
    if (idx <= 0 || idx >= (int)list.size()) return;
    std::swap(list[(size_t)idx], list[(size_t)idx - 1]);
    rebuildCommandList();
    if (idx - 1 < mCommandTree->topLevelItemCount())
        mCommandTree->setCurrentItem(mCommandTree->topLevelItem(idx - 1));
    savePageWidgets();
}

void QtEventEditorDialog::onCommandDown() {
    auto& list = page(mPage).list;
    const int idx = currentCommandIndex();
    if (idx < 0 || idx >= (int)list.size() - 1) return;
    std::swap(list[(size_t)idx], list[(size_t)idx + 1]);
    rebuildCommandList();
    if (idx + 1 < mCommandTree->topLevelItemCount())
        mCommandTree->setCurrentItem(mCommandTree->topLevelItem(idx + 1));
    savePageWidgets();
}

// ---------------------------------------------------------------------------
// Abschluss
// ---------------------------------------------------------------------------

void QtEventEditorDialog::onApply() {
    savePageWidgets();
    mEvent.name = mNameEdit->text().trimmed().toStdString();
}

void QtEventEditorDialog::onOk() {
    onApply();
    if (mEvent.name.empty())
        mEvent.name = "EV" + std::to_string(mEvent.id);
    accept();
}

rpg::MapEvent QtEventEditorDialog::mapEvent() const {
    return mEvent;
}

bool QtEventEditorDialog::EditEvent(QWidget* parent, rpg::MapEvent& ev) {
    QtEventEditorDialog dlg(ev, parent);
    if (dlg.exec() != QDialog::Accepted) return false;
    ev = dlg.mapEvent();
    return true;
}

} // namespace qt_editor
