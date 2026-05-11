#include "TerminalView.h"

#include <QEvent>
#include <QFontDatabase>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QScrollBar>
#include <QTextCursor>
#include <QVBoxLayout>

TerminalView::TerminalView(QWidget* parent)
    : QWidget(parent)
    , m_process(new QProcess(this))
    , m_output(new QPlainTextEdit(this))
    , m_input(new QLineEdit(this)) {
    auto* rootLayout = new QVBoxLayout(this);
    rootLayout->setContentsMargins(0, 0, 0, 0);
    rootLayout->setSpacing(6);

    m_output->setReadOnly(true);
    QFont mono = QFontDatabase::systemFont(QFontDatabase::FixedFont);
    mono.setPointSize(10);
    m_output->setFont(mono);

    auto* inputRow = new QWidget(this);
    auto* inputLayout = new QHBoxLayout(inputRow);
    inputLayout->setContentsMargins(0, 0, 0, 0);
    inputLayout->setSpacing(6);

    auto* promptLabel = new QLabel(QStringLiteral("$"), inputRow);
    promptLabel->setMinimumWidth(12);
    inputLayout->addWidget(promptLabel);

    m_input->setPlaceholderText(QStringLiteral("Type command and press Enter"));
    m_input->setFont(mono);
    inputLayout->addWidget(m_input, 1);

    rootLayout->addWidget(m_output, 1);
    rootLayout->addWidget(inputRow, 0);

    m_output->installEventFilter(this);
    m_input->installEventFilter(this);

    connect(m_process, &QProcess::readyReadStandardOutput, this, &TerminalView::onReadyReadStdout);
    connect(m_process, &QProcess::readyReadStandardError, this, &TerminalView::onReadyReadStderr);
    connect(m_process, &QProcess::started, this, &TerminalView::onProcessStarted);
    connect(m_process, qOverload<int, QProcess::ExitStatus>(&QProcess::finished), this, &TerminalView::onProcessFinished);
    connect(m_input, &QLineEdit::returnPressed, this, &TerminalView::onUserCommandEntered);
}

void TerminalView::setShell(const QString& shellPath) {
    m_shellPath = shellPath;
}

QString TerminalView::shell() const {
    return m_shellPath;
}

void TerminalView::startShell() {
    if (m_process->state() != QProcess::NotRunning) {
        return;
    }

#ifdef Q_OS_WIN
    QString program = m_shellPath.isEmpty() ? QStringLiteral("powershell.exe") : m_shellPath;
    m_process->setProgram(program);
    m_process->setArguments({QStringLiteral("-NoLogo")});
#else
    QString program = m_shellPath.isEmpty() ? QStringLiteral("/bin/bash") : m_shellPath;
    m_process->setProgram(program);
    m_process->setArguments({QStringLiteral("-i")});
#endif

    m_process->setProcessChannelMode(QProcess::SeparateChannels);
    m_process->start();
}

void TerminalView::sendCommand(const QString& command) {
    if (m_process->state() != QProcess::Running) {
        startShell();
    }

    if (m_process->state() != QProcess::Running) {
        appendOutput(QStringLiteral("[error] shell is not running\n"));
        return;
    }

    const QByteArray line = (command + QLatin1Char('\n')).toLocal8Bit();
    m_process->write(line);
}

void TerminalView::copy() {
    if (m_input->hasFocus()) {
        m_input->copy();
        return;
    }
    m_output->copy();
}

void TerminalView::paste() {
    m_input->setFocus();
    m_input->paste();
}

void TerminalView::selectAll() {
    if (m_input->hasFocus()) {
        m_input->selectAll();
        return;
    }
    m_output->selectAll();
}

bool TerminalView::eventFilter(QObject* watched, QEvent* event) {
    if ((watched == m_output || watched == m_input) && event->type() == QEvent::FocusIn) {
        emit becameActive();
    }
    return QWidget::eventFilter(watched, event);
}

void TerminalView::onReadyReadStdout() {
    const QByteArray out = m_process->readAllStandardOutput();
    appendOutput(QString::fromLocal8Bit(out));
}

void TerminalView::onReadyReadStderr() {
    const QByteArray out = m_process->readAllStandardError();
    appendOutput(QString::fromLocal8Bit(out));
}

void TerminalView::onProcessStarted() {
    appendOutput(QStringLiteral("[shell started] ") + m_process->program() + QLatin1Char('\n'));
}

void TerminalView::onProcessFinished(int exitCode, QProcess::ExitStatus exitStatus) {
    QString statusText = exitStatus == QProcess::NormalExit ? QStringLiteral("normal") : QStringLiteral("crashed");
    appendOutput(QStringLiteral("[shell exited] code=") + QString::number(exitCode) + QStringLiteral(" status=") + statusText + QLatin1Char('\n'));
}

void TerminalView::onUserCommandEntered() {
    const QString command = m_input->text().trimmed();
    if (command.isEmpty()) {
        return;
    }

    appendOutput(QStringLiteral("$ ") + command + QLatin1Char('\n'));
    sendCommand(command);
    m_input->clear();
}

void TerminalView::appendOutput(const QString& text) {
    m_output->moveCursor(QTextCursor::End);
    m_output->insertPlainText(text);
    m_output->verticalScrollBar()->setValue(m_output->verticalScrollBar()->maximum());
}
