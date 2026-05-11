#include "TerminalView.h"

#include <QCoreApplication>
#include <QEvent>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QMetaObject>
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
    m_terminal->installEventFilter(this);
}

QStringList TerminalView::availableColorSchemes() {
    QTermWidget probe(0, nullptr);
    QStringList schemes;
    if (QMetaObject::invokeMethod(&probe, "availableColorSchemes", Q_RETURN_ARG(QStringList, schemes)) && !schemes.isEmpty()) {
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
    m_colorSchemeName = normalized;

    const bool applied = QMetaObject::invokeMethod(m_terminal, "setColorScheme", Q_ARG(QString, normalized));
    if (!applied) {
        m_colorSchemeName = QStringLiteral("WhiteOnBlack");
        QMetaObject::invokeMethod(m_terminal, "setColorScheme", Q_ARG(QString, m_colorSchemeName));
    }
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

bool TerminalView::eventFilter(QObject* watched, QEvent* event) {
    if (watched == m_terminal && event->type() == QEvent::MouseButtonPress) {
        auto* mouseEvent = static_cast<QMouseEvent*>(event);
        if (mouseEvent->button() == Qt::MiddleButton) {
            m_terminal->pasteSelection();
            emit becameActive();
            return true;
        }
    }

    if (watched == m_terminal && event->type() == QEvent::FocusIn) {
        emit becameActive();
    }
    return QWidget::eventFilter(watched, event);
}
