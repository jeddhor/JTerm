#pragma once

#include "AppSettings.h"

#include <QJsonObject>
#include <QMainWindow>
#include <QSet>

class CommandServer;
class QSplitter;
class TerminalPane;

class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    explicit MainWindow(QWidget* parent = nullptr);

private slots:
    void splitActivePaneHorizontal();
    void splitActivePaneVertical();
    void saveLayoutToFile();
    void loadLayoutFromFile();
    void showSettingsDialog();
    void applyTheme(const QString& themeName);
    void handleRemoteCommand(const QString& paneId, const QString& paneTitle, const QString& command);

private:
    void initializeUi();
    void createMenus();
    void createInitialPane();

    TerminalPane* createPane(const QString& title = QString());
    void replaceNodeInParent(QWidget* oldNode, QWidget* newNode);
    void splitPane(TerminalPane* pane, Qt::Orientation orientation);

    QJsonObject serializeNode(QWidget* node) const;
    QWidget* deserializeNode(const QJsonObject& nodeObject, bool* ok);

    void collectPanes(QWidget* node, QList<TerminalPane*>& outPanes) const;
    TerminalPane* findPaneById(const QString& paneId) const;
    TerminalPane* findPaneByTitle(const QString& paneTitle) const;
    QString nextPaneId();
    QString normalizePaneId(const QString& desiredId);

    void setActivePane(TerminalPane* pane);
    void wirePaneSignals(TerminalPane* pane);

    QWidget* m_rootNode;
    TerminalPane* m_activePane;
    int m_nextPaneCounter;
    QSet<QString> m_usedPaneIds;
    AppSettings m_settings;
    CommandServer* m_commandServer;
};
