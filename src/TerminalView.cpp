#include "TerminalView.h"

#include <QCoreApplication>
#include <QEvent>
#include <QKeyEvent>
#include <QMetaObject>
#include <QMouseEvent>
#include <QVBoxLayout>

#include <qtermwidget6/qtermwidget.h>

TerminalView::TerminalView(QWidget* parent)
    : QWidget(parent)
    , m_colorSchemeName(QStringLiteral("WhiteOnBlack"))
    , m_terminal(new QTermWidget(0, this)) {
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);

    auto* rootLayout = new QVBoxLayout(this);
    rootLayout->setContentsMargins(0, 0, 0, 0);
    rootLayout->setSpacing(0);
    rootLayout->addWidget(m_terminal, 1);

    m_terminal->setTerminalSizeHint(false);
    m_terminal->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    setTerminalColorScheme(m_colorSchemeName);
    // Best-effort backend polish for hyperlink/escape handling where supported.
    QMetaObject::invokeMethod(m_terminal, "setUrlFilterEnabled", Q_ARG(bool, true));
    m_terminal->installEventFilter(this);
    connect(m_terminal, &QTermWidget::termKeyPressed, this, [this](QKeyEvent* event) {
        if (!event) {
            return;
        }
        emit keyPressed(event->key(), static_cast<int>(event->modifiers()), event->text());
    });
}

QStringList TerminalView::availableColorSchemes() {
    QTermWidget probe(0, nullptr);
    const QStringList schemes = probe.availableColorSchemes();
    if (!schemes.isEmpty()) {
        return schemes;
    }

    return {
        QStringLiteral("WhiteOnBlack"),
        QStringLiteral("BlackOnWhite"),
        QStringLiteral("Linux"),
        QStringLiteral("GreenOnBlack"),
        QStringLiteral("Solarized"),
        QStringLiteral("SolarizedLight")
    };
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

void TerminalView::setTerminalColorScheme(const QString& schemeName) {
    const QString normalized = schemeName.trimmed().isEmpty() ? QStringLiteral("WhiteOnBlack") : schemeName.trimmed();
    const QStringList available = availableColorSchemes();
    m_colorSchemeName = available.contains(normalized) ? normalized : QStringLiteral("WhiteOnBlack");
    m_terminal->setColorScheme(m_colorSchemeName);
}

QString TerminalView::terminalColorScheme() const {
    return m_colorSchemeName;
}

void TerminalView::setTerminalColors(const QColor& foreground, const QColor& background) {
    Q_UNUSED(foreground);
    Q_UNUSED(background);

    setTerminalColorScheme(m_colorSchemeName);
    m_terminal->setStyleSheet(QString());
}

int TerminalView::shellProcessId() const {
    return m_terminal->getShellPID();
}

int TerminalView::foregroundProcessId() const {
    return m_terminal->getForegroundProcessId();
}

bool TerminalView::hasRunningForegroundProcess() const {
    const int foregroundPid = foregroundProcessId();
    if (foregroundPid <= 0) {
        return false;
    }

    const int shellPid = shellProcessId();
    if (shellPid <= 0) {
        return foregroundPid > 0;
    }

    return foregroundPid != shellPid;
}

void TerminalView::copy() {
    m_terminal->copyClipboard();
}

void TerminalView::paste() {
    m_terminal->pasteClipboard();
}

void TerminalView::selectAll() {
    if (QMetaObject::invokeMethod(m_terminal, "selectAll")) {
        return;
    }

    m_terminal->setFocus();
    QKeyEvent press(QEvent::KeyPress, Qt::Key_A, Qt::ControlModifier | Qt::ShiftModifier);
    QKeyEvent release(QEvent::KeyRelease, Qt::Key_A, Qt::ControlModifier | Qt::ShiftModifier);
    QCoreApplication::sendEvent(m_terminal, &press);
    QCoreApplication::sendEvent(m_terminal, &release);
}

bool TerminalView::findInScrollback(const QString& query, bool forward) {
    const QString needle = query.trimmed();
    if (needle.isEmpty()) {
        return false;
    }

    if (QMetaObject::invokeMethod(m_terminal, "find", Q_ARG(QString, needle))) {
        return true;
    }
    if (QMetaObject::invokeMethod(m_terminal, "findText", Q_ARG(QString, needle))) {
        return true;
    }
    if (QMetaObject::invokeMethod(m_terminal, "search", Q_ARG(QString, needle), Q_ARG(bool, forward))) {
        return true;
    }
    if (QMetaObject::invokeMethod(m_terminal, "search", Q_ARG(QString, needle))) {
        return true;
    }

    return false;
}

QString TerminalView::selectedText() const {
    QString result;
    if (QMetaObject::invokeMethod(const_cast<QTermWidget*>(m_terminal), "selectedText", Q_RETURN_ARG(QString, result))) {
        return result;
    }
    if (QMetaObject::invokeMethod(const_cast<QTermWidget*>(m_terminal), "selection", Q_RETURN_ARG(QString, result))) {
        return result;
    }
    return QString();
}

void TerminalView::focusTerminal() {
    m_terminal->setFocus(Qt::OtherFocusReason);
}

void TerminalView::sendKeyPress(int key, Qt::KeyboardModifiers modifiers, const QString& text) {
    QKeyEvent event(QEvent::KeyPress, key, modifiers, text);
    m_terminal->sendKeyEvent(&event);
}

bool TerminalView::eventFilter(QObject* watched, QEvent* event) {
    if (watched == m_terminal && event->type() == QEvent::KeyPress) {
        auto* keyEvent = static_cast<QKeyEvent*>(event);
        const bool pasteChord =
            ((keyEvent->modifiers() & Qt::ControlModifier) && keyEvent->key() == Qt::Key_V)
            || ((keyEvent->modifiers() & Qt::ShiftModifier) && keyEvent->key() == Qt::Key_Insert);
        if (pasteChord) {
            emit pasteShortcutRequested();
            emit becameActive();
            return true;
        }
    }

    if (watched == m_terminal && event->type() == QEvent::MouseButtonPress) {
        auto* mouseEvent = static_cast<QMouseEvent*>(event);
        if (mouseEvent->button() == Qt::MiddleButton) {
            m_terminal->pasteSelection();
            emit becameActive();
            return true;
        }

        if (mouseEvent->button() == Qt::LeftButton && (mouseEvent->modifiers() & Qt::ControlModifier)) {
            emit openSelectionRequested();
            emit becameActive();
            return true;
        }
    }

    if (watched == m_terminal && event->type() == QEvent::FocusIn) {
        emit becameActive();
    }
    return QWidget::eventFilter(watched, event);
}
