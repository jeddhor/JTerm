#include "TerminalPane.h"

#include "TerminalView.h"

#include <QAction>
#include <QHBoxLayout>
#include <QLabel>
#include <QMenu>
#include <QVBoxLayout>

TerminalPane::TerminalPane(const QString& paneId, const QString& shellPath, QWidget* parent)
    : QWidget(parent)
    , m_paneId(paneId)
    , m_title(QStringLiteral("Pane ") + paneId)
    , m_idLabel(new QLabel(this))
    , m_titleLabel(new QLabel(this))
    , m_terminalView(new TerminalView(this)) {
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    auto* rootLayout = new QVBoxLayout(this);
    rootLayout->setContentsMargins(6, 6, 6, 6);
    rootLayout->setSpacing(4);

    auto* titleBar = new QWidget(this);
    auto* titleLayout = new QHBoxLayout(titleBar);
    titleLayout->setContentsMargins(8, 6, 8, 6);
    titleLayout->setSpacing(8);

    m_titleLabel->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    QFont titleFont = m_titleLabel->font();
    titleFont.setBold(true);
    m_titleLabel->setFont(titleFont);

    m_idLabel->setMinimumWidth(60);
    m_idLabel->setAlignment(Qt::AlignCenter);

    titleLayout->addWidget(m_titleLabel, 1);
    titleLayout->addWidget(m_idLabel, 0);

    m_terminalView->setShell(shellPath);
    m_terminalView->startShell();

    rootLayout->addWidget(titleBar, 0);
    rootLayout->addWidget(m_terminalView, 1);

    setContextMenuPolicy(Qt::CustomContextMenu);
    m_terminalView->setContextMenuPolicy(Qt::CustomContextMenu);

    auto openContextMenu = [this](const QPoint& pos) {
        emit activated(this);

        QMenu menu(this);
        QAction* copyAction = menu.addAction(QStringLiteral("Copy"));
        QAction* pasteAction = menu.addAction(QStringLiteral("Paste"));
        QAction* selectAllAction = menu.addAction(QStringLiteral("Select All"));
        menu.addSeparator();
        QAction* splitHorizontalAction = menu.addAction(QStringLiteral("Split Horizontally"));
        QAction* splitVerticalAction = menu.addAction(QStringLiteral("Split Vertically"));
        QAction* renameAction = menu.addAction(QStringLiteral("Rename Pane..."));
        QAction* closeAction = menu.addAction(QStringLiteral("Close Terminal"));
        menu.addSeparator();
        QAction* preferencesAction = menu.addAction(QStringLiteral("Preferences..."));

        QAction* chosen = menu.exec(mapToGlobal(pos));
        if (!chosen) {
            return;
        }

        if (chosen == copyAction) {
            emit copyRequested(this);
        } else if (chosen == pasteAction) {
            emit pasteRequested(this);
        } else if (chosen == selectAllAction) {
            emit selectAllRequested(this);
        } else if (chosen == splitHorizontalAction) {
            emit splitRequested(this, Qt::Vertical);
        } else if (chosen == splitVerticalAction) {
            emit splitRequested(this, Qt::Horizontal);
        } else if (chosen == renameAction) {
            emit renameRequested(this);
        } else if (chosen == closeAction) {
            emit closeRequested(this);
        } else if (chosen == preferencesAction) {
            emit preferencesRequested(this);
        }
    };

    connect(this, &QWidget::customContextMenuRequested, this, openContextMenu);
    connect(m_terminalView, &QWidget::customContextMenuRequested, this, [this, openContextMenu](const QPoint& localPos) {
        const QPoint mapped = m_terminalView->mapTo(this, localPos);
        openContextMenu(mapped);
    });

    connect(m_terminalView, &TerminalView::becameActive, this, &TerminalPane::onInnerActivated);

    updateHeader();
}

QString TerminalPane::paneId() const {
    return m_paneId;
}

QString TerminalPane::title() const {
    return m_title;
}

void TerminalPane::setTitle(const QString& title) {
    m_title = title.trimmed().isEmpty() ? (QStringLiteral("Pane ") + m_paneId) : title.trimmed();
    updateHeader();
}

TerminalView* TerminalPane::terminalView() const {
    return m_terminalView;
}

void TerminalPane::onInnerActivated() {
    emit activated(this);
}

void TerminalPane::updateHeader() {
    m_titleLabel->setText(m_title);
    m_idLabel->setText(QStringLiteral("#") + m_paneId);
}
