#pragma once

#include <QWidget>

class QLineEdit;
class QLabel;
class QPushButton;
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

private slots:
    void onTitleEdited(const QString& newTitle);
    void onInnerActivated();

private:
    QString m_paneId;

    QLabel* m_idLabel;
    QLineEdit* m_titleEdit;
    QPushButton* m_splitHorizontalButton;
    QPushButton* m_splitVerticalButton;
    TerminalView* m_terminalView;
};
