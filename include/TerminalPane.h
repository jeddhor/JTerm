#pragma once

#include <QWidget>

class QLabel;
class TerminalView;

class TerminalPane : public QWidget {
    Q_OBJECT

public:
    explicit TerminalPane(const QString& paneId, const QString& shellPath, QWidget* parent = nullptr);

    QString paneId() const;
    QString title() const;
    void setTitle(const QString& title);

    TerminalView* terminalView() const;

signals:
    void splitRequested(TerminalPane* pane, Qt::Orientation orientation);
    void activated(TerminalPane* pane);
    void closeRequested(TerminalPane* pane);
    void renameRequested(TerminalPane* pane);
    void preferencesRequested(TerminalPane* pane);
    void copyRequested(TerminalPane* pane);
    void pasteRequested(TerminalPane* pane);
    void selectAllRequested(TerminalPane* pane);

private slots:
    void onInnerActivated();

private:
    void updateHeader();

    QString m_paneId;
    QString m_title;

    QLabel* m_idLabel;
    QLabel* m_titleLabel;
    TerminalView* m_terminalView;
};
