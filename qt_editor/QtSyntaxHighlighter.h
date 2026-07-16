#pragma once
// Einfacher QSyntaxHighlighter fuer Ruby- und C++-Code im Code-Workspace.

#include <QSyntaxHighlighter>
#include <QTextCharFormat>
#include <QRegularExpression>
#include <vector>

namespace qt_editor {

enum class HighlightLanguage {
    None,
    Ruby,
    Cpp
};

class QtSyntaxHighlighter : public QSyntaxHighlighter {
    Q_OBJECT
public:
    explicit QtSyntaxHighlighter(QTextDocument* parent = nullptr);

    void setLanguage(HighlightLanguage lang);

protected:
    void highlightBlock(const QString& text) override;

private:
    struct Rule {
        QRegularExpression pattern;
        QTextCharFormat format;
    };

    void rebuildRules();

    HighlightLanguage mLang = HighlightLanguage::None;
    std::vector<Rule> mRules;
    QTextCharFormat mFmtKeyword;
    QTextCharFormat mFmtType;
    QTextCharFormat mFmtString;
    QTextCharFormat mFmtComment;
    QTextCharFormat mFmtNumber;
    QTextCharFormat mFmtPreprocessor;
    QTextCharFormat mFmtFunction;
    QRegularExpression mCommentStart;
    QRegularExpression mCommentEnd;
    QTextCharFormat mMultiComment;
};

} // namespace qt_editor
