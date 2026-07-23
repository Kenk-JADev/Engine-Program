#pragma once
// Code-Workspace: ersetzt den alten ImGui-"Game Scene"-Schwerpunkt.
// Dient dem Editieren von Ruby-Spiellogik und C++-Engine-API-Referenz/Snippets
// in nativen Qt-Widgets (QPlainTextEdit + Dateibaum).

#include <QWidget>
#include <QString>
#include <memory>
#include <vector>

class QListWidget;
class QPlainTextEdit;
class QLabel;
class QComboBox;
class QLineEdit;
class QSplitter;
class QToolBar;
class QAction;
class QTabWidget;

namespace rpg {
class Engine;
class ScriptManager;
}

namespace qt_editor {

class QtSyntaxHighlighter;

enum class CodeLanguage {
    Ruby,
    Cpp
};

class QtCodeWorkspace : public QWidget {
    Q_OBJECT
public:
    explicit QtCodeWorkspace(rpg::Engine* engine, QWidget* parent = nullptr);

    // Scripts vom ScriptManager neu laden und Liste aufbauen
    void refresh();
    // Aktuellen Buffer speichern (falls Ruby und geaendert)
    bool saveCurrent();
    bool saveAll();
    // Ruby-Scripts ausfuehren (Playtest-Vorbereitung)
    void hotReloadAll(); // speichern + ExecuteAllScripts

    bool hasUnsavedChanges() const;
    void showRubyError(const QString& err);
    CodeLanguage currentLanguage() const { return mLanguage; }

signals:
    void logMessage(const QString& msg);
    void scriptsChanged();

public slots:
    void setLanguage(int index); // 0=Ruby, 1=C++

private slots:
    void onFileSelected();
    void onTextChanged();
    void onNewRubyScript();
    void onRenameRubyScript();   // F2 / Doppelklick (XP: Enter auf Listeneintrag)
    void onDeleteRubyScript();
    void onReloadFromDisk();
    void onOpenExternal();
    void onInsertSnippet(int index);
    void onFind();
    void onFindNext();
    void onHotReload();

private:
    void buildUi();
    void loadRubyFile(int index);
    void loadCppReference(int index);
    void flushCurrentToManager();
    void updateDirtyLabel();
    void populateRubyList();
    void populateCppList();
    void applyEditorFont();

    rpg::Engine* mEngine = nullptr;

    QComboBox* mLangCombo = nullptr;
    QListWidget* mFileList = nullptr;
    QPlainTextEdit* mEditor = nullptr;
    QLabel* mPathLabel = nullptr;
    QLabel* mDirtyLabel = nullptr;
    QComboBox* mSnippetCombo = nullptr;
    QAction* mSaveAction = nullptr;
    QAction* mNewAction = nullptr;
    QAction* mDeleteAction = nullptr;
    QLineEdit* mFindEdit = nullptr;
    QString mLastFind;
    int mFindPos = 0;

    CodeLanguage mLanguage = CodeLanguage::Ruby;
    int mCurrentIndex = -1;
    bool mLoading = false;
    bool mDirty = false;
    QString mCurrentBuffer;
    QString mCurrentPath;
    QString mCurrentName;

    QtSyntaxHighlighter* mHighlighter = nullptr;

    // C++ API-Referenz (eingebettet, read-only)
    struct CppDoc {
        QString name;
        QString pathHint;
        QString content;
    };
    std::vector<CppDoc> mCppDocs;
    void ensureCppDocs();
    void updateHighlighter();
};

} // namespace qt_editor
