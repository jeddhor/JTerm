#include "MainWindow.h"

#include "AppSettings.h"
#include "CommandServer.h"
#include "SettingsDialog.h"
#include "TerminalPane.h"
#include "TerminalView.h"
#include "ThemeManager.h"

#include <QApplication>
#include <QFile>
#include <QFileDialog>
#include <QInputDialog>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLayout>
#include <QMenu>
#include <QMenuBar>
#include <QMessageBox>
#include <QSplitter>
#include <QStatusBar>
#include <QStyle>
#include <QTabBar>
#include <QTabWidget>
#include <QVBoxLayout>
#include <QtGlobal>

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent)
    , m_tabWidget(nullptr)
    , m_activePane(nullptr)
    , m_nextPaneCounter(1)
    , m_nextTabCounter(1)
    , m_settings(SettingsStore::load())
    , m_commandServer(new CommandServer(this)) {
    initializeUi();
    createMenus();
    createNewTab();

    applyTheme(QString());

    connect(m_commandServer, &CommandServer::commandReceived, this, &MainWindow::handleRemoteCommand);
    if (!m_commandServer->startListening()) {
        QMessageBox::warning(this, QStringLiteral("IPC unavailable"), QStringLiteral("Another instance may already hold the command socket."));
    }
}

void MainWindow::splitActivePaneHorizontal() {
    if (!m_activePane) {
        return;
    }
    splitPane(m_activePane, Qt::Vertical);
}

void MainWindow::splitActivePaneVertical() {
    if (!m_activePane) {
        return;
    }
    splitPane(m_activePane, Qt::Horizontal);
}

void MainWindow::closeActivePane() {
    if (!m_activePane) {
        return;
    }
    closePane(m_activePane);
}

void MainWindow::renameActivePane() {
    if (!m_activePane) {
        return;
    }
    renamePane(m_activePane);
}

void MainWindow::createNewTab() {
    const QString tabId = nextTabId();
    const QString tabTitle = QStringLiteral("Tab ") + tabId;
    QWidget* page = createTabPage(tabId, tabTitle, nullptr);
    m_tabWidget->setCurrentWidget(page);
}

void MainWindow::closeCurrentTab() {
    closeTabByIndex(m_tabWidget->currentIndex());
}

void MainWindow::renameCurrentTab() {
    renameTabByIndex(m_tabWidget->currentIndex());
}

void MainWindow::saveLayoutToFile() {
    if (m_tabWidget->count() == 0) {
        return;
    }

    const QString path = QFileDialog::getSaveFileName(this, QStringLiteral("Save Layout"), QStringLiteral("layout.json"), QStringLiteral("JSON (*.json)"));
    if (path.isEmpty()) {
        return;
    }

    QJsonObject rootObject;
    rootObject.insert(QStringLiteral("version"), 2);

    QJsonArray tabsArray;
    for (int i = 0; i < m_tabWidget->count(); ++i) {
        QWidget* page = m_tabWidget->widget(i);
        const TabInfo* info = tabInfoForPage(page);
        if (!info || !info->rootNode) {
            continue;
        }

        QJsonObject tabObject;
        tabObject.insert(QStringLiteral("id"), info->id);
        tabObject.insert(QStringLiteral("title"), info->title);
        tabObject.insert(QStringLiteral("root"), serializeNode(info->rootNode));
        tabsArray.append(tabObject);
    }

    rootObject.insert(QStringLiteral("tabs"), tabsArray);
    const TabInfo* currentInfo = currentTabInfo();
    if (currentInfo) {
        rootObject.insert(QStringLiteral("currentTabId"), currentInfo->id);
    }

    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        QMessageBox::critical(this, QStringLiteral("Save failed"), QStringLiteral("Could not open file for writing."));
        return;
    }

    file.write(QJsonDocument(rootObject).toJson(QJsonDocument::Indented));
}

