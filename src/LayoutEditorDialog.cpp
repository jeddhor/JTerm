#include "LayoutEditorDialog.h"

#include <QFile>
#include <QFileDialog>
#include <QFontDatabase>
#include <QHBoxLayout>
#include <QJsonDocument>
#include <QJsonParseError>
#include <QLabel>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QRegularExpression>
#include <QShortcut>
#include <QSyntaxHighlighter>
#include <QTextBlock>
#include <QTextCharFormat>
#include <QTextCursor>
#include <QTextEdit>
#include <QTimer>
#include <QVBoxLayout>

namespace {
class JsonSyntaxHighlighter : public QSyntaxHighlighter {
public:
    explicit JsonSyntaxHighlighter(QTextDocument* parent)
        : QSyntaxHighlighter(parent) {
        QTextCharFormat keyFormat;
        keyFormat.setForeground(QColor(86, 156, 214));
        keyFormat.setFontWeight(QFont::Bold);
        m_rules.append({QRegularExpression(QStringLiteral("\\\"[^\\\"]*\\\"(?=\\s*:)")), keyFormat});

        QTextCharFormat stringFormat;
        stringFormat.setForeground(QColor(206, 145, 120));
        m_rules.append({QRegularExpression(QStringLiteral(":\\s*\\\"([^\\\"\\\\]|\\\\.)*\\\"")), stringFormat});

        QTextCharFormat numberFormat;
        numberFormat.setForeground(QColor(181, 206, 168));
        m_rules.append({QRegularExpression(QStringLiteral("\\b-?(0|[1-9]\\d*)(\\.\\d+)?([eE][+-]?\\d+)?\\b")), numberFormat});

        QTextCharFormat boolNullFormat;
        boolNullFormat.setForeground(QColor(197, 134, 192));
        boolNullFormat.setFontWeight(QFont::Bold);
        m_rules.append({QRegularExpression(QStringLiteral("\\b(true|false|null)\\b")), boolNullFormat});

        QTextCharFormat braceFormat;
        braceFormat.setForeground(QColor(220, 220, 220));
        m_rules.append({QRegularExpression(QStringLiteral("[\\{\\}\\[\\],:]")), braceFormat});
    }

protected:
    void highlightBlock(const QString& text) override {
        for (const Rule& rule : m_rules) {
            QRegularExpressionMatchIterator it = rule.pattern.globalMatch(text);
            while (it.hasNext()) {
                const QRegularExpressionMatch match = it.next();
                int start = match.capturedStart();
                int length = match.capturedLength();
                if (start >= 0 && length > 0) {
                    setFormat(start, length, rule.format);
                }
            }
        }
    }

private:
    struct Rule {
        QRegularExpression pattern;
        QTextCharFormat format;
    };

    QList<Rule> m_rules;
};
}

