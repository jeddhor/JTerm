#include "StartupScriptDialog.h"

#include <QDialogButtonBox>
#include <QFontDatabase>
#include <QLabel>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QRegularExpression>
#include <QShortcut>
#include <QSyntaxHighlighter>
#include <QTextCharFormat>
#include <QVBoxLayout>

namespace {
class ShellSyntaxHighlighter : public QSyntaxHighlighter {
public:
    explicit ShellSyntaxHighlighter(QTextDocument* parent)
        : QSyntaxHighlighter(parent) {
        QTextCharFormat commentFormat;
        commentFormat.setForeground(QColor(106, 153, 85));
        m_rules.append({QRegularExpression(QStringLiteral("#.*$")), commentFormat});

        QTextCharFormat stringFormat;
        stringFormat.setForeground(QColor(206, 145, 120));
        m_rules.append({QRegularExpression(QStringLiteral("\"([^\"\\\\]|\\\\.)*\"|'([^'\\\\]|\\\\.)*'")), stringFormat});

        QTextCharFormat keywordFormat;
        keywordFormat.setForeground(QColor(86, 156, 214));
        keywordFormat.setFontWeight(QFont::Bold);
        m_rules.append({QRegularExpression(QStringLiteral("\\b(if|then|else|elif|fi|for|while|do|done|case|esac|function|in)\\b")), keywordFormat});

        QTextCharFormat commandFormat;
        commandFormat.setForeground(QColor(220, 220, 170));
        m_rules.append({QRegularExpression(QStringLiteral("^\\s*([a-zA-Z_][a-zA-Z0-9_\\-]*)")), commandFormat});

        QTextCharFormat varFormat;
        varFormat.setForeground(QColor(156, 220, 254));
        m_rules.append({QRegularExpression(QStringLiteral("(\\$[a-zA-Z_][a-zA-Z0-9_]*|\\$\\{[^}]+\\})")), varFormat});
    }

protected:
    void highlightBlock(const QString& text) override {
        for (const Rule& rule : m_rules) {
            QRegularExpressionMatchIterator it = rule.pattern.globalMatch(text);
            while (it.hasNext()) {
                const QRegularExpressionMatch match = it.next();
                const int start = match.capturedStart();
                const int length = match.capturedLength();
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

StartupScriptDialog::StartupScriptDialog(const QString& paneTitle, const QString& initialScript, QWidget* parent)
    : QDialog(parent)
    , m_editor(new QPlainTextEdit(this))
    , m_infoLabel(new QLabel(this))
    , m_applyButton(new QPushButton(QStringLiteral("Apply"), this))
    , m_script(initialScript) {
    setWindowTitle(QStringLiteral("Pane Startup Script"));
    resize(760, 520);

    auto* rootLayout = new QVBoxLayout(this);
    rootLayout->setContentsMargins(10, 10, 10, 10);
    rootLayout->setSpacing(8);

    auto* helperLabel = new QLabel(
        QStringLiteral("Commands in this script are executed when this pane is created from a loaded layout JSON."),
        this);
    helperLabel->setWordWrap(true);
    rootLayout->addWidget(helperLabel);

    m_infoLabel->setText(QStringLiteral("Pane: ") + paneTitle);
    rootLayout->addWidget(m_infoLabel);

    QFont mono = QFontDatabase::systemFont(QFontDatabase::FixedFont);
    mono.setPointSize(10);

    m_editor->setFont(mono);
    m_editor->setLineWrapMode(QPlainTextEdit::NoWrap);
    m_editor->setTabStopDistance(24.0);
    m_editor->setPlainText(initialScript);
    m_editor->setPlaceholderText(QStringLiteral("#!/usr/bin/env bash\n# startup commands\necho 'hello'"));
    m_editor->setStyleSheet(QStringLiteral(
        "QPlainTextEdit {"
        "  background: #0b1220;"
        "  color: #d7e1ef;"
        "  border: 1px solid #2f3a4e;"
        "  border-radius: 6px;"
        "  selection-background-color: #1f6feb;"
        "}"
    ));
    new ShellSyntaxHighlighter(m_editor->document());
    rootLayout->addWidget(m_editor, 1);

    auto* buttonBox = new QDialogButtonBox(Qt::Horizontal, this);
    auto* cancelButton = buttonBox->addButton(QStringLiteral("Cancel"), QDialogButtonBox::RejectRole);
    buttonBox->addButton(m_applyButton, QDialogButtonBox::AcceptRole);
    rootLayout->addWidget(buttonBox);

    connect(m_applyButton, &QPushButton::clicked, this, &StartupScriptDialog::applyAndClose);
    connect(cancelButton, &QPushButton::clicked, this, &QDialog::reject);

    auto* applyShortcut = new QShortcut(QKeySequence(QStringLiteral("Ctrl+Return")), this);
    connect(applyShortcut, &QShortcut::activated, this, &StartupScriptDialog::applyAndClose);
}

QString StartupScriptDialog::script() const {
    return m_script;
}

void StartupScriptDialog::applyAndClose() {
    m_script = m_editor->toPlainText();
    accept();
}