void MainWindow::loadLayoutFromFile() {
    const QString path = QFileDialog::getOpenFileName(this, QStringLiteral("Load Layout"), QString(), QStringLiteral("JSON (*.json)"));
    if (path.isEmpty()) {
        return;
    }

    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        QMessageBox::critical(this, QStringLiteral("Load failed"), QStringLiteral("Could not open layout file."));
        return;
    }

    const QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
    if (!doc.isObject()) {
        QMessageBox::critical(this, QStringLiteral("Load failed"), QStringLiteral("Invalid layout JSON."));
        return;
    }

    const QJsonObject rootObject = doc.object();

    while (m_tabWidget->count() > 0) {
        QWidget* page = m_tabWidget->widget(0);
        m_tabInfos.remove(page);
        m_tabWidget->removeTab(0);
        page->deleteLater();
    }

    m_usedPaneIds.clear();
    m_usedTabIds.clear();
    m_nextPaneCounter = 1;
    m_nextTabCounter = 1;
    m_activePane = nullptr;

    const int version = rootObject.value(QStringLiteral("version")).toInt(1);
    if (version <= 1) {
        bool ok = true;
        QWidget* rootNode = deserializeNode(rootObject.value(QStringLiteral("root")).toObject(), &ok);
        if (!ok || !rootNode) {
            QMessageBox::critical(this, QStringLiteral("Load failed"), QStringLiteral("Could not deserialize layout."));
            createNewTab();
            return;
        }
        createTabPage(nextTabId(), QStringLiteral("Tab 1"), rootNode);
        m_tabWidget->setCurrentIndex(0);
        syncActivePaneToCurrentTab();
        applyTheme(QString());
        return;
    }

    const QJsonArray tabsArray = rootObject.value(QStringLiteral("tabs")).toArray();
    for (const QJsonValue& tabValue : tabsArray) {
        if (!tabValue.isObject()) {
            continue;
        }
        const QJsonObject tabObject = tabValue.toObject();

        bool ok = true;
        QWidget* rootNode = deserializeNode(tabObject.value(QStringLiteral("root")).toObject(), &ok);
        if (!ok || !rootNode) {
            if (rootNode) {
                rootNode->deleteLater();
            }
            continue;
        }

        QString tabId = normalizeTabId(tabObject.value(QStringLiteral("id")).toString());
        QString tabTitle = tabObject.value(QStringLiteral("title")).toString();
        if (tabTitle.trimmed().isEmpty()) {
            tabTitle = QStringLiteral("Tab ") + tabId;
        }

        createTabPage(tabId, tabTitle, rootNode);
    }

    if (m_tabWidget->count() == 0) {
        createNewTab();
    }

    const QString currentTabId = rootObject.value(QStringLiteral("currentTabId")).toString();
    if (!currentTabId.isEmpty()) {
        for (int i = 0; i < m_tabWidget->count(); ++i) {
            QWidget* page = m_tabWidget->widget(i);
            const TabInfo* info = tabInfoForPage(page);
            if (info && info->id == currentTabId) {
                m_tabWidget->setCurrentIndex(i);
                break;
            }
        }
    }

    syncActivePaneToCurrentTab();
    applyTheme(QString());
}

void MainWindow::showSettingsDialog() {
    SettingsDialog dialog(m_settings, this);
    if (dialog.exec() != QDialog::Accepted) {
        return;
    }

    m_settings = dialog.settings();
    if (m_settings.defaultShell.isEmpty()) {
#ifdef Q_OS_WIN
        m_settings.defaultShell = QStringLiteral("powershell.exe");
#else
        m_settings.defaultShell = QStringLiteral("/bin/bash");
#endif
    }

    SettingsStore::save(m_settings);
    applyTheme(QString());
}

void MainWindow::applyTheme(const QString& themeName) {
    ThemeManager::applyTheme(*qApp, themeName);

    const TerminalColors colors = ThemeManager::terminalColorsForTheme(themeName);
    QList<TerminalPane*> panes;
    collectAllPanes(panes);

    for (TerminalPane* pane : panes) {
        TerminalView* view = pane->terminalView();
        if (!view) {
            continue;
        }
        view->setTerminalColors(colors.foreground, colors.background);
    }

    setActivePane(m_activePane);
}