LayoutEditorDialog::LayoutEditorDialog(const QJsonObject& initialLayout, QWidget* parent)
    : QDialog(parent)
    , m_editor(new QPlainTextEdit(this))
    , m_statusLabel(new QLabel(this))
    , m_validateButton(new QPushButton(QStringLiteral("Validate"), this))
    , m_formatButton(new QPushButton(QStringLiteral("Format JSON"), this))
    , m_applyButton(new QPushButton(QStringLiteral("Apply"), this))
    , m_saveButton(new QPushButton(QStringLiteral("Save As..."), this))
    , m_liveValidationTimer(new QTimer(this))
    , m_layoutObject(initialLayout) {
    setWindowTitle(QStringLiteral("Edit Layout JSON"));
    resize(880, 680);

    auto* rootLayout = new QVBoxLayout(this);
    rootLayout->setContentsMargins(10, 10, 10, 10);
    rootLayout->setSpacing(8);

    auto* helperLabel = new QLabel(
        QStringLiteral("Edit the current layout JSON. Shortcuts: Ctrl+Shift+F format, Ctrl+Enter apply, Ctrl+Shift+S save."),
        this);
    rootLayout->addWidget(helperLabel);

    m_editor->setLineWrapMode(QPlainTextEdit::NoWrap);
    m_editor->setTabStopDistance(24.0);
    QFont codeFont = QFontDatabase::systemFont(QFontDatabase::FixedFont);
    codeFont.setPointSize(10);
    m_editor->setFont(codeFont);
    m_editor->setPlainText(QString::fromUtf8(QJsonDocument(initialLayout).toJson(QJsonDocument::Indented)));
    m_editor->setPlaceholderText(QStringLiteral("Layout JSON"));
    m_editor->setStyleSheet(QStringLiteral(
        "QPlainTextEdit {"
        "  background: #0b1220;"
        "  color: #d7e1ef;"
        "  border: 1px solid #2f3a4e;"
        "  border-radius: 6px;"
        "  selection-background-color: #1f6feb;"
        "}"
    ));
    new JsonSyntaxHighlighter(m_editor->document());
    rootLayout->addWidget(m_editor, 1);

    m_statusLabel->setWordWrap(true);
    rootLayout->addWidget(m_statusLabel);

    auto* buttonRow = new QHBoxLayout();
    buttonRow->setSpacing(8);
    buttonRow->addWidget(m_validateButton);
    buttonRow->addWidget(m_formatButton);
    buttonRow->addWidget(m_saveButton);
    buttonRow->addStretch(1);

    auto* cancelButton = new QPushButton(QStringLiteral("Cancel"), this);
    buttonRow->addWidget(cancelButton);
    buttonRow->addWidget(m_applyButton);
    rootLayout->addLayout(buttonRow);

    connect(m_validateButton, &QPushButton::clicked, this, &LayoutEditorDialog::validateJson);
    connect(m_formatButton, &QPushButton::clicked, this, &LayoutEditorDialog::formatJson);
    connect(m_applyButton, &QPushButton::clicked, this, &LayoutEditorDialog::applyAndClose);
    connect(m_saveButton, &QPushButton::clicked, this, &LayoutEditorDialog::saveToFile);
    connect(cancelButton, &QPushButton::clicked, this, &QDialog::reject);

    m_liveValidationTimer->setInterval(220);
    m_liveValidationTimer->setSingleShot(true);
    connect(m_liveValidationTimer, &QTimer::timeout, this, &LayoutEditorDialog::validateJsonLive);
    connect(m_editor, &QPlainTextEdit::textChanged, this, &LayoutEditorDialog::scheduleValidation);

    auto* applyShortcut = new QShortcut(QKeySequence(QStringLiteral("Ctrl+Return")), this);
    connect(applyShortcut, &QShortcut::activated, this, &LayoutEditorDialog::applyAndClose);

    auto* formatShortcut = new QShortcut(QKeySequence(QStringLiteral("Ctrl+Shift+F")), this);
    connect(formatShortcut, &QShortcut::activated, this, &LayoutEditorDialog::formatJson);

    auto* saveShortcut = new QShortcut(QKeySequence(QStringLiteral("Ctrl+Shift+S")), this);
    connect(saveShortcut, &QShortcut::activated, this, &LayoutEditorDialog::saveToFile);

    m_applyButton->setDefault(true);

    setStatusMessage(QStringLiteral("Ready."), false);
    validateJsonLive();
}

QJsonObject LayoutEditorDialog::layoutObject() const {
    return m_layoutObject;
}

void LayoutEditorDialog::validateJson() {
    QJsonObject parsed;
    QString error;
    qsizetype errorOffset = -1;
    if (!parseEditorJson(&parsed, &error, &errorOffset)) {
        if (errorOffset >= 0) {
            setErrorHighlightAtOffset(errorOffset);
            moveCursorToOffset(errorOffset);
        } else {
            clearErrorHighlights();
        }
        setStatusMessage(error, true);
        return;
    }

    clearErrorHighlights();
    const QString hint = validationHintForLayout(parsed);
    if (!hint.isEmpty()) {
        setStatusMessage(QStringLiteral("Valid JSON. ") + hint, false, true);
        return;
    }
    setStatusMessage(QStringLiteral("Valid JSON."), false);
}

void LayoutEditorDialog::applyAndClose() {
    QJsonObject parsed;
    QString error;
    qsizetype errorOffset = -1;
    if (!parseEditorJson(&parsed, &error, &errorOffset)) {
        if (errorOffset >= 0) {
            setErrorHighlightAtOffset(errorOffset);
            moveCursorToOffset(errorOffset);
        } else {
            clearErrorHighlights();
        }
        setStatusMessage(error, true);
        return;
    }

    clearErrorHighlights();
    m_layoutObject = parsed;
    accept();
}

void LayoutEditorDialog::saveToFile() {
    QJsonObject parsed;
    QString error;
    qsizetype errorOffset = -1;
    if (!parseEditorJson(&parsed, &error, &errorOffset)) {
        if (errorOffset >= 0) {
            setErrorHighlightAtOffset(errorOffset);
            moveCursorToOffset(errorOffset);
        } else {
            clearErrorHighlights();
        }
        setStatusMessage(error, true);
        return;
    }

    clearErrorHighlights();

    const QString path = QFileDialog::getSaveFileName(this, QStringLiteral("Save Layout JSON"), QStringLiteral("layout.json"), QStringLiteral("JSON (*.json)"));
    if (path.isEmpty()) {
        return;
    }

    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        setStatusMessage(QStringLiteral("Failed to write file."), true);
        return;
    }

    file.write(QJsonDocument(parsed).toJson(QJsonDocument::Indented));
    setStatusMessage(QStringLiteral("Saved layout JSON to file."), false);
}

void LayoutEditorDialog::formatJson() {
    QJsonObject parsed;
    QString error;
    qsizetype errorOffset = -1;
    if (!parseEditorJson(&parsed, &error, &errorOffset)) {
        if (errorOffset >= 0) {
            setErrorHighlightAtOffset(errorOffset);
            moveCursorToOffset(errorOffset);
        }
        setStatusMessage(error, true);
        return;
    }

    clearErrorHighlights();
    m_editor->setPlainText(QString::fromUtf8(QJsonDocument(parsed).toJson(QJsonDocument::Indented)));
    setStatusMessage(QStringLiteral("Formatted JSON."), false);
}

