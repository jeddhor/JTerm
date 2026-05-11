#pragma once

#include <QColor>
#include <QWidget>

class QTermWidget;

class TerminalView : public QWidget {
    Q_OBJECT

public:
    explicit TerminalView(QWidget* parent = nullptr);

    void setShell(const QString& shellPath);
    QString shell() const;

    void startShell();
    void sendCommand(const QString& command);
    void setTerminalColors(const QColor& foreground, const QColor& background);

    void copy();
    void paste();
    void selectAll();

signals:
    void becameActive();

protected:
    bool eventFilter(QObject* watched, QEvent* event) override;

private:
    QString m_shellPath;
    QTermWidget* m_terminal;
};