void MainWindow::handleRemoteCommand(const QString& paneId, const QString& paneTitle, const QString& command) {
    TerminalPane* target = nullptr;
    if (!paneId.isEmpty()) {
        target = findPaneById(paneId);
    }
    if (!target && !paneTitle.isEmpty()) {
        target = findPaneByTitle(paneTitle);
    }

    if (!target) {
        statusBar()->showMessage(QStringLiteral("Remote command failed: target pane not found."), 4000);
        return;
    }

    target->terminalView()->sendCommand(command);

    for (int i = 0; i < m_tabWidget->count(); ++i) {
        QWidget* page = m_tabWidget->widget(i);
        if (page == target || page->isAncestorOf(target)) {
            m_tabWidget->setCurrentIndex(i);
            break;
        }
    }

    setActivePane(target);
    statusBar()->showMessage(QStringLiteral("Remote command sent to pane ") + target->paneId(), 2500);
}

void MainWindow::onTabCloseRequested(int index) {
    closeTabByIndex(index);
}

void MainWindow::onCurrentTabChanged(int) {
    syncActivePaneToCurrentTab();
}

void MainWindow::initializeUi() {
    setWindowTitle(QStringLiteral("SplitTerm"));
    resize(1500, 900);

    auto* host = new QWidget(this);
    auto* hostLayout = new QVBoxLayout(host);
    hostLayout->setContentsMargins(8, 8, 8, 8);
    hostLayout->setSpacing(6);

    m_tabWidget = new QTabWidget(host);
    m_tabWidget->setDocumentMode(true);
    m_tabWidget->setMovable(true);
    m_tabWidget->setTabsClosable(true);

    QTabBar* tabBar = m_tabWidget->tabBar();
    tabBar->setContextMenuPolicy(Qt::CustomContextMenu);

    connect(m_tabWidget, &QTabWidget::tabCloseRequested, this, &MainWindow::onTabCloseRequested);
    connect(m_tabWidget, &QTabWidget::currentChanged, this, &MainWindow::onCurrentTabChanged);
    connect(tabBar, &QWidget::customContextMenuRequested, this, [this, tabBar](const QPoint& pos) {
        const int index = tabBar->tabAt(pos);
        QMenu menu(this);
        QAction* newTabAction = menu.addAction(QStringLiteral("New Tab"));
        QAction* renameTabAction = nullptr;
        QAction* closeTabAction = nullptr;
        if (index >= 0) {
            renameTabAction = menu.addAction(QStringLiteral("Rename Tab..."));
            closeTabAction = menu.addAction(QStringLiteral("Close Tab"));
        }

        QAction* chosen = menu.exec(tabBar->mapToGlobal(pos));
        if (!chosen) {
            return;
        }
        if (chosen == newTabAction) {
            createNewTab();
        } else if (chosen == renameTabAction) {
            renameTabByIndex(index);
        } else if (chosen == closeTabAction) {
            closeTabByIndex(index);
        }
    });

    hostLayout->addWidget(m_tabWidget, 1);
    setCentralWidget(host);
}

