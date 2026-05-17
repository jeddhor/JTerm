#include "TerminalPane.h"

#include "TerminalView.h"

#include <QAction>
#include <QCheckBox>
#include <QEvent>
#include <QHBoxLayout>
#include <QLabel>
#include <QMenu>
#include <QMouseEvent>
#include <QTimer>
#include <QToolButton>
#include <QVBoxLayout>
#include <QtGlobal>

TerminalPane::TerminalPane(const QString& paneId, const QString& shellPath, QWidget* parent)
    : QWidget(parent)
    , m_paneId(paneId)
    , m_title(QStringLiteral("Pane ") + paneId)
    , m_titleLabel(new QLabel(this))
    , m_startupIndicatorLabel(new QLabel(this))
    , m_startupThrottleIndicatorLabel(new QLabel(this))
    , m_titleBar(new QWidget(this))
    , m_broadcastTargetCheck(new QCheckBox(this))
    , m_moveToTabButton(new QToolButton(this))
    , m_terminalView(new TerminalView(this))
    , m_startupThrottleTimer(new QTimer(this))
    , m_isBroadcastSource(false) {
    m_startupScriptThrottled = false;
    m_startupThrottleIntervalSeconds = 1;

    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    auto* rootLayout = new QVBoxLayout(this);
    rootLayout->setContentsMargins(4, 4, 4, 4);
    rootLayout->setSpacing(2);

    auto* titleLayout = new QHBoxLayout(m_titleBar);
    titleLayout->setContentsMargins(6, 2, 6, 2);
    titleLayout->setSpacing(6);

    m_titleLabel->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    QFont titleFont = m_titleLabel->font();
    titleFont.setBold(true);
    titleFont.setPointSize(qMax(7, titleFont.pointSize() - 1));
    m_titleLabel->setFont(titleFont);

    m_startupIndicatorLabel->setText(QStringLiteral("⚡"));
    m_startupIndicatorLabel->setToolTip(QStringLiteral("This pane has startup commands configured"));
    m_startupIndicatorLabel->setVisible(false);

    m_startupThrottleIndicatorLabel->setText(QStringLiteral("⏱"));
    m_startupThrottleIndicatorLabel->setToolTip(QStringLiteral("Startup script throttling is enabled for this pane"));
    m_startupThrottleIndicatorLabel->setVisible(false);

    m_broadcastTargetCheck->setText(QStringLiteral("◎"));
    m_broadcastTargetCheck->setToolTip(QStringLiteral("Broadcast target"));

    m_moveToTabButton->setText(QStringLiteral("↗"));
    m_moveToTabButton->setAutoRaise(true);
    m_moveToTabButton->setToolTip(QStringLiteral("Move this pane to a new tab"));

    titleLayout->addWidget(m_titleLabel, 1);
    titleLayout->addWidget(m_startupIndicatorLabel, 0);
    titleLayout->addWidget(m_startupThrottleIndicatorLabel, 0);
    titleLayout->addWidget(m_broadcastTargetCheck, 0);
    titleLayout->addWidget(m_moveToTabButton, 0);

    m_terminalView->setShell(shellPath);
    m_terminalView->startShell();

    rootLayout->addWidget(m_titleBar, 0);
    rootLayout->addWidget(m_terminalView, 1);

    m_titleBar->installEventFilter(this);
    m_titleLabel->installEventFilter(this);

    setContextMenuPolicy(Qt::CustomContextMenu);
    m_terminalView->setContextMenuPolicy(Qt::CustomContextMenu);

    auto openContextMenu = [this](const QPoint& pos) {
        emit activated(this);

        QMenu menu(this);
        QAction* copyAction = menu.addAction(QStringLiteral("⎘ Copy"));
        QAction* pasteAction = menu.addAction(QStringLiteral("📋 Paste"));
        QAction* selectAllAction = menu.addAction(QStringLiteral("Select All"));
        menu.addSeparator();
        QAction* renameTabAction = menu.addAction(QStringLiteral("Rename Tab..."));
        menu.addSeparator();
        QAction* splitHorizontalAction = menu.addAction(QStringLiteral("Split Horizontally"));
        QAction* splitVerticalAction = menu.addAction(QStringLiteral("Split Vertically"));
        QAction* moveToTabAction = menu.addAction(QStringLiteral("Move To New Tab"));
        QAction* closeAction = menu.addAction(QStringLiteral("Close Terminal"));
        menu.addSeparator();
        QAction* broadcastSourceAction = menu.addAction(QStringLiteral("Broadcast Source"));
        broadcastSourceAction->setCheckable(true);
        broadcastSourceAction->setChecked(m_isBroadcastSource);
        QAction* setBroadcastGroupAction = menu.addAction(QStringLiteral("Set Broadcast Group..."));
        QAction* clearBroadcastGroupAction = menu.addAction(QStringLiteral("Clear Broadcast Group"));
        clearBroadcastGroupAction->setEnabled(!m_broadcastGroup.trimmed().isEmpty());
        menu.addSeparator();
        QAction* openSelectionAction = menu.addAction(QStringLiteral("Open Selected URL/File"));
        menu.addSeparator();
        QAction* startupScriptAction = menu.addAction(QStringLiteral("Edit Startup Script..."));
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
        } else if (chosen == renameTabAction) {
            emit renameTabRequested(this);
        } else if (chosen == splitHorizontalAction) {
            emit splitRequested(this, Qt::Vertical);
        } else if (chosen == splitVerticalAction) {
            emit splitRequested(this, Qt::Horizontal);
        } else if (chosen == broadcastSourceAction) {
            emit broadcastSourceToggleRequested(this);
        } else if (chosen == setBroadcastGroupAction) {
            emit broadcastGroupEditRequested(this);
        } else if (chosen == clearBroadcastGroupAction) {
            emit broadcastGroupClearRequested(this);
        } else if (chosen == openSelectionAction) {
            emit openSelectionRequested(this);
        } else if (chosen == moveToTabAction) {
            emit moveToNewTabRequested(this);
        } else if (chosen == startupScriptAction) {
            emit startupScriptRequested(this);
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
    connect(m_moveToTabButton, &QToolButton::clicked, this, [this]() {
        emit activated(this);
        emit moveToNewTabRequested(this);
    });
    connect(m_broadcastTargetCheck, &QCheckBox::toggled, this, [this](bool checked) {
        emit broadcastTargetToggled(this, checked);
    });
    connect(m_startupThrottleTimer, &QTimer::timeout, this, &TerminalPane::runNextStartupStep);
    m_startupThrottleTimer->setSingleShot(false);
    connect(m_terminalView, &TerminalView::openSelectionRequested, this, [this]() {
        emit openSelectionRequested(this);
    });

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

QString TerminalPane::startupScript() const {
    return m_startupScript;
}

void TerminalPane::setStartupScript(const QString& script) {
    m_startupScript = script;
    updateHeader();
}

bool TerminalPane::startupScriptThrottled() const {
    return m_startupScriptThrottled;
}

void TerminalPane::setStartupScriptThrottled(bool enabled) {
    m_startupScriptThrottled = enabled;
    updateHeader();
}

void TerminalPane::setStartupThrottleIntervalSeconds(int seconds) {
    m_startupThrottleIntervalSeconds = qMax(1, seconds);
}

void TerminalPane::runStartupScript() {
    m_startupThrottleTimer->stop();
    m_pendingStartupSteps.clear();

    if (m_startupScript.trimmed().isEmpty()) {
        return;
    }

    if (!m_startupScriptThrottled) {
        m_terminalView->sendCommand(m_startupScript);
        return;
    }

    const QStringList lines = m_startupScript.split(QLatin1Char('\n'));
    for (const QString& line : lines) {
        const QString step = line.trimmed();
        if (!step.isEmpty()) {
            m_pendingStartupSteps.append(step);
        }
    }
    if (m_pendingStartupSteps.isEmpty()) {
        return;
    }

    m_startupThrottleTimer->start(m_startupThrottleIntervalSeconds * 1000);
}

void TerminalPane::runNextStartupStep() {
    if (m_pendingStartupSteps.isEmpty()) {
        m_startupThrottleTimer->stop();
        return;
    }

    const QString step = m_pendingStartupSteps.takeFirst();
    m_terminalView->sendCommand(step);
}

bool TerminalPane::hasRunningProcess() const {
    return m_terminalView && m_terminalView->hasRunningForegroundProcess();
}

void TerminalPane::setMoveToTabVisible(bool visible) {
    m_moveToTabButton->setVisible(visible);
}

bool TerminalPane::isBroadcastTargetChecked() const {
    return m_broadcastTargetCheck->isChecked();
}

void TerminalPane::setBroadcastTargetChecked(bool checked) {
    m_broadcastTargetCheck->setChecked(checked);
}

void TerminalPane::setBroadcastTargetEnabled(bool enabled) {
    m_broadcastTargetCheck->setEnabled(enabled);
}

void TerminalPane::setBroadcastSourceSelected(bool selected) {
    m_isBroadcastSource = selected;
}

QString TerminalPane::broadcastGroup() const {
    return m_broadcastGroup;
}

void TerminalPane::setBroadcastGroup(const QString& groupName) {
    m_broadcastGroup = groupName.trimmed();
    updateHeader();
}

TerminalView* TerminalPane::terminalView() const {
    return m_terminalView;
}

void TerminalPane::onInnerActivated() {
    emit activated(this);
}

bool TerminalPane::eventFilter(QObject* watched, QEvent* event) {
    if ((watched == m_titleBar || watched == m_titleLabel) && event->type() == QEvent::MouseButtonDblClick) {
        auto* mouseEvent = static_cast<QMouseEvent*>(event);
        if (mouseEvent->button() == Qt::LeftButton) {
            emit activated(this);
            emit renameRequested(this);
            return true;
        }
    }

    return QWidget::eventFilter(watched, event);
}

void TerminalPane::updateHeader() {
    m_titleLabel->setText(m_title);
    const QString tooltip = m_title + QStringLiteral(" (id: ") + m_paneId + QStringLiteral(")");
    QString detailedTooltip = tooltip;
    if (!m_broadcastGroup.trimmed().isEmpty()) {
        detailedTooltip += QStringLiteral("\nBroadcast group: ") + m_broadcastGroup.trimmed();
    }
    m_titleLabel->setToolTip(detailedTooltip);
    m_titleBar->setToolTip(detailedTooltip);
    m_startupIndicatorLabel->setVisible(!m_startupScript.trimmed().isEmpty());
    m_startupThrottleIndicatorLabel->setVisible(m_startupScriptThrottled && !m_startupScript.trimmed().isEmpty());
    m_moveToTabButton->setToolTip(QStringLiteral("Move ") + tooltip + QStringLiteral(" to a new tab"));
}
