#pragma once

#include <QWidget>

class QLabel;
class TerminalView;
class QToolButton;

class TerminalPane : public QWidget {
    Q_OBJECT

public:
    explicit TerminalPane(const QString& paneId, const QString& shellPath, QWidget* parent = nullptr);

    QString paneId() const;
    QString title() const;
    void setTitle(const QString& title);
    QString startupScript() const;
    void setStartupScript(const QString& script);
    void runStartupScript();
    bool hasRunningProcess() const;
    void setMoveToTabVisible(bool visible);

    TerminalView* terminalView() const;

signals:
    void splitRequested(TerminalPane* pane, Qt::Orientation orientation);
    void activated(TerminalPane* pane);
    void closeRequested(TerminalPane* pane);
    void renameRequested(TerminalPane* pane);
    void startupScriptRequested(TerminalPane* pane);
    void preferencesRequested(TerminalPane* pane);
    void copyRequested(TerminalPane* pane);
    void pasteRequested(TerminalPane* pane);
    void selectAllRequested(TerminalPane* pane);
    void moveToNewTabRequested(TerminalPane* pane);

private slots:
    void onInnerActivated();

protected:
    bool eventFilter(QObject* watched, QEvent* event) override;

private:
    void updateHeader();

    QString m_paneId;
    QString m_title;
    QString m_startupScript;

    QLabel* m_titleLabel;
    QLabel* m_startupIndicatorLabel;
    QWidget* m_titleBar;
    QToolButton* m_moveToTabButton;
    TerminalView* m_terminalView;
};