void MainWindow::createMenus() {
    auto* fileMenu = menuBar()->addMenu(QStringLiteral("&File"));
    fileMenu->addAction(QStringLiteral("Save Layout..."), this, &MainWindow::saveLayoutToFile, QKeySequence(QStringLiteral("Ctrl+S")));
    fileMenu->addAction(QStringLiteral("Load Layout..."), this, &MainWindow::loadLayoutFromFile, QKeySequence(QStringLiteral("Ctrl+O")));
    fileMenu->addSeparator();
    fileMenu->addAction(QStringLiteral("Exit"), this, &QWidget::close, QKeySequence(QStringLiteral("Ctrl+Q")));

    auto* editMenu = menuBar()->addMenu(QStringLiteral("&Edit"));
    editMenu->addAction(QStringLiteral("Copy"), this, [this]() {
        if (m_activePane) {
            m_activePane->terminalView()->copy();
        }
    }, QKeySequence::Copy);
    editMenu->addAction(QStringLiteral("Paste"), this, [this]() {
        if (m_activePane) {
            m_activePane->terminalView()->paste();
        }
    }, QKeySequence::Paste);
    editMenu->addAction(QStringLiteral("Select All"), this, [this]() {
        if (m_activePane) {
            m_activePane->terminalView()->selectAll();
        }
    }, QKeySequence::SelectAll);

    auto* paneMenu = menuBar()->addMenu(QStringLiteral("&Pane"));
    paneMenu->addAction(QStringLiteral("Split Horizontally"), this, &MainWindow::splitActivePaneHorizontal, QKeySequence(QStringLiteral("Ctrl+Shift+H")));
    paneMenu->addAction(QStringLiteral("Split Vertically"), this, &MainWindow::splitActivePaneVertical, QKeySequence(QStringLiteral("Ctrl+Shift+V")));
    paneMenu->addAction(QStringLiteral("Rename Pane..."), this, &MainWindow::renameActivePane, QKeySequence(QStringLiteral("Ctrl+Shift+R")));
    paneMenu->addAction(QStringLiteral("Close Terminal"), this, &MainWindow::closeActivePane, QKeySequence(QStringLiteral("Ctrl+Shift+W")));

    auto* tabMenu = menuBar()->addMenu(QStringLiteral("&Tab"));
    tabMenu->addAction(QStringLiteral("New Tab"), this, &MainWindow::createNewTab, QKeySequence(QStringLiteral("Ctrl+T")));
    tabMenu->addAction(QStringLiteral("Rename Tab..."), this, &MainWindow::renameCurrentTab, QKeySequence(QStringLiteral("Ctrl+Alt+R")));
    tabMenu->addAction(QStringLiteral("Close Tab"), this, &MainWindow::closeCurrentTab, QKeySequence(QStringLiteral("Ctrl+W")));

    auto* settingsMenu = menuBar()->addMenu(QStringLiteral("&Settings"));
    settingsMenu->addAction(QStringLiteral("Preferences..."), this, &MainWindow::showSettingsDialog, QKeySequence(QStringLiteral("Ctrl+,")));
}

QWidget* MainWindow::createTabPage(const QString& tabId, const QString& tabTitle, QWidget* initialRootNode) {
    QWidget* page = new QWidget(this);
    auto* pageLayout = new QVBoxLayout(page);
    pageLayout->setContentsMargins(2, 2, 2, 2);
    pageLayout->setSpacing(2);

    QWidget* rootNode = initialRootNode;
    if (!rootNode) {
        rootNode = createPane();
    }
    rootNode->setParent(page);
    pageLayout->addWidget(rootNode, 1);

    TabInfo info;
    info.id = tabId;
    info.title = tabTitle.trimmed().isEmpty() ? (QStringLiteral("Tab ") + tabId) : tabTitle.trimmed();
    info.rootNode = rootNode;
    m_tabInfos.insert(page, info);

    const int index = m_tabWidget->addTab(page, info.title);
    refreshTabVisual(index);
    return page;
}

MainWindow::TabInfo* MainWindow::tabInfoForPage(QWidget* page) {
    if (!page) {
        return nullptr;
    }
    auto it = m_tabInfos.find(page);
    if (it == m_tabInfos.end()) {
        return nullptr;
    }
    return &it.value();
}

const MainWindow::TabInfo* MainWindow::tabInfoForPage(QWidget* page) const {
    if (!page) {
        return nullptr;
    }
    auto it = m_tabInfos.constFind(page);
    if (it == m_tabInfos.constEnd()) {
        return nullptr;
    }
    return &it.value();
}

QWidget* MainWindow::currentTabPage() const {
    return m_tabWidget->currentWidget();
}

MainWindow::TabInfo* MainWindow::currentTabInfo() {
    return tabInfoForPage(currentTabPage());
}

