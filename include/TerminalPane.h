#pragma once

#include <QWidget>

class QLabel;
class QCheckBox;
class QTimer;
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
    bool startupScriptThrottled() const;
    void setStartupScriptThrottled(bool enabled);
    void setStartupThrottleIntervalSeconds(int seconds);
    void runStartupScript();
    bool hasRunningProcess() const;
    void setMoveToTabVisible(bool visible);
    bool isBroadcastTargetChecked() const;
    void setBroadcastTargetChecked(bool checked);
    void setBroadcastTargetEnabled(bool enabled);
    void setBroadcastSourceSelected(bool selected);

    TerminalView* terminalView() const;

signals:
    void splitRequested(TerminalPane* pane, Qt::Orientation orientation);
    void activated(TerminalPane* pane);
    void closeRequested(TerminalPane* pane);
    void renameRequested(TerminalPane* pane);
    void renameTabRequested(TerminalPane* pane);
    void startupScriptRequested(TerminalPane* pane);
    void preferencesRequested(TerminalPane* pane);
    void copyRequested(TerminalPane* pane);
    void pasteRequested(TerminalPane* pane);
    void selectAllRequested(TerminalPane* pane);
    void moveToNewTabRequested(TerminalPane* pane);
    void broadcastTargetToggled(TerminalPane* pane, bool checked);
    void broadcastSourceToggleRequested(TerminalPane* pane);

private slots:
    void onInnerActivated();
    void runNextStartupStep();

protected:
    bool eventFilter(QObject* watched, QEvent* event) override;

private:
    void updateHeader();

    QString m_paneId;
    QString m_title;
    QString m_startupScript;
    bool m_startupScriptThrottled;
    int m_startupThrottleIntervalSeconds;
    QStringList m_pendingStartupSteps;

    QLabel* m_titleLabel;
    QLabel* m_startupIndicatorLabel;
    QWidget* m_titleBar;
    QCheckBox* m_broadcastTargetCheck;
    QToolButton* m_moveToTabButton;
    TerminalView* m_terminalView;
    QTimer* m_startupThrottleTimer;
    bool m_isBroadcastSource;
};
