#include "TerminalView.h"

#include <QEvent>
#include <QVBoxLayout>

#include <qtermwidget6/qtermwidget.h>

TerminalView::TerminalView(QWidget* parent)
    : QWidget(parent)
    , m_terminal(new QTermWidget(0, this)) {
    auto* rootLayout = new QVBoxLayout(this);
    rootLayout->setContentsMargins(0, 0, 0, 0);
    rootLayout->setSpacing(0);
    rootLayout->addWidget(m_terminal, 1);

    m_terminal->setTerminalSizeHint(false);
    m_terminal->installEventFilter(this);
}

void TerminalView::setShell(const QString& shellPath) {
    m_shellPath = shellPath;
}

QString TerminalView::shell() const {
    return m_shellPath;
}

void TerminalView::startShell() {
#ifdef Q_OS_WIN
    QString program = m_shellPath.isEmpty() ? QStringLiteral("powershell.exe") : m_shellPath;
    m_terminal->setShellProgram(program);
#else
    QString program = m_shellPath.isEmpty() ? QStringLiteral("/bin/bash") : m_shellPath;
    m_terminal->setShellProgram(program);
#endif
    m_terminal->startShellProgram();
}

void TerminalView::sendCommand(const QString& command) {
    m_terminal->sendText(command + QLatin1Char('\n'));
}

void TerminalView::setTerminalColors(const QColor& foreground, const QColor& background) {
    m_terminal->setStyleSheet(
        QStringLiteral("QWidget { background-color: %1; color: %2; }")
            .arg(background.name(), foreground.name()));
}

void TerminalView::copy() {
    m_terminal->copyClipboard();
}

void TerminalView::paste() {
    m_terminal->pasteClipboard();
}

void TerminalView::selectAll() {
    m_terminal->selectAll();
}

bool TerminalView::eventFilter(QObject* watched, QEvent* event) {
    if (watched == m_terminal && event->type() == QEvent::FocusIn) {
        emit becameActive();
    }
    return QWidget::eventFilter(watched, event);
}
