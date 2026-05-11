#include "TerminalPane.h"

#include "TerminalView.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QVBoxLayout>

TerminalPane::TerminalPane(const QString& paneId, const QString& shellPath, QWidget* parent)
    : QWidget(parent)
    , m_paneId(paneId)
    , m_idLabel(new QLabel(this))
    , m_titleEdit(new QLineEdit(this))
    , m_splitHorizontalButton(new QPushButton(QStringLiteral("Split H"), this))
    , m_splitVerticalButton(new QPushButton(QStringLiteral("Split V"), this))
    , m_terminalView(new TerminalView(this)) {
    auto* rootLayout = new QVBoxLayout(this);
    rootLayout->setContentsMargins(4, 4, 4, 4);
    rootLayout->setSpacing(6);

    auto* titleBar = new QWidget(this);
    auto* titleLayout = new QHBoxLayout(titleBar);
    titleLayout->setContentsMargins(0, 0, 0, 0);
    titleLayout->setSpacing(6);

    m_idLabel->setText(QStringLiteral("id: ") + paneId);
    m_idLabel->setMinimumWidth(90);

    m_titleEdit->setPlaceholderText(QStringLiteral("Pane title"));
    m_titleEdit->setText(QStringLiteral("Pane ") + paneId);

    titleLayout->addWidget(m_idLabel, 0);
    titleLayout->addWidget(m_titleEdit, 1);
    titleLayout->addWidget(m_splitHorizontalButton, 0);
    titleLayout->addWidget(m_splitVerticalButton, 0);

    m_terminalView->setShell(shellPath);
    m_terminalView->startShell();

    rootLayout->addWidget(titleBar, 0);
    rootLayout->addWidget(m_terminalView, 1);

    setStyleSheet(QStringLiteral(
        "TerminalPane {"
        "  border: 1px solid rgba(92, 107, 129, 0.4);"
        "  border-radius: 8px;"
        "  background: rgba(250, 251, 253, 0.8);"
        "}"
    ));

    connect(m_titleEdit, &QLineEdit::textEdited, this, &TerminalPane::onTitleEdited);
    connect(m_splitHorizontalButton, &QPushButton::clicked, this, [this]() {
        emit splitRequested(this, Qt::Horizontal);
    });
    connect(m_splitVerticalButton, &QPushButton::clicked, this, [this]() {
        emit splitRequested(this, Qt::Vertical);
    });
    connect(m_terminalView, &TerminalView::becameActive, this, &TerminalPane::onInnerActivated);
}

QString TerminalPane::paneId() const {
    return m_paneId;
}

QString TerminalPane::title() const {
    return m_titleEdit->text();
}

void TerminalPane::setTitle(const QString& title) {
    m_titleEdit->setText(title);
}

TerminalView* TerminalPane::terminalView() const {
    return m_terminalView;
}

void TerminalPane::onTitleEdited(const QString&) {
    emit activated(this);
}

void TerminalPane::onInnerActivated() {
    emit activated(this);
}
