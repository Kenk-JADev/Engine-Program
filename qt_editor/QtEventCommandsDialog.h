#pragma once
// "Event-Befehle" Auswahldialog im Stil von RPG Maker XP:
// Drei Seiten (Tabs) mit einer Button je Befehl (siehe QtEventCommandCatalog).
// OK waehlt den markierten Befehl, Abbrechen verwirft.

#include <QDialog>

class QTabWidget;
class QPushButton;
class QListWidget;

#include "rpgmaker3d/EventSystem.h"

namespace qt_editor {

class QtEventCommandsDialog : public QDialog {
    Q_OBJECT
public:
    explicit QtEventCommandsDialog(QWidget* parent = nullptr);

    /// Gewaehlter Befehlscode (gueltig nur bei Accepted)
    rpg::EventCommandCode selectedCode() const { return mSelected; }

    /// Bequemer statischer Aufruf: liefert false bei Abbruch.
    static bool ChooseCommand(QWidget* parent, rpg::EventCommandCode& outCode);

private slots:
    void onSelectionChanged();
    void onDoubleClicked();
    void onOk();

private:
    QTabWidget* mTabs = nullptr;
    QPushButton* mOkButton = nullptr;
    rpg::EventCommandCode mSelected = rpg::EventCommandCode::None;
};

} // namespace qt_editor
