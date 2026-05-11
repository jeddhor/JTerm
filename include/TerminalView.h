#pragma once

#include <QProcess>
#include <QWidget>

class QLineEdit;
class QPlainTextEdit;

class TerminalView : public QWidget {
    Q_OBJECT

public:
    explicit TerminalView(QWidget* parent = nullptr);

    void setShell(const QString& shellPath);
    QString shell() const;

    void startShell();
    void sendCommand(const QString& command);

    void copy();
    void paste();
    void selectAll();

signals:
    void becameActive();

protected:
    bool eventFilter(QObject* watched, QEvent* event) override;

private slots:
    void onReadyReadStdout();
    void onReadyReadStderr();
    void onProcessStarted();
    void onProcessFinished(int exitCode, QProcess::ExitStatus exitStatus);
    void onUserCommandEntered();

private:
    void appendOutput(const QString& text);

    QString m_shellPath;
    QProcess* m_process;
    QPlainTextEdit* m_output;
    QLineEdit* m_input;
};
