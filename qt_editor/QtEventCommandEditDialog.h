#pragma once
// Parameterdialog für einen Event-Befehl.
// Wird generisch aus den ArgSpecs des Katalogs aufgebaut.
// "Bedingung" (111) bekommt einen eigenen Formular-Bereich (13 Typen).

#include <QDialog>
#include <QHash>

#include "QtEventCommandCatalog.h"

class QFormLayout;
class QStackedWidget;
class QComboBox;
class QCheckBox;

namespace qt_editor {

class QtEventCommandEditDialog : public QDialog {
    Q_OBJECT
public:
    QtEventCommandEditDialog(const rpg::EventCommand& cmd, int eventContext, QWidget* parent = nullptr);

    /// Ergebnis nach OK (bereits finalisiert)
    rpg::EventCommand command() const { return mResult; }

    /// Modal einen Befehl bearbeiten. Liefert false bei Abbruch.
    /// inOut enthält danach den finalisierten Befehl.
    static bool EditCommand(QWidget* parent, rpg::EventCommand& inOut, int eventContext = 0);

private slots:
    void onOk();
    void onBranchTypeChanged(int index);
    void onActorKindChanged(int index);
    void onVarOperandKindChanged(int index);

private:
    QWidget* buildArgRow(const ArgSpec& spec, QFormLayout* form);
    void buildConditionalBranch();
    void collectGeneric(const CommandSpec& spec);
    void collectConditionalBranch();

    rpg::EventCommand mOriginal;
    rpg::EventCommand mResult;
    int mEventContext = 0;

    // generische Widgets: key -> Eingabewidget
    QHash<QString, QWidget*> mFields;

    // Bedingungs-Widgets
    QComboBox* mBranchType = nullptr;
    QStackedWidget* mBranchStack = nullptr;
    // Typ 1 Variable
    QStackedWidget* mVarOperandStack = nullptr;
    // Typ 4 Akteur
    QStackedWidget* mActorValueStack = nullptr;
    QCheckBox* mBranchElse = nullptr;
};

} // namespace qt_editor