const MainWindow::TabInfo* MainWindow::currentTabInfo() const {
    return tabInfoForPage(currentTabPage());
}

TerminalPane* MainWindow::createPane(const QString& title, const QString& forcedPaneId) {
    const QString paneId = forcedPaneId.isEmpty() ? nextPaneId() : normalizePaneId(forcedPaneId);
    TerminalPane* pane = new TerminalPane(paneId, m_settings.defaultShell, this);
    if (!title.isEmpty()) {
        pane->setTitle(title);
    }
    wirePaneSignals(pane);
    return pane;
}

void MainWindow::replaceNodeInParent(QWidget* tabPage, QWidget* oldNode, QWidget* newNode) {
    TabInfo* info = tabInfoForPage(tabPage);
    if (!info) {
        return;
    }

    QWidget* parent = oldNode->parentWidget();
    if (!parent) {
        info->rootNode = newNode;
        return;
    }

    if (auto* parentSplitter = qobject_cast<QSplitter*>(parent)) {
        const int index = parentSplitter->indexOf(oldNode);
        if (index < 0) {
            return;
        }
        parentSplitter->replaceWidget(index, newNode);
        oldNode->setParent(nullptr);
        return;
    }

    if (parent == tabPage) {
        auto* layout = tabPage->layout();
        layout->removeWidget(oldNode);
        oldNode->setParent(nullptr);
        newNode->setParent(tabPage);
        layout->addWidget(newNode);
        info->rootNode = newNode;
    }
}

void MainWindow::splitPane(TerminalPane* pane, Qt::Orientation orientation) {
    if (!pane) {
        return;
    }

    QWidget* tabPage = nullptr;
    for (int i = 0; i < m_tabWidget->count(); ++i) {
        QWidget* candidate = m_tabWidget->widget(i);
        if (candidate == pane || candidate->isAncestorOf(pane)) {
            tabPage = candidate;
            break;
        }
    }
    if (!tabPage) {
        return;
    }

    QList<TerminalPane*> allPanes;
    collectAllPanes(allPanes);
    if (allPanes.size() >= m_settings.maxPanes) {
        QMessageBox::information(this, QStringLiteral("Pane limit reached"), QStringLiteral("Increase pane limit in Settings to split further."));
        return;
    }

    QWidget* parentNode = pane->parentWidget();
    auto* sibling = createPane();

    if (auto* parentSplitter = qobject_cast<QSplitter*>(parentNode)) {
        if (parentSplitter->orientation() == orientation) {
            const int index = parentSplitter->indexOf(pane);
            parentSplitter->insertWidget(index + 1, sibling);
            parentSplitter->setChildrenCollapsible(false);
            parentSplitter->setStretchFactor(index, 1);
            parentSplitter->setStretchFactor(index + 1, 1);

            QList<int> sizes = parentSplitter->sizes();
            if (index >= 0 && index < sizes.size()) {
                const int current = sizes[index];
                const int first = qMax(80, current / 2);
                const int second = qMax(80, current - first);
                sizes[index] = first;
                sizes.insert(index + 1, second);
                parentSplitter->setSizes(sizes);
            }
            setActivePane(sibling);
            return;
        }
    }

    auto* newSplitter = new QSplitter(orientation, tabPage);
    newSplitter->setChildrenCollapsible(false);
    replaceNodeInParent(tabPage, pane, newSplitter);

    pane->setParent(newSplitter);
    sibling->setParent(newSplitter);
    newSplitter->addWidget(pane);
    newSplitter->addWidget(sibling);
    newSplitter->setStretchFactor(0, 1);
    newSplitter->setStretchFactor(1, 1);
    newSplitter->setSizes({500, 500});

    setActivePane(sibling);
}

