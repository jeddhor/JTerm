#pragma once

#include "AppSettings.h"

#include <QHash>
#include <QJsonObject>
#include <QMainWindow>
#include <QSet>

class CommandServer;
class QTabWidget;
class TerminalPane;
class QWidget;

class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    explicit MainWindow(QWidget* parent = nullptr);
    bool loadLayoutFromPath(const QString& path, QString* errorMessage = nullptr);

protected:
    void closeEvent(QCloseEvent* event) override;

private slots:
    void splitActivePaneHorizontal();
    void splitActivePaneVertical();
    void closeActivePane();
    void renameActivePane();
    void createNewTab();
    void closeCurrentTab();
    void renameCurrentTab();
    void editLayoutJson();
    void saveLayoutToFile();
    void loadLayoutFromFile();
    void openProjectGithubPage();
    void showAboutDialog();
    void showSettingsDialog();
    void applyTheme(const QString& themeName);
    void handleRemoteCommand(const QString& paneId, const QString& paneTitle, const QString& command);
    void onTabCloseRequested(int index);
    void onCurrentTabChanged(int index);

private:
    struct TabInfo {
        QString id;
        QString title;
        QWidget* rootNode = nullptr;
    };

    void initializeUi();
    void createMenus();
    QWidget* createTabPage(const QString& tabId, const QString& tabTitle, QWidget* initialRootNode = nullptr);
    TabInfo* tabInfoForPage(QWidget* page);
    const TabInfo* tabInfoForPage(QWidget* page) const;
    QWidget* currentTabPage() const;
    TabInfo* currentTabInfo();
    const TabInfo* currentTabInfo() const;

    TerminalPane* createPane(const QString& title = QString(), const QString& forcedPaneId = QString());
    void replaceNodeInParent(QWidget* tabPage, QWidget* oldNode, QWidget* newNode);
    void splitPane(TerminalPane* pane, Qt::Orientation orientation);
    void closePane(TerminalPane* pane);
    void renamePane(TerminalPane* pane);
    void editPaneStartupScript(TerminalPane* pane);

    void closeTabByIndex(int index);
    void renameTabByIndex(int index);

    QJsonObject exportLayoutObject() const;
    bool importLayoutObject(const QJsonObject& rootObject, QString* errorMessage);

    QJsonObject serializeNode(QWidget* node) const;
    QWidget* deserializeNode(const QJsonObject& nodeObject, bool* ok);
    void applySnapScope(QWidget* node, QWidget* snapScope);
    bool nodeContainsStartupScripts(const QJsonObject& nodeObject) const;
    bool layoutContainsStartupScripts(const QJsonObject& layoutObject) const;
    bool confirmLoadLayoutWithStartupScripts() const;

    void collectPanes(QWidget* node, QList<TerminalPane*>& outPanes) const;
    void collectAllPanes(QList<TerminalPane*>& outPanes) const;
    QList<TerminalPane*> busyPanes(const QList<TerminalPane*>& panes) const;
    bool confirmCloseBusyPanes(const QList<TerminalPane*>& panes, const QString& scopeName) const;
    TerminalPane* findPaneById(const QString& paneId) const;
    TerminalPane* findPaneByTitle(const QString& paneTitle) const;
    QString nextPaneId();
    QString normalizePaneId(const QString& desiredId);
    QString nextTabId();
    QString normalizeTabId(const QString& desiredId);

    void setActivePane(TerminalPane* pane);
    void wirePaneSignals(TerminalPane* pane);
    void syncActivePaneToCurrentTab();
    void refreshTabVisual(int index);

    QTabWidget* m_tabWidget;
    QHash<QWidget*, TabInfo> m_tabInfos;

    TerminalPane* m_activePane;
    int m_nextPaneCounter;
    int m_nextTabCounter;
    QSet<QString> m_usedPaneIds;
    QSet<QString> m_usedTabIds;
    AppSettings m_settings;
    CommandServer* m_commandServer;
};