void LayoutEditorDialog::scheduleValidation() {
    m_liveValidationTimer->start();
}

void LayoutEditorDialog::validateJsonLive() {
    QJsonObject parsed;
    QString error;
    qsizetype errorOffset = -1;
    if (!parseEditorJson(&parsed, &error, &errorOffset)) {
        if (errorOffset >= 0) {
            setErrorHighlightAtOffset(errorOffset);
        } else {
            clearErrorHighlights();
        }
        setStatusMessage(error, true);
        return;
    }

    clearErrorHighlights();
    const QString hint = validationHintForLayout(parsed);
    if (!hint.isEmpty()) {
        setStatusMessage(QStringLiteral("Valid JSON. ") + hint, false, true);
        return;
    }
    setStatusMessage(QStringLiteral("Valid JSON."), false);
}

bool LayoutEditorDialog::parseEditorJson(QJsonObject* parsedObject, QString* errorMessage, qsizetype* errorOffset) const {
    const QByteArray jsonBytes = m_editor->toPlainText().toUtf8();

    QJsonParseError parseError;
    const QJsonDocument doc = QJsonDocument::fromJson(jsonBytes, &parseError);
    if (parseError.error != QJsonParseError::NoError) {
        if (errorOffset) {
            *errorOffset = parseError.offset;
        }
        if (errorMessage) {
            const auto lineCol = lineColumnFromOffset(m_editor->toPlainText(), parseError.offset);
            *errorMessage = QStringLiteral("Invalid JSON at line %1, column %2 (offset %3): %4")
                .arg(lineCol.first)
                .arg(lineCol.second)
                .arg(parseError.offset)
                .arg(parseError.errorString());
        }
        return false;
    }

    if (errorOffset) {
        *errorOffset = -1;
    }

    if (!doc.isObject()) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("Layout JSON must be an object at the top level.");
        }
        return false;
    }

    if (parsedObject) {
        *parsedObject = doc.object();
    }
    return true;
}

void LayoutEditorDialog::setStatusMessage(const QString& text, bool isError, bool isWarning) {
    m_statusLabel->setText(text);
    if (isError) {
        m_statusLabel->setStyleSheet(QStringLiteral("QLabel { color: #f85149; }") );
        return;
    }

    if (isWarning) {
        m_statusLabel->setStyleSheet(QStringLiteral("QLabel { color: #d29922; }") );
        return;
    }

    m_statusLabel->setStyleSheet(QStringLiteral("QLabel { color: #3fb950; }") );
}

QString LayoutEditorDialog::validationHintForLayout(const QJsonObject& rootObject) const {
    const bool hasTabs = rootObject.contains(QStringLiteral("tabs"));
    const bool hasRoot = rootObject.contains(QStringLiteral("root"));

    if (!hasTabs && !hasRoot) {
        return QStringLiteral("This JSON is valid, but does not look like a SplitTerm layout (missing 'tabs' or 'root').");
    }

    if (!rootObject.contains(QStringLiteral("version"))) {
        return QStringLiteral("No 'version' field found; import may be treated as legacy layout.");
    }

    return QString();
}

QPair<int, int> LayoutEditorDialog::lineColumnFromOffset(const QString& text, qsizetype offset) {
    if (offset < 0) {
        return {1, 1};
    }

    const qsizetype safeOffset = qMin<qsizetype>(offset, text.size());
    int line = 1;
    int column = 1;

    for (qsizetype i = 0; i < safeOffset; ++i) {
        if (text.at(i) == QLatin1Char('\n')) {
            ++line;
            column = 1;
        } else {
            ++column;
        }
    }

    return {line, column};
}

void LayoutEditorDialog::moveCursorToOffset(qsizetype offset) {
    QTextCursor cursor = m_editor->textCursor();
    cursor.setPosition(qMax<qsizetype>(0, qMin<qsizetype>(offset, m_editor->toPlainText().size())));
    m_editor->setTextCursor(cursor);
    m_editor->centerCursor();
}

void LayoutEditorDialog::clearErrorHighlights() {
    m_editor->setExtraSelections({});
}

void LayoutEditorDialog::setErrorHighlightAtOffset(qsizetype offset) {
    QTextCursor cursor(m_editor->document());
    cursor.setPosition(qMax<qsizetype>(0, qMin<qsizetype>(offset, m_editor->toPlainText().size())));

    QTextEdit::ExtraSelection selection;
    selection.cursor = cursor;
    selection.cursor.movePosition(QTextCursor::StartOfLine);
    selection.cursor.movePosition(QTextCursor::EndOfLine, QTextCursor::KeepAnchor);
    selection.format.setBackground(QColor(120, 20, 20, 80));

    m_editor->setExtraSelections({selection});
}
