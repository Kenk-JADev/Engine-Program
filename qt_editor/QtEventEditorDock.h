#pragma once
// Event-Editor: Map-Events listen, Seiten, Befehls-Liste (RPG-Maker-Style).

#include <QWidget>
#include <QString>

class QListWidget;
class QLineEdit;
class QSpinBox;
class QComboBox;
class QPlainTextEdit;
class QLabel;
class QPushButton;
class QCheckBox;
class QDoubleSpinBox;

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
    void onNewEvent();
    void onDeleteEvent();
    void onSaveEvents();
    void onReloadEvents();
    void onApplyEventProps();
    void onCommandSelected();
    void onAddCommand();
    void onRemoveCommand();
    void onMoveCommandUp();
    void onMoveCommandDown();
    void onApplyCommand();
    void onPageChanged(int index);

private:
    void buildUi();
    void rebuildEventList();
    void rebuildCommandList();
    void syncEventProps();
    void syncCommandProps();
    int currentEventId() const;
    int currentCommandIndex() const;

    rpg::Engine* mEngine = nullptr;

    QListWidget* mEventList = nullptr;
    QListWidget* mCommandList = nullptr;
    QComboBox* mPageCombo = nullptr;

    QLineEdit* mNameEdit = nullptr;
    QSpinBox* mPosX = nullptr;
    QSpinBox* mPosY = nullptr;
    QSpinBox* mPosZ = nullptr;
    QComboBox* mTriggerCombo = nullptr;
    QCheckBox* mCondSwitchCheck = nullptr;
    QSpinBox* mCondSwitchId = nullptr;
    QCheckBox* mSelfSwitchCheck = nullptr;
    QComboBox* mSelfSwitchChar = nullptr;
    QLineEdit* mMoveRouteEdit = nullptr;

    QComboBox* mCmdCodeCombo = nullptr;
    QPlainTextEdit* mCmdText = nullptr;
    QSpinBox* mCmdP1 = nullptr;
    QSpinBox* mCmdP2 = nullptr;
    QSpinBox* mCmdP3 = nullptr;
    QLabel* mInfoLabel = nullptr;

    int mSelectedEventId = -1;
    int mSelectedPage = 0;
    int mSelectedCommand = -1;
    bool mSyncing = false;
};

} // namespace qt_editor
