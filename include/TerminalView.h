#pragma once

#include <QColor>
#include <QWidget>

class QTermWidget;

class TerminalView : public QWidget {
    Q_OBJECT

public:
    explicit TerminalView(QWidget* parent = nullptr);
    static QStringList availableColorSchemes();

    void setShell(const QString& shellPath);
    QString shell() const;

    void startShell();
    void sendCommand(const QString& command);
    void setTerminalColorScheme(const QString& schemeName);
    QString terminalColorScheme() const;
    void setTerminalColors(const QColor& foreground, const QColor& background);
    int shellProcessId() const;
    int foregroundProcessId() const;
    bool hasRunningForegroundProcess() const;

    void copy();
    void paste();
    void selectAll();
    void focusTerminal();
    void sendKeyPress(int key, Qt::KeyboardModifiers modifiers, const QString& text = QString());

signals:
    void becameActive();
    void keyPressed(int key, int modifiers, const QString& text);

protected:
    bool eventFilter(QObject* watched, QEvent* event) override;

private:
    QString m_shellPath;
    QString m_colorSchemeName;
    QTermWidget* m_terminal;
};
