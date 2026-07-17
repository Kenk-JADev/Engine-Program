#include "QtEventEditorDock.h"

#include "QtEventEditorDialog.h"

#include "rpgmaker3d/Engine.h"
#include "rpgmaker3d/EventSystem.h"
#include "rpgmaker3d/Project.h"
#include "rpgmaker3d/Database.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QListWidget>
#include <QMessageBox>
#include <QPushButton>
#include <QSpinBox>
#include <QVBoxLayout>
#include <algorithm>

namespace qt_editor {

#ifndef QL
#define QL(x) QStringLiteral(x)
#endif

QtEventEditorDock::QtEventEditorDock(rpg::Engine* engine, QWidget* parent)
    : QWidget(parent), mEngine(engine) {

    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(4, 4, 4, 4);

    // ---- Werkzeugzeile -----------------------------------------------------
    auto* tb = new QHBoxLayout();
    const struct { const char* text; const char* tip; void (QtEventEditorDock::*slot)(); } btns[] = {
        {"Neu", "Neues Event anlegen", &QtEventEditorDock::onNewEvent},
        {"Bearbeiten ...", "Event-Dialog öffnen (Seiten, Bedingungen, Befehle)",
            &QtEventEditorDock::onEditEvent},
        {"Duplizieren", "Event kopieren", &QtEventEditorDock::onDuplicateEvent},
        {"Löschen", "Event entfernen", &QtEventEditorDock::onDeleteEvent},
    };
    for (const auto& b : btns) {
        auto* btn = new QPushButton(QLatin1String(b.text), this);
        btn->setToolTip(QLatin1String(b.tip));
        connect(btn, &QPushButton::clicked, this, b.slot);
        tb->addWidget(btn);
    }
    tb->addStretch(1);
    root->addLayout(tb);

    auto* tb2 = new QHBoxLayout();
    auto* bSave = new QPushButton(QL("Speichern"), this);
    bSave->setToolTip(QL("Events der Karte in events_map<N>.json schreiben"));
    connect(bSave, &QPushButton::clicked, this, &QtEventEditorDock::onSaveEvents);
    auto* bReload = new QPushButton(QL("Neu laden"), this);
    connect(bReload, &QPushButton::clicked, this, &QtEventEditorDock::onReloadEvents);
    tb2->addWidget(bSave);
    tb2->addWidget(bReload);
    tb2->addStretch(1);
    root->addLayout(tb2);

    // ---- Event-Liste --------------------------------------------------------
    mEventList = new QListWidget(this);
    mEventList->setAlternatingRowColors(true);
    connect(mEventList, &QListWidget::currentRowChanged,
            this, [this](int) { onEventSelected(); });
    connect(mEventList, &QListWidget::itemDoubleClicked,
            this, [this](QListWidgetItem*) { onEditEvent(); });
    root->addWidget(mEventList, 1);

    // ---- Position -----------------------------------------------------------
    auto* posRow = new QHBoxLayout();
    posRow->addWidget(new QLabel(QL("Position X/Y/Z:"), this));
    mPosX = new QSpinBox(this); mPosX->setRange(-9999, 9999);
    mPosY = new QSpinBox(this); mPosY->setRange(-9999, 9999);
    mPosZ = new QSpinBox(this); mPosZ->setRange(-9999, 9999);
    posRow->addWidget(mPosX);
    posRow->addWidget(mPosY);
    posRow->addWidget(mPosZ);
    auto* applyPos = new QPushButton(QL("Setzen"), this);
    connect(applyPos, &QPushButton::clicked, this, &QtEventEditorDock::onApplyPosition);
    posRow->addWidget(applyPos);
    root->addLayout(posRow);

    mInfoLabel = new QLabel(
        QL("Doppelklick auf ein Event öffnet den Event-Dialog\n"
           "(Seiten, Bedingungen, Grafik, Bewegung, Befehlsliste)."), this);
    mInfoLabel->setWordWrap(true);
    root->addWidget(mInfoLabel);

    refresh();
}

void QtEventEditorDock::refresh() {
    if (!mEngine || !mEngine->IsInitialized()) return;
    rebuildEventList();
}

void QtEventEditorDock::rebuildEventList() {
    mEventList->blockSignals(true);
    mEventList->clear();
    auto& events = rpg::EventSystem::Get().GetEvents();
    int selectRow = -1;
    for (size_t i = 0; i < events.size(); ++i) {
        const auto& e = events[i];
        QString state;
        if (e.erased) state = QL("  (gelöscht)");
        auto* item = new QListWidgetItem(
            QL("%1: %2 @(%3,%4,%5)%6")
                .arg(e.id).arg(QString::fromStdString(e.name))
                .arg(e.x).arg(e.y).arg(e.z).arg(state), mEventList);
        item->setData(Qt::UserRole, e.id);
        if (e.id == mSelectedEventId) selectRow = (int)i;
    }
    mEventList->blockSignals(false);
    if (selectRow >= 0) mEventList->setCurrentRow(selectRow);
    else if (mEventList->count() > 0) mEventList->setCurrentRow(0);
    if (mEventList->count() == 0) {
        mSelectedEventId = -1;
        mInfoLabel->setText(QL("Noch keine Events. 'Neu' legt ein NPC-Event an."));
    }
}

int QtEventEditorDock::currentEventId() const {
    auto* item = mEventList->currentItem();
    return item ? item->data(Qt::UserRole).toInt() : -1;
}

void QtEventEditorDock::onEventSelected() {
    mSelectedEventId = currentEventId();
    auto* ev = rpg::EventSystem::Get().GetEvent(mSelectedEventId);
    if (!ev) return;
    mSyncing = true;
    mPosX->setValue(ev->x);
    mPosY->setValue(ev->y);
    mPosZ->setValue(ev->z);
    const size_t pages = ev->pages.size();
    mInfoLabel->setText(QL("Event %1 \"%2\": %3 Seite(n), Trigger: %4")
        .arg(ev->id).arg(QString::fromStdString(ev->name)).arg(pages)
        .arg(pages > 0 ? QString::number((int)ev->pages[0].trigger) : QL("-")));
    mSyncing = false;
}

void QtEventEditorDock::onEditEvent() {
    auto* ev = rpg::EventSystem::Get().GetEvent(mSelectedEventId);
    if (!ev) {
        emit logMessage(QL("Kein Event ausgewählt."));
        return;
    }
    rpg::MapEvent copy = *ev;
    if (QtEventEditorDialog::EditEvent(this, copy)) {
        *ev = copy;
        rpg::EventSystem::Get().RefreshAllPages();
        rebuildEventList();
        emit logMessage(QL("Event %1 \"%2\" gespeichert.").arg(ev->id)
                            .arg(QString::fromStdString(ev->name)));
        emit eventsChanged();
    }
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

    // Direkt den Editor öffnen, damit das Event sinnvoll befüllt wird
    if (auto* created = es.GetEvent(ev.id)) {
        rpg::MapEvent copy = *created;
        if (QtEventEditorDialog::EditEvent(this, copy)) *created = copy;
    }
    rebuildEventList();
    emit logMessage(QL("Event angelegt: %1").arg(ev.id));
    emit eventsChanged();
}

void QtEventEditorDock::onDuplicateEvent() {
    auto* src = rpg::EventSystem::Get().GetEvent(mSelectedEventId);
    if (!src) return;
    rpg::MapEvent ev = *src;
    auto& es = rpg::EventSystem::Get();
    int maxId = 0;
    for (const auto& e : es.GetEvents()) maxId = std::max(maxId, e.id);
    ev.id = maxId + 1;
    ev.name += "_kopie";
    es.AddEvent(ev);
    mSelectedEventId = ev.id;
    rebuildEventList();
    emit logMessage(QL("Event dupliziert: %1 -> %2").arg(src->id).arg(ev.id));
    emit eventsChanged();
}

void QtEventEditorDock::onDeleteEvent() {
    if (mSelectedEventId < 0) return;
    if (QMessageBox::question(this, QL("Event löschen"),
            QL("Event %1 wirklich löschen?").arg(mSelectedEventId)) != QMessageBox::Yes)
        return;
    rpg::EventSystem::Get().RemoveEvent(mSelectedEventId);
    mSelectedEventId = -1;
    rebuildEventList();
    emit logMessage(QL("Event gelöscht."));
    emit eventsChanged();
}

void QtEventEditorDock::onSaveEvents() {
    if (!mEngine) return;
    const std::string pp = mEngine->GetProject().GetProjectPath();
    if (pp.empty()) {
        QMessageBox::information(this, QL("Speichern"),
                                 QL("Kein Projekt geladen - bitte zuerst öffnen."));
        return;
    }
    const int mapId = rpg::Database::Get().System().startMapId;
    rpg::EventSystem::Get().SaveMapEvents(mapId > 0 ? mapId : 1, pp);
    emit logMessage(QL("Events gespeichert (Karte %1).").arg(mapId > 0 ? mapId : 1));
}

void QtEventEditorDock::onReloadEvents() {
    if (!mEngine) return;
    const int mapId = rpg::Database::Get().System().startMapId;
    const std::string pp = mEngine->GetProject().GetProjectPath();
    rpg::EventSystem::Get().LoadMapEvents(mapId > 0 ? mapId : 1, pp.empty() ? "." : pp);
    mSelectedEventId = -1;
    rebuildEventList();
    emit logMessage(QL("Events neu geladen."));
    emit eventsChanged();
}

void QtEventEditorDock::onApplyPosition() {
    if (mSyncing) return;
    auto* ev = rpg::EventSystem::Get().GetEvent(mSelectedEventId);
    if (!ev) return;
    ev->x = mPosX->value();
    ev->y = mPosY->value();
    ev->z = mPosZ->value();
    ev->worldPos = rpg::Vec3((float)ev->x, (float)ev->y, (float)ev->z);
    rebuildEventList();
    emit logMessage(QL("Event %1 -> (%2,%3,%4)").arg(ev->id).arg(ev->x).arg(ev->y).arg(ev->z));
    emit eventsChanged();
}

} // namespace qt_editor
