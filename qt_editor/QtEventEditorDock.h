#pragma once
// Event-Verwaltung als Dock: Liste aller Map-Events, Neu/Löschen,
// Position, Speichern/Laden. Der eigentliche Editor öffnet modal
// (QtEventEditorDialog, XP-artig mit Seiten + Befehlsliste).

#include <QWidget>
#include <QString>

class QListWidget;
class QSpinBox;
class QLabel;

namespace rpg { class Engine; }

namespace qt_editor {

class QtEventEditorDock : public QWidget {
    Q_OBJECT
public:
    explicit QtEventEditorDock(rpg::Engine* engine, QWidget* parent = nullptr);

    void refresh();

signals:
    void logMessage(const QString& msg);
    void eventsChanged();

private slots:
    void onEventSelected();
    void onEditEvent();       // Doppelklick / Button -> XP-Eventdialog
    void onNewEvent();
    void onDeleteEvent();
    void onDuplicateEvent();
    void onSaveEvents();
    void onReloadEvents();
    void onApplyPosition();

private:
    int currentEventId() const;
    void rebuildEventList();

    rpg::Engine* mEngine = nullptr;
    QListWidget* mEventList = nullptr;
    QSpinBox* mPosX = nullptr;
    QSpinBox* mPosY = nullptr;
    QSpinBox* mPosZ = nullptr;
    QLabel* mInfoLabel = nullptr;
    int mSelectedEventId = -1;
    bool mSyncing = false;
};

} // namespace qt_editor