void MainWindow::closePane(TerminalPane* pane) {
    if (!pane) {
        return;
    }

    QWidget* tabPage = nullptr;
    for (int i = 0; i < m_tabWidget->count(); ++i) {
        QWidget* candidate = m_tabWidget->widget(i);
        if (candidate == pane || candidate->isAncestorOf(pane)) {
            tabPage = candidate;
            break;
        }
    }
    if (!tabPage) {
        return;
    }

    TabInfo* info = tabInfoForPage(tabPage);
    if (!info || !info->rootNode) {
        return;
    }

    QList<TerminalPane*> tabPanes;
    collectPanes(info->rootNode, tabPanes);
    if (tabPanes.size() <= 1) {
        statusBar()->showMessage(QStringLiteral("A tab must contain at least one terminal."), 2500);
        return;
    }

    auto* parentSplitter = qobject_cast<QSplitter*>(pane->parentWidget());
    if (!parentSplitter) {
        return;
    }

    if (parentSplitter->count() > 2) {
        pane->setParent(nullptr);
        pane->deleteLater();
    } else {
        QWidget* survivor = nullptr;
        for (int i = 0; i < parentSplitter->count(); ++i) {
            QWidget* child = parentSplitter->widget(i);
            if (child != pane) {
                survivor = child;
                break;
            }
        }
        if (!survivor) {
            return;
        }

        QWidget* grandParent = parentSplitter->parentWidget();
        pane->setParent(nullptr);
        pane->deleteLater();

        if (auto* grandSplitter = qobject_cast<QSplitter*>(grandParent)) {
            const int index = grandSplitter->indexOf(parentSplitter);
            survivor->setParent(grandSplitter);
            grandSplitter->insertWidget(index, survivor);
            parentSplitter->deleteLater();
        } else if (grandParent == tabPage) {
            tabPage->layout()->removeWidget(parentSplitter);
            survivor->setParent(tabPage);
            tabPage->layout()->addWidget(survivor);
            info->rootNode = survivor;
            parentSplitter->deleteLater();
        }
    }

    QList<TerminalPane*> refreshed;
    collectPanes(info->rootNode, refreshed);
    if (!refreshed.isEmpty()) {
        setActivePane(refreshed.first());
    }
}

void MainWindow::renamePane(TerminalPane* pane) {
    if (!pane) {
        return;
    }

    bool ok = false;
    const QString newTitle = QInputDialog::getText(this, QStringLiteral("Rename Pane"), QStringLiteral("Pane title:"), QLineEdit::Normal, pane->title(), &ok);
    if (!ok) {
        return;
    }

    pane->setTitle(newTitle);
    setActivePane(pane);
}

void MainWindow::closeTabByIndex(int index) {
    if (index < 0 || index >= m_tabWidget->count()) {
        return;
    }
    if (m_tabWidget->count() <= 1) {
        close();
        return;
    }

    QWidget* page = m_tabWidget->widget(index);
    if (m_activePane && (page == m_activePane || page->isAncestorOf(m_activePane))) {
        m_activePane = nullptr;
    }

    m_tabInfos.remove(page);
    m_tabWidget->removeTab(index);
    page->deleteLater();

    if (m_tabWidget->count() == 0) {
        createNewTab();
    }
    syncActivePaneToCurrentTab();
}

void MainWindow::renameTabByIndex(int index) {
    if (index < 0 || index >= m_tabWidget->count()) {
        return;
    }

    QWidget* page = m_tabWidget->widget(index);
    TabInfo* info = tabInfoForPage(page);
    if (!info) {
        return;
    }

    bool ok = false;
    const QString newTitle = QInputDialog::getText(this, QStringLiteral("Rename Tab"), QStringLiteral("Tab title:"), QLineEdit::Normal, info->title, &ok);
    if (!ok) {
        return;
    }

    info->title = newTitle.trimmed().isEmpty() ? (QStringLiteral("Tab ") + info->id) : newTitle.trimmed();
    refreshTabVisual(index);
}

