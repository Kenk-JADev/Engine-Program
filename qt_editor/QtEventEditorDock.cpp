#include "QtEventEditorDock.h"

#include "rpgmaker3d/Engine.h"
#include "rpgmaker3d/EventSystem.h"
#include "rpgmaker3d/Project.h"
#include "rpgmaker3d/Database.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QListWidget>
#include <QLineEdit>
#include <QSpinBox>
#include <QComboBox>
#include <QPlainTextEdit>
#include <QLabel>
#include <QPushButton>
#include <QGroupBox>
#include <QSplitter>
#include <QMessageBox>
#include <algorithm>

namespace qt_editor {

namespace {

const char* TriggerName(rpg::EventTrigger t) {
    switch (t) {
        case rpg::EventTrigger::ActionButton: return "ActionButton (E)";
        case rpg::EventTrigger::PlayerTouch: return "PlayerTouch";
        case rpg::EventTrigger::EventTouch: return "EventTouch";
        case rpg::EventTrigger::Autorun: return "Autorun";
        case rpg::EventTrigger::Parallel: return "Parallel";
        default: return "None";
    }
}

const char* CommandName(rpg::EventCommandCode c) {
    switch (c) {
        case rpg::EventCommandCode::ShowText: return "Show Text";
        case rpg::EventCommandCode::ShowChoices: return "Show Choices";
        case rpg::EventCommandCode::ShowScreenText: return "Show Screen Text";
        case rpg::EventCommandCode::ClearScreenTexts: return "Clear Screen Texts";
        case rpg::EventCommandCode::Comment: return "Comment";
        case rpg::EventCommandCode::ChangeSwitch: return "Change Switch";
        case rpg::EventCommandCode::ChangeVariable: return "Change Variable";
        case rpg::EventCommandCode::ChangeGold: return "Change Gold";
        case rpg::EventCommandCode::ChangeItems: return "Change Items";
        case rpg::EventCommandCode::TransferPlayer: return "Transfer Player";
        case rpg::EventCommandCode::Wait: return "Wait";
        case rpg::EventCommandCode::PlayBGM: return "Play BGM";
        case rpg::EventCommandCode::PlaySE: return "Play SE";
        case rpg::EventCommandCode::Script: return "Script (Ruby)";
        case rpg::EventCommandCode::SpawnEntity: return "Spawn Entity";
        case rpg::EventCommandCode::MoveEntity: return "Move Entity";
        case rpg::EventCommandCode::ConditionalBranch: return "Conditional Branch";
        case rpg::EventCommandCode::BattleProcessing: return "Battle Processing";
        case rpg::EventCommandCode::ShopProcessing: return "Shop Processing";
        case rpg::EventCommandCode::ChangeActorHP: return "Change Actor HP";
        case rpg::EventCommandCode::RecoverAll: return "Recover All";
        default: return "None / Other";
    }
}

struct CmdOpt { rpg::EventCommandCode code; const char* label; };
const CmdOpt kCmdOpts[] = {
    {rpg::EventCommandCode::ShowText, "Show Text"},
    {rpg::EventCommandCode::ShowScreenText, "Show Screen Text"},
    {rpg::EventCommandCode::ClearScreenTexts, "Clear Screen Texts"},
    {rpg::EventCommandCode::Comment, "Comment"},
    {rpg::EventCommandCode::ChangeSwitch, "Change Switch"},
    {rpg::EventCommandCode::ChangeVariable, "Change Variable"},
    {rpg::EventCommandCode::ChangeGold, "Change Gold"},
    {rpg::EventCommandCode::ChangeItems, "Change Items"},
    {rpg::EventCommandCode::TransferPlayer, "Transfer Player"},
    {rpg::EventCommandCode::Wait, "Wait (frames/sec)"},
    {rpg::EventCommandCode::PlayBGM, "Play BGM"},
    {rpg::EventCommandCode::PlaySE, "Play SE"},
    {rpg::EventCommandCode::Script, "Script (Ruby)"},
    {rpg::EventCommandCode::SpawnEntity, "Spawn Entity"},
    {rpg::EventCommandCode::MoveEntity, "Move Entity"},
    {rpg::EventCommandCode::ConditionalBranch, "Conditional Branch"},
    {rpg::EventCommandCode::BattleProcessing, "Battle Processing"},
    {rpg::EventCommandCode::ShopProcessing, "Shop Processing"},
    {rpg::EventCommandCode::ChangeActorHP, "Change Actor HP"},
    {rpg::EventCommandCode::RecoverAll, "Recover All"},
    {rpg::EventCommandCode::ShowChoices, "Show Choices"},
    {rpg::EventCommandCode::ChangeExp, "Change EXP"},
    {rpg::EventCommandCode::ChangeLevel, "Change Level"},
    {rpg::EventCommandCode::RecoverAll, "Recover All"},
};

} // namespace

QtEventEditorDock::QtEventEditorDock(rpg::Engine* engine, QWidget* parent)
    : QWidget(parent), mEngine(engine) {
    buildUi();
    refresh();
}

void QtEventEditorDock::buildUi() {
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(4, 4, 4, 4);

    auto* tb = new QHBoxLayout();
    auto* bNew = new QPushButton("Neu", this);
    auto* bDel = new QPushButton("Loeschen", this);
    auto* bSave = new QPushButton("Speichern", this);
    auto* bReload = new QPushButton("Neu laden", this);
    connect(bNew, &QPushButton::clicked, this, &QtEventEditorDock::onNewEvent);
    connect(bDel, &QPushButton::clicked, this, &QtEventEditorDock::onDeleteEvent);
    connect(bSave, &QPushButton::clicked, this, &QtEventEditorDock::onSaveEvents);
    connect(bReload, &QPushButton::clicked, this, &QtEventEditorDock::onReloadEvents);
    tb->addWidget(bNew); tb->addWidget(bDel); tb->addWidget(bSave); tb->addWidget(bReload);
    tb->addStretch(1);
    root->addLayout(tb);

    auto* split = new QSplitter(Qt::Vertical, this);

    mEventList = new QListWidget(split);
    mEventList->setMaximumHeight(140);
    connect(mEventList, &QListWidget::currentRowChanged, this, [this](int) { onEventSelected(); });

    auto* props = new QGroupBox("Event-Eigenschaften", split);
    auto* form = new QFormLayout(props);
    mNameEdit = new QLineEdit(props);
    mPosX = new QSpinBox(props); mPosX->setRange(-9999, 9999);
    mPosY = new QSpinBox(props); mPosY->setRange(-9999, 9999);
    mPosZ = new QSpinBox(props); mPosZ->setRange(-9999, 9999);
    mTriggerCombo = new QComboBox(props);
    mTriggerCombo->addItem(TriggerName(rpg::EventTrigger::ActionButton), (int)rpg::EventTrigger::ActionButton);
    mTriggerCombo->addItem(TriggerName(rpg::EventTrigger::PlayerTouch), (int)rpg::EventTrigger::PlayerTouch);
    mTriggerCombo->addItem(TriggerName(rpg::EventTrigger::EventTouch), (int)rpg::EventTrigger::EventTouch);
    mTriggerCombo->addItem(TriggerName(rpg::EventTrigger::Autorun), (int)rpg::EventTrigger::Autorun);
    mTriggerCombo->addItem(TriggerName(rpg::EventTrigger::Parallel), (int)rpg::EventTrigger::Parallel);
    mPageCombo = new QComboBox(props);
    connect(mPageCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &QtEventEditorDock::onPageChanged);
    auto* applyEv = new QPushButton("Event uebernehmen", props);
    connect(applyEv, &QPushButton::clicked, this, &QtEventEditorDock::onApplyEventProps);
    form->addRow("Name", mNameEdit);
    form->addRow("X", mPosX);
    form->addRow("Y", mPosY);
    form->addRow("Z", mPosZ);
    form->addRow("Seite", mPageCombo);
    form->addRow("Trigger", mTriggerCombo);
    form->addRow(applyEv);

    auto* cmdBox = new QGroupBox("Befehle (aktuelle Seite)", split);
    auto* cmdLay = new QVBoxLayout(cmdBox);
    mCommandList = new QListWidget(cmdBox);
    connect(mCommandList, &QListWidget::currentRowChanged, this, [this](int) { onCommandSelected(); });
    cmdLay->addWidget(mCommandList, 1);
    auto* cmdTb = new QHBoxLayout();
    auto* addC = new QPushButton("+", cmdBox);
    auto* remC = new QPushButton("-", cmdBox);
    auto* upC = new QPushButton("▲", cmdBox);
    auto* dnC = new QPushButton("▼", cmdBox);
    connect(addC, &QPushButton::clicked, this, &QtEventEditorDock::onAddCommand);
    connect(remC, &QPushButton::clicked, this, &QtEventEditorDock::onRemoveCommand);
    connect(upC, &QPushButton::clicked, this, &QtEventEditorDock::onMoveCommandUp);
    connect(dnC, &QPushButton::clicked, this, &QtEventEditorDock::onMoveCommandDown);
    cmdTb->addWidget(addC); cmdTb->addWidget(remC); cmdTb->addWidget(upC); cmdTb->addWidget(dnC);
    cmdTb->addStretch(1);
    cmdLay->addLayout(cmdTb);

    auto* cmdForm = new QFormLayout();
    mCmdCodeCombo = new QComboBox(cmdBox);
    for (const auto& o : kCmdOpts)
        mCmdCodeCombo->addItem(o.label, static_cast<int>(o.code));
    mCmdText = new QPlainTextEdit(cmdBox);
    mCmdText->setMaximumHeight(80);
    mCmdP1 = new QSpinBox(cmdBox); mCmdP1->setRange(-999999, 999999);
    mCmdP2 = new QSpinBox(cmdBox); mCmdP2->setRange(-999999, 999999);
    mCmdP3 = new QSpinBox(cmdBox); mCmdP3->setRange(-999999, 999999);
    auto* applyCmd = new QPushButton("Befehl speichern", cmdBox);
    connect(applyCmd, &QPushButton::clicked, this, &QtEventEditorDock::onApplyCommand);
    cmdForm->addRow("Code", mCmdCodeCombo);
    cmdForm->addRow("Text / Pfad", mCmdText);
    cmdForm->addRow("Param1", mCmdP1);
    cmdForm->addRow("Param2", mCmdP2);
    cmdForm->addRow("Param3", mCmdP3);
    cmdForm->addRow(applyCmd);
    cmdLay->addLayout(cmdForm);

    mInfoLabel = new QLabel(
        "Param-Hinweise: Switch(id,0/1) | Gold(amount) | Wait(sec*10) | "
        "Transfer(x,y,map) | Items(id,amount) | BGM/SE(text=path)", cmdBox);
    mInfoLabel->setWordWrap(true);
    cmdLay->addWidget(mInfoLabel);

    split->addWidget(mEventList);
    split->addWidget(props);
    split->addWidget(cmdBox);
    split->setStretchFactor(2, 1);
    root->addWidget(split, 1);
}

void QtEventEditorDock::refresh() {
    rebuildEventList();
}

void QtEventEditorDock::rebuildEventList() {
    mEventList->blockSignals(true);
    mEventList->clear();
    auto& events = rpg::EventSystem::Get().GetEvents();
    int selectRow = -1;
    for (size_t i = 0; i < events.size(); ++i) {
        const auto& e = events[i];
        mEventList->addItem(QString("%1: %2 @(%3,%4,%5)")
            .arg(e.id).arg(QString::fromStdString(e.name))
            .arg(e.x).arg(e.y).arg(e.z));
        mEventList->item(static_cast<int>(i))->setData(Qt::UserRole, e.id);
        if (e.id == mSelectedEventId) selectRow = static_cast<int>(i);
    }
    mEventList->blockSignals(false);
    if (selectRow >= 0) mEventList->setCurrentRow(selectRow);
    else if (!events.empty()) {
        mEventList->setCurrentRow(0);
        onEventSelected();
    } else {
        mSelectedEventId = -1;
        mCommandList->clear();
    }
}

int QtEventEditorDock::currentEventId() const {
    auto* item = mEventList->currentItem();
    return item ? item->data(Qt::UserRole).toInt() : -1;
}

int QtEventEditorDock::currentCommandIndex() const {
    return mCommandList->currentRow();
}

void QtEventEditorDock::onEventSelected() {
    mSelectedEventId = currentEventId();
    syncEventProps();
    rebuildCommandList();
}

void QtEventEditorDock::syncEventProps() {
    auto* ev = rpg::EventSystem::Get().GetEvent(mSelectedEventId);
    if (!ev) return;
    mSyncing = true;
    mNameEdit->setText(QString::fromStdString(ev->name));
    mPosX->setValue(ev->x);
    mPosY->setValue(ev->y);
    mPosZ->setValue(ev->z);
    mPageCombo->clear();
    for (size_t i = 0; i < ev->pages.size(); ++i)
        mPageCombo->addItem(QString("Seite %1").arg(static_cast<int>(i) + 1), static_cast<int>(i));
    if (ev->pages.empty()) {
        rpg::EventPage page;
        page.id = 0;
        ev->pages.push_back(page);
        mPageCombo->addItem("Seite 1", 0);
    }
    if (mSelectedPage >= static_cast<int>(ev->pages.size())) mSelectedPage = 0;
    mPageCombo->setCurrentIndex(mSelectedPage);
    auto& page = ev->pages[static_cast<size_t>(mSelectedPage)];
    int ti = mTriggerCombo->findData(static_cast<int>(page.trigger));
    mTriggerCombo->setCurrentIndex(ti >= 0 ? ti : 0);
    mSyncing = false;
}

void QtEventEditorDock::onPageChanged(int index) {
    if (mSyncing || index < 0) return;
    mSelectedPage = index;
    syncEventProps();
    rebuildCommandList();
}

void QtEventEditorDock::rebuildCommandList() {
    mCommandList->blockSignals(true);
    mCommandList->clear();
    auto* ev = rpg::EventSystem::Get().GetEvent(mSelectedEventId);
    if (!ev || mSelectedPage < 0 || mSelectedPage >= static_cast<int>(ev->pages.size())) {
        mCommandList->blockSignals(false);
        return;
    }
    auto& list = ev->pages[static_cast<size_t>(mSelectedPage)].list;
    for (size_t i = 0; i < list.size(); ++i) {
        const auto& c = list[i];
        QString line = QString("%1. %2").arg(static_cast<int>(i) + 1).arg(CommandName(c.code));
        if (!c.text.empty()) line += "  \"" + QString::fromStdString(c.text).left(40) + "\"";
        if (c.param1 || c.param2 || c.param3)
            line += QString("  [%1,%2,%3]").arg(c.param1).arg(c.param2).arg(c.param3);
        mCommandList->addItem(line);
    }
    mCommandList->blockSignals(false);
    if (mSelectedCommand >= 0 && mSelectedCommand < mCommandList->count())
        mCommandList->setCurrentRow(mSelectedCommand);
    else if (mCommandList->count() > 0)
        mCommandList->setCurrentRow(0);
}

void QtEventEditorDock::onCommandSelected() {
    mSelectedCommand = currentCommandIndex();
    syncCommandProps();
}

void QtEventEditorDock::syncCommandProps() {
    auto* ev = rpg::EventSystem::Get().GetEvent(mSelectedEventId);
    if (!ev || mSelectedPage < 0 || mSelectedPage >= static_cast<int>(ev->pages.size())) return;
    auto& list = ev->pages[static_cast<size_t>(mSelectedPage)].list;
    if (mSelectedCommand < 0 || mSelectedCommand >= static_cast<int>(list.size())) return;
    const auto& c = list[static_cast<size_t>(mSelectedCommand)];
    mSyncing = true;
    int ci = mCmdCodeCombo->findData(static_cast<int>(c.code));
    mCmdCodeCombo->setCurrentIndex(ci >= 0 ? ci : 0);
    mCmdText->setPlainText(QString::fromStdString(c.text));
    mCmdP1->setValue(c.param1);
    mCmdP2->setValue(c.param2);
    mCmdP3->setValue(c.param3);
    mSyncing = false;
}

void QtEventEditorDock::onNewEvent() {
    auto& es = rpg::EventSystem::Get();
    rpg::MapEvent ev;
    int maxId = 0;
    for (const auto& e : es.GetEvents()) maxId = std::max(maxId, e.id);
    ev.id = maxId + 1;
    ev.name = "EV" + std::to_string(ev.id);
    ev.x = 0; ev.y = 0; ev.z = 0;
    ev.worldPos = rpg::Vec3(0, 0, 0);
    rpg::EventPage page;
    page.id = 0;
    page.trigger = rpg::EventTrigger::ActionButton;
    rpg::EventCommand cmd;
    cmd.code = rpg::EventCommandCode::ShowText;
    cmd.text = "Hallo! (neues Event)";
    page.list.push_back(cmd);
    ev.pages.push_back(page);
    es.AddEvent(ev);
    mSelectedEventId = ev.id;
    rebuildEventList();
    emit logMessage(QString("Event angelegt: %1").arg(ev.id));
    emit eventsChanged();
}

void QtEventEditorDock::onDeleteEvent() {
    if (mSelectedEventId < 0) return;
    if (QMessageBox::question(this, "Event loeschen",
            QString("Event %1 loeschen?").arg(mSelectedEventId)) != QMessageBox::Yes)
        return;
    rpg::EventSystem::Get().RemoveEvent(mSelectedEventId);
    mSelectedEventId = -1;
    rebuildEventList();
    emit logMessage("Event geloescht.");
    emit eventsChanged();
}

void QtEventEditorDock::onSaveEvents() {
    if (!mEngine) return;
    onApplyEventProps();
    const int mapId = rpg::Database::Get().System().startMapId;
    const std::string pp = mEngine->GetProject().GetProjectPath();
    if (pp.empty()) {
        QMessageBox::information(this, "Speichern", "Kein Projekt geladen.");
        return;
    }
    rpg::EventSystem::Get().SaveMapEvents(mapId, pp);
    emit logMessage(QString("Events gespeichert (Map %1).").arg(mapId));
}

void QtEventEditorDock::onReloadEvents() {
    if (!mEngine) return;
    const int mapId = rpg::Database::Get().System().startMapId;
    const std::string pp = mEngine->GetProject().GetProjectPath();
    rpg::EventSystem::Get().LoadMapEvents(mapId, pp.empty() ? "." : pp);
    mSelectedEventId = -1;
    rebuildEventList();
    emit logMessage("Events neu geladen.");
    emit eventsChanged();
}

void QtEventEditorDock::onApplyEventProps() {
    auto* ev = rpg::EventSystem::Get().GetEvent(mSelectedEventId);
    if (!ev) return;
    ev->name = mNameEdit->text().toStdString();
    ev->x = mPosX->value();
    ev->y = mPosY->value();
    ev->z = mPosZ->value();
    ev->worldPos = rpg::Vec3(static_cast<float>(ev->x), static_cast<float>(ev->y), static_cast<float>(ev->z));
    if (mSelectedPage >= 0 && mSelectedPage < static_cast<int>(ev->pages.size())) {
        auto& page = ev->pages[static_cast<size_t>(mSelectedPage)];
        page.trigger = static_cast<rpg::EventTrigger>(mTriggerCombo->currentData().toInt());
    }
    rebuildEventList();
    emit logMessage(QString("Event %1 aktualisiert.").arg(ev->id));
    emit eventsChanged();
}

void QtEventEditorDock::onAddCommand() {
    auto* ev = rpg::EventSystem::Get().GetEvent(mSelectedEventId);
    if (!ev || mSelectedPage < 0 || mSelectedPage >= static_cast<int>(ev->pages.size())) return;
    rpg::EventCommand cmd;
    cmd.code = rpg::EventCommandCode::ShowText;
    cmd.text = "Neuer Befehl";
    auto& list = ev->pages[static_cast<size_t>(mSelectedPage)].list;
    list.push_back(cmd);
    mSelectedCommand = static_cast<int>(list.size()) - 1;
    rebuildCommandList();
    syncCommandProps();
}

void QtEventEditorDock::onRemoveCommand() {
    auto* ev = rpg::EventSystem::Get().GetEvent(mSelectedEventId);
    if (!ev || mSelectedPage < 0 || mSelectedPage >= static_cast<int>(ev->pages.size())) return;
    auto& list = ev->pages[static_cast<size_t>(mSelectedPage)].list;
    if (mSelectedCommand < 0 || mSelectedCommand >= static_cast<int>(list.size())) return;
    list.erase(list.begin() + mSelectedCommand);
    mSelectedCommand = std::min(mSelectedCommand, static_cast<int>(list.size()) - 1);
    rebuildCommandList();
}

void QtEventEditorDock::onMoveCommandUp() {
    auto* ev = rpg::EventSystem::Get().GetEvent(mSelectedEventId);
    if (!ev || mSelectedPage < 0 || mSelectedPage >= static_cast<int>(ev->pages.size())) return;
    auto& list = ev->pages[static_cast<size_t>(mSelectedPage)].list;
    if (mSelectedCommand <= 0 || mSelectedCommand >= static_cast<int>(list.size())) return;
    std::swap(list[static_cast<size_t>(mSelectedCommand)], list[static_cast<size_t>(mSelectedCommand - 1)]);
    --mSelectedCommand;
    rebuildCommandList();
}

void QtEventEditorDock::onMoveCommandDown() {
    auto* ev = rpg::EventSystem::Get().GetEvent(mSelectedEventId);
    if (!ev || mSelectedPage < 0 || mSelectedPage >= static_cast<int>(ev->pages.size())) return;
    auto& list = ev->pages[static_cast<size_t>(mSelectedPage)].list;
    if (mSelectedCommand < 0 || mSelectedCommand >= static_cast<int>(list.size()) - 1) return;
    std::swap(list[static_cast<size_t>(mSelectedCommand)], list[static_cast<size_t>(mSelectedCommand + 1)]);
    ++mSelectedCommand;
    rebuildCommandList();
}

void QtEventEditorDock::onApplyCommand() {
    auto* ev = rpg::EventSystem::Get().GetEvent(mSelectedEventId);
    if (!ev || mSelectedPage < 0 || mSelectedPage >= static_cast<int>(ev->pages.size())) return;
    auto& list = ev->pages[static_cast<size_t>(mSelectedPage)].list;
    if (mSelectedCommand < 0 || mSelectedCommand >= static_cast<int>(list.size())) return;
    auto& c = list[static_cast<size_t>(mSelectedCommand)];
    c.code = static_cast<rpg::EventCommandCode>(mCmdCodeCombo->currentData().toInt());
    c.text = mCmdText->toPlainText().toStdString();
    c.param1 = mCmdP1->value();
    c.param2 = mCmdP2->value();
    c.param3 = mCmdP3->value();
    rebuildCommandList();
    emit logMessage(QString("Befehl %1 gespeichert.").arg(mSelectedCommand + 1));
}

} // namespace qt_editor
