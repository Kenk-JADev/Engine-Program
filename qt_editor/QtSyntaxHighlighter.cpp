#include "QtSyntaxHighlighter.h"

namespace qt_editor {

QtSyntaxHighlighter::QtSyntaxHighlighter(QTextDocument* parent)
    : QSyntaxHighlighter(parent) {
    mFmtKeyword.setForeground(QColor(86, 156, 214));
    mFmtKeyword.setFontWeight(QFont::Bold);

    mFmtType.setForeground(QColor(78, 201, 176));

    mFmtString.setForeground(QColor(206, 145, 120));

    mFmtComment.setForeground(QColor(106, 153, 85));
    mFmtComment.setFontItalic(true);

    mFmtNumber.setForeground(QColor(181, 206, 168));

    mFmtPreprocessor.setForeground(QColor(155, 155, 155));

    mFmtFunction.setForeground(QColor(220, 220, 170));

    mMultiComment = mFmtComment;
    mCommentStart = QRegularExpression(QStringLiteral("/\\*"));
    mCommentEnd = QRegularExpression(QStringLiteral("\\*/"));
}

void QtSyntaxHighlighter::setLanguage(HighlightLanguage lang) {
    if (mLang == lang) return;
    mLang = lang;
    rebuildRules();
    rehighlight();
}

void QtSyntaxHighlighter::rebuildRules() {
    mRules.clear();
    if (mLang == HighlightLanguage::None) return;

    auto addKeywords = [this](const QStringList& words, const QTextCharFormat& fmt) {
        for (const QString& w : words) {
            Rule r;
            r.pattern = QRegularExpression(QStringLiteral("\\b") + w + QStringLiteral("\\b"));
            r.format = fmt;
            mRules.push_back(r);
        }
    };

    if (mLang == HighlightLanguage::Ruby) {
        addKeywords({
            "class", "module", "def", "end", "if", "elsif", "else", "unless",
            "while", "until", "for", "in", "do", "begin", "rescue", "ensure",
            "return", "yield", "super", "self", "nil", "true", "false",
            "and", "or", "not", "then", "when", "case", "break", "next",
            "redo", "retry", "alias", "undef", "defined?", "attr_accessor",
            "attr_reader", "attr_writer", "include", "extend", "require",
            "require_relative", "private", "public", "protected"
        }, mFmtKeyword);

        addKeywords({
            "String", "Integer", "Float", "Array", "Hash", "Symbol", "Object",
            "Class", "Module", "Proc", "Lambda", "Range", "Regexp",
            "Scene_Base", "Scene_Title", "Scene_Map", "Scene_Battle",
            "SceneManager", "Game_Temp", "Game_System", "Engine", "Audio",
            "Input", "UI", "Actor", "Map"
        }, mFmtType);

        // Symbols :foo
        Rule sym;
        sym.pattern = QRegularExpression(QStringLiteral(":[A-Za-z_][A-Za-z0-9_]*"));
        sym.format = mFmtNumber;
        mRules.push_back(sym);

        // Instance vars @foo, globals $foo
        Rule ivar;
        ivar.pattern = QRegularExpression(QStringLiteral("[@$][A-Za-z_][A-Za-z0-9_]*"));
        ivar.format = mFmtType;
        mRules.push_back(ivar);

        // Strings
        Rule s1; s1.pattern = QRegularExpression(QStringLiteral("\"(?:\\\\.|[^\"\\\\])*\""));
        s1.format = mFmtString; mRules.push_back(s1);
        Rule s2; s2.pattern = QRegularExpression(QStringLiteral("'(?:\\\\.|[^'\\\\])*'"));
        s2.format = mFmtString; mRules.push_back(s2);

        // Comments
        Rule c; c.pattern = QRegularExpression(QStringLiteral("#[^\n]*"));
        c.format = mFmtComment; mRules.push_back(c);

        // Numbers
        Rule n; n.pattern = QRegularExpression(QStringLiteral("\\b[0-9]+(?:\\.[0-9]+)?\\b"));
        n.format = mFmtNumber; mRules.push_back(n);

        // Method calls foo(
        Rule fn; fn.pattern = QRegularExpression(QStringLiteral("\\b[A-Za-z_][A-Za-z0-9_]*(?=\\s*\\()"));
        fn.format = mFmtFunction; mRules.push_back(fn);
    } else if (mLang == HighlightLanguage::Cpp) {
        addKeywords({
            "alignas", "alignof", "and", "and_eq", "asm", "auto", "bitand", "bitor",
            "bool", "break", "case", "catch", "char", "char8_t", "char16_t", "char32_t",
            "class", "compl", "concept", "const", "consteval", "constexpr", "constinit",
            "const_cast", "continue", "co_await", "co_return", "co_yield", "decltype",
            "default", "delete", "do", "double", "dynamic_cast", "else", "enum",
            "explicit", "export", "extern", "false", "float", "for", "friend", "goto",
            "if", "inline", "int", "long", "mutable", "namespace", "new", "noexcept",
            "not", "not_eq", "nullptr", "operator", "or", "or_eq", "private",
            "protected", "public", "register", "reinterpret_cast", "requires", "return",
            "short", "signed", "sizeof", "static", "static_assert", "static_cast",
            "struct", "switch", "template", "this", "thread_local", "throw", "true",
            "try", "typedef", "typeid", "typename", "union", "unsigned", "using",
            "virtual", "void", "volatile", "wchar_t", "while", "xor", "xor_eq",
            "override", "final", "nullptr"
        }, mFmtKeyword);

        addKeywords({
            "std", "string", "vector", "unique_ptr", "shared_ptr", "optional",
            "Engine", "Scene", "EntityID", "Transform", "Vec2", "Vec3", "Vec4",
            "Color", "Mat4", "Map", "Project", "RubyVM", "ScriptManager",
            "Renderer", "Camera", "Input", "Texture", "Model", "Database"
        }, mFmtType);

        Rule prep; prep.pattern = QRegularExpression(QStringLiteral("^\\s*#\\s*\\w+.*"));
        prep.format = mFmtPreprocessor; mRules.push_back(prep);

        Rule s1; s1.pattern = QRegularExpression(QStringLiteral("\"(?:\\\\.|[^\"\\\\])*\""));
        s1.format = mFmtString; mRules.push_back(s1);
        Rule s2; s2.pattern = QRegularExpression(QStringLiteral("'(?:\\\\.|[^'\\\\])'"));
        s2.format = mFmtString; mRules.push_back(s2);
        Rule raw; raw.pattern = QRegularExpression(QStringLiteral("R\"[^(]*\\([\\s\\S]*?\\)[^\"]*\""));
        raw.format = mFmtString; mRules.push_back(raw);

        Rule c; c.pattern = QRegularExpression(QStringLiteral("//[^\n]*"));
        c.format = mFmtComment; mRules.push_back(c);

        Rule n; n.pattern = QRegularExpression(QStringLiteral("\\b[0-9]+(?:\\.[0-9]+)?(?:[fFuUlL]*)?\\b"));
        n.format = mFmtNumber; mRules.push_back(n);

        Rule fn; fn.pattern = QRegularExpression(QStringLiteral("\\b[A-Za-z_][A-Za-z0-9_]*(?=\\s*\\()"));
        fn.format = mFmtFunction; mRules.push_back(fn);
    }
}

void QtSyntaxHighlighter::highlightBlock(const QString& text) {
    if (mLang == HighlightLanguage::None) return;

    for (const Rule& rule : mRules) {
        auto it = rule.pattern.globalMatch(text);
        while (it.hasNext()) {
            auto m = it.next();
            setFormat(m.capturedStart(), m.capturedLength(), rule.format);
        }
    }

    // Multi-line /* */ only for C++
    if (mLang != HighlightLanguage::Cpp) {
        setCurrentBlockState(0);
        return;
    }

    setCurrentBlockState(0);
    int startIndex = 0;
    if (previousBlockState() != 1)
        startIndex = text.indexOf(mCommentStart);

    while (startIndex >= 0) {
        auto match = mCommentEnd.match(text, startIndex);
        int endIndex = match.hasMatch() ? match.capturedStart() : -1;
        int commentLength;
        if (endIndex == -1) {
            setCurrentBlockState(1);
            commentLength = text.length() - startIndex;
        } else {
            commentLength = endIndex - startIndex + match.capturedLength();
        }
        setFormat(startIndex, commentLength, mMultiComment);
        startIndex = text.indexOf(mCommentStart, startIndex + commentLength);
    }
}

} // namespace qt_editor