QJsonObject MainWindow::serializeNode(QWidget* node) const {
    QJsonObject nodeObject;

    if (auto* pane = qobject_cast<TerminalPane*>(node)) {
        nodeObject.insert(QStringLiteral("type"), QStringLiteral("pane"));
        nodeObject.insert(QStringLiteral("id"), pane->paneId());
        nodeObject.insert(QStringLiteral("title"), pane->title());
        return nodeObject;
    }

    auto* splitter = qobject_cast<QSplitter*>(node);
    if (!splitter) {
        return nodeObject;
    }

    nodeObject.insert(QStringLiteral("type"), QStringLiteral("splitter"));
    nodeObject.insert(QStringLiteral("orientation"), splitter->orientation() == Qt::Horizontal ? QStringLiteral("horizontal") : QStringLiteral("vertical"));

    QJsonArray sizes;
    const QList<int> splitterSizes = splitter->sizes();
    for (int size : splitterSizes) {
        sizes.append(size);
    }
    nodeObject.insert(QStringLiteral("sizes"), sizes);

    QJsonArray children;
    for (int i = 0; i < splitter->count(); ++i) {
        children.append(serializeNode(splitter->widget(i)));
    }
    nodeObject.insert(QStringLiteral("children"), children);

    return nodeObject;
}

QWidget* MainWindow::deserializeNode(const QJsonObject& nodeObject, bool* ok) {
    const QString type = nodeObject.value(QStringLiteral("type")).toString();
    if (type == QStringLiteral("pane")) {
        const QString paneId = normalizePaneId(nodeObject.value(QStringLiteral("id")).toString());
        const QString paneTitle = nodeObject.value(QStringLiteral("title")).toString();
        return createPane(paneTitle, paneId);
    }

    if (type == QStringLiteral("splitter")) {
        const QString orientationString = nodeObject.value(QStringLiteral("orientation")).toString();
        const Qt::Orientation orientation = orientationString == QStringLiteral("vertical") ? Qt::Vertical : Qt::Horizontal;

        auto* splitter = new QSplitter(orientation, this);
        splitter->setChildrenCollapsible(false);

        const QJsonArray children = nodeObject.value(QStringLiteral("children")).toArray();
        if (children.isEmpty()) {
            *ok = false;
            return splitter;
        }

        for (const QJsonValue& childValue : children) {
            if (!childValue.isObject()) {
                *ok = false;
                continue;
            }
            QWidget* childNode = deserializeNode(childValue.toObject(), ok);
            if (childNode) {
                childNode->setParent(splitter);
                splitter->addWidget(childNode);
            }
        }

        const QJsonArray sizesArray = nodeObject.value(QStringLiteral("sizes")).toArray();
        QList<int> sizes;
        for (const QJsonValue& value : sizesArray) {
            sizes.append(value.toInt(1));
        }
        if (sizes.size() == splitter->count()) {
            splitter->setSizes(sizes);
        }

        return splitter;
    }

    *ok = false;
    return nullptr;
}

void MainWindow::collectPanes(QWidget* node, QList<TerminalPane*>& outPanes) const {
    if (!node) {
        return;
    }

    if (auto* pane = qobject_cast<TerminalPane*>(node)) {
        outPanes.append(pane);
        return;
    }

    if (auto* splitter = qobject_cast<QSplitter*>(node)) {
        for (int i = 0; i < splitter->count(); ++i) {
            collectPanes(splitter->widget(i), outPanes);
        }
    }
}

void MainWindow::collectAllPanes(QList<TerminalPane*>& outPanes) const {
    outPanes.clear();
    for (auto it = m_tabInfos.constBegin(); it != m_tabInfos.constEnd(); ++it) {
        collectPanes(it.value().rootNode, outPanes);
    }
}

TerminalPane* MainWindow::findPaneById(const QString& paneId) const {
    QList<TerminalPane*> panes;
    collectAllPanes(panes);
    for (TerminalPane* pane : panes) {
        if (pane->paneId() == paneId) {
            return pane;
        }
    }
    return nullptr;
}

TerminalPane* MainWindow::findPaneByTitle(const QString& paneTitle) const {
    QList<TerminalPane*> panes;
    collectAllPanes(panes);
    for (TerminalPane* pane : panes) {
        if (pane->title() == paneTitle) {
            return pane;
        }
    }
    return nullptr;
}

