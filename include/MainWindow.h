#pragma once

#include "AppSettings.h"
#include "SettingsDialog.h"

#include <QHash>
#include <QJsonObject>
#include <QMainWindow>
#include <QSet>
#include <QElapsedTimer>

class CommandServer;
class QAction;
class LlmChatDialog;
class QSystemTrayIcon;
class QTimer;
class QTabWidget;
class TerminalPane;
class QToolButton;
class QSplitter;
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
    void duplicateCurrentTab();
    void setCurrentTabColor();
    void clearCurrentTabColor();
    void autoArrangeCurrentTabPanes();
    void toggleActivePaneZoom();
    void searchInActivePane();
    void editLayoutJson();
    void saveLayoutToFile();
    void loadLayoutFromFile();
    void openProjectGithubPage();
    void showAboutDialog();
    void showLlmChatDialog();
    void showLlmSettingsDialog();
    void showSettingsDialog();
    void resetWindowLayout();
    void setMenuBarVisible(bool visible);
    void applyTheme(const QString& themeName);
    void handleRemoteCommand(const QString& paneId, const QString& paneTitle, const QString& command);
    void onTabCloseRequested(int index);
    void onCurrentTabChanged(int index);
    void checkLongRunningProcesses();

private:
    struct TabInfo {
        QString id;
        QString title;
        QString colorHex;
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
    void movePaneToNewTab(TerminalPane* pane);
    void editPaneStartupScript(TerminalPane* pane);

    void closeTabByIndex(int index);
    void renameTabByIndex(int index);
    void duplicateTabByIndex(int index);

    QJsonObject exportLayoutObject() const;
    bool importLayoutObject(const QJsonObject& rootObject, QString* errorMessage);

    QJsonObject serializeNode(QWidget* node) const;
    QWidget* deserializeNode(const QJsonObject& nodeObject, bool* ok, bool runStartupScripts = true);
    void applySnapScope(QWidget* node, QWidget* snapScope);
    bool nodeContainsStartupScripts(const QJsonObject& nodeObject) const;
    bool layoutContainsStartupScripts(const QJsonObject& layoutObject) const;
    bool confirmLoadLayoutWithStartupScripts() const;
    QString autoSavedLayoutPath() const;
    bool tryRestoreAutoSavedLayout();
    void saveAutoLayoutSnapshot();

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
    void collectSplitters(QWidget* node, QList<QSplitter*>& outSplitters) const;
    void refreshTabVisual(int index);
    void refreshLlmActionState();
    void focusActivePaneTerminal();
    void refreshMoveToTabButtonVisibility();
    void showSettingsDialogAtTab(SettingsDialog::InitialTab initialTab);
    void toggleBroadcastSource(TerminalPane* pane);
    void editBroadcastGroup(TerminalPane* pane);
    void clearBroadcastGroup(TerminalPane* pane);
    void applyBroadcastAllOverrideState();
    void clearBroadcastState();
    void relayBroadcastKeyPress(TerminalPane* sourcePane, int key, int modifiers, const QString& text);
    bool isRiskyCommandText(const QString& text) const;
    bool confirmRiskyBroadcast(const QString& commandText) const;
    bool confirmSafePaste(const QString& text) const;
    void requestPasteIntoPane(TerminalPane* pane);
    void handleOpenSelection(TerminalPane* pane);

    QTabWidget* m_tabWidget;
    QHash<QWidget*, TabInfo> m_tabInfos;

    TerminalPane* m_activePane;
    int m_nextPaneCounter;
    int m_nextTabCounter;
    QSet<QString> m_usedPaneIds;
    QSet<QString> m_usedTabIds;
    AppSettings m_settings;
    CommandServer* m_commandServer;
    QAction* m_askLlmAction;
    QAction* m_toggleMenuBarAction;
    LlmChatDialog* m_llmChatDialog;
    QToolButton* m_newTabButton;
    QSystemTrayIcon* m_trayIcon;
    QTimer* m_processWatchTimer;
    TerminalPane* m_broadcastSourcePane;
    bool m_broadcastRelaying;
    bool m_lastBroadcastAllOverrideState;
    QHash<TerminalPane*, QString> m_broadcastInputBuffers;
    QWidget* m_zoomedTabPage;
    QHash<QSplitter*, QList<int>> m_zoomedSplitterSizes;
    QHash<TerminalPane*, qint64> m_runningSinceMs;
};