QString MainWindow::nextPaneId() {
    while (m_usedPaneIds.contains(QString::number(m_nextPaneCounter))) {
        ++m_nextPaneCounter;
    }
    const QString id = QString::number(m_nextPaneCounter++);
    m_usedPaneIds.insert(id);
    return id;
}

QString MainWindow::normalizePaneId(const QString& desiredId) {
    if (!desiredId.isEmpty() && !m_usedPaneIds.contains(desiredId)) {
        m_usedPaneIds.insert(desiredId);

        bool parseOk = false;
        const int numericId = desiredId.toInt(&parseOk);
        if (parseOk && numericId >= m_nextPaneCounter) {
            m_nextPaneCounter = numericId + 1;
        }
        return desiredId;
    }

    return nextPaneId();
}

QString MainWindow::nextTabId() {
    while (m_usedTabIds.contains(QString::number(m_nextTabCounter))) {
        ++m_nextTabCounter;
    }
    const QString id = QString::number(m_nextTabCounter++);
    m_usedTabIds.insert(id);
    return id;
}

QString MainWindow::normalizeTabId(const QString& desiredId) {
    if (!desiredId.isEmpty() && !m_usedTabIds.contains(desiredId)) {
        m_usedTabIds.insert(desiredId);

        bool parseOk = false;
        const int numericId = desiredId.toInt(&parseOk);
        if (parseOk && numericId >= m_nextTabCounter) {
            m_nextTabCounter = numericId + 1;
        }
        return desiredId;
    }

    return nextTabId();
}

void MainWindow::setActivePane(TerminalPane* pane) {
    m_activePane = pane;

    QList<TerminalPane*> panes;
    collectAllPanes(panes);
    for (TerminalPane* p : panes) {
        p->setProperty("active", p == pane);
        p->style()->unpolish(p);
        p->style()->polish(p);
        p->update();
    }
}

void MainWindow::wirePaneSignals(TerminalPane* pane) {
    connect(pane, &TerminalPane::splitRequested, this, &MainWindow::splitPane);
    connect(pane, &TerminalPane::activated, this, &MainWindow::setActivePane);
    connect(pane, &TerminalPane::closeRequested, this, &MainWindow::closePane);
    connect(pane, &TerminalPane::renameRequested, this, &MainWindow::renamePane);
    connect(pane, &TerminalPane::copyRequested, this, [this](TerminalPane* sourcePane) {
        if (sourcePane) {
            setActivePane(sourcePane);
            sourcePane->terminalView()->copy();
        }
    });
    connect(pane, &TerminalPane::pasteRequested, this, [this](TerminalPane* sourcePane) {
        if (sourcePane) {
            setActivePane(sourcePane);
            sourcePane->terminalView()->paste();
        }
    });
    connect(pane, &TerminalPane::selectAllRequested, this, [this](TerminalPane* sourcePane) {
        if (sourcePane) {
            setActivePane(sourcePane);
            sourcePane->terminalView()->selectAll();
        }
    });
}

void MainWindow::syncActivePaneToCurrentTab() {
    const TabInfo* info = currentTabInfo();
    if (!info || !info->rootNode) {
        setActivePane(nullptr);
        return;
    }

    QList<TerminalPane*> panes;
    collectPanes(info->rootNode, panes);
    if (panes.isEmpty()) {
        setActivePane(nullptr);
        return;
    }

    if (m_activePane && panes.contains(m_activePane)) {
        setActivePane(m_activePane);
    } else {
        setActivePane(panes.first());
    }
}

void MainWindow::refreshTabVisual(int index) {
    if (index < 0 || index >= m_tabWidget->count()) {
        return;
    }
    QWidget* page = m_tabWidget->widget(index);
    const TabInfo* info = tabInfoForPage(page);
    if (!info) {
        return;
    }

    m_tabWidget->setTabText(index, info->title);
    m_tabWidget->setTabToolTip(index, QStringLiteral("Tab #") + info->id);
}
