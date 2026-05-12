#include "MainWindow.h"

#include "AppSettings.h"
#include "CommandServer.h"
#include "LayoutEditorDialog.h"
#include "LlmChatDialog.h"
#include "SnapSplitter.h"
#include "SettingsDialog.h"
#include "StartupScriptDialog.h"
#include "TerminalPane.h"
#include "TerminalView.h"
#include "ThemeManager.h"

#include <QApplication>
#include <QByteArray>
#include <QCloseEvent>
#include <QDesktopServices>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDir>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QInputDialog>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLayout>
#include <QLabel>
#include <QMenu>
#include <QMenuBar>
#include <QMessageBox>
#include <QPixmap>
#include <QStatusBar>
#include <QStandardPaths>
#include <QStyle>
#include <QTabBar>
#include <QTabWidget>
#include <QTimer>
#include <QToolButton>
#include <QVBoxLayout>
#include <cmath>
#include <QtGlobal>
#include <QUrl>

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent)
    , m_tabWidget(nullptr)
    , m_activePane(nullptr)
    , m_nextPaneCounter(1)
    , m_nextTabCounter(1)
    , m_settings(SettingsStore::load())
    , m_commandServer(new CommandServer(this))
    , m_askLlmAction(nullptr)
    , m_toggleMenuBarAction(nullptr)
    , m_llmChatDialog(nullptr)
    , m_newTabButton(nullptr)
    , m_broadcastSourcePane(nullptr)
    , m_broadcastRelaying(false)
    , m_lastBroadcastAllOverrideState(false) {
    initializeUi();
    createMenus();
    refreshLlmActionState();
    if (!tryRestoreAutoSavedLayout()) {
        createNewTab();
    }
    QTimer::singleShot(0, this, [this]() {
        focusActivePaneTerminal();
    });

    applyTheme(QString());

    connect(m_commandServer, &CommandServer::commandReceived, this, &MainWindow::handleRemoteCommand);
    if (!m_commandServer->startListening()) {
        QMessageBox::warning(this, QStringLiteral("IPC unavailable"), QStringLiteral("Another instance may already hold the command socket."));
    }
}

void MainWindow::closeEvent(QCloseEvent* event) {
    QList<TerminalPane*> panes;
    collectAllPanes(panes);

    const QList<TerminalPane*> runningPanes = busyPanes(panes);
    if (!runningPanes.isEmpty() && !confirmCloseBusyPanes(runningPanes, QStringLiteral("app"))) {
        event->ignore();
        return;
    }

    if (!m_settings.confirmOnMultiPaneExit) {
        saveAutoLayoutSnapshot();
        event->accept();
        return;
    }

    const bool hasMultipleTabs = m_tabWidget && m_tabWidget->count() > 1;
    const bool hasMultiplePanes = panes.size() > 1;
    if (!hasMultipleTabs && !hasMultiplePanes) {
        saveAutoLayoutSnapshot();
        event->accept();
        return;
    }

    const QMessageBox::StandardButton button = QMessageBox::question(
        this,
        QStringLiteral("Confirm Exit"),
        QStringLiteral("You have multiple tabs or panes open. Exit JTerm?"),
        QMessageBox::Yes | QMessageBox::No,
        QMessageBox::No);

    if (button == QMessageBox::Yes) {
        saveAutoLayoutSnapshot();
        event->accept();
    } else {
        event->ignore();
    }
}

QString MainWindow::autoSavedLayoutPath() const {
    QString baseDir = QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation);
    if (baseDir.trimmed().isEmpty()) {
        baseDir = QDir::homePath() + QStringLiteral("/.config");
    }

    QDir dir(baseDir);
    dir.mkpath(QStringLiteral("jterm"));
    return dir.filePath(QStringLiteral("jterm/layout-autosave.json"));
}

bool MainWindow::tryRestoreAutoSavedLayout() {
    if (!m_settings.autoSaveRestoreLayout) {
        return false;
    }

    QFile file(autoSavedLayoutPath());
    if (!file.exists() || !file.open(QIODevice::ReadOnly)) {
        return false;
    }

    const QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
    if (!doc.isObject()) {
        return false;
    }

    QString error;
    return importLayoutObject(doc.object(), &error);
}

void MainWindow::saveAutoLayoutSnapshot() {
    if (!m_settings.autoSaveRestoreLayout || m_tabWidget->count() <= 0) {
        return;
    }

    QFile file(autoSavedLayoutPath());
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        return;
    }

    file.write(QJsonDocument(exportLayoutObject()).toJson(QJsonDocument::Indented));
}

bool MainWindow::loadLayoutFromPath(const QString& path, QString* errorMessage) {
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("Could not open layout file: ") + path;
        }
        return false;
    }

    const QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
    if (!doc.isObject()) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("Invalid layout JSON in file: ") + path;
        }
        return false;
    }

    if (layoutContainsStartupScripts(doc.object()) && !confirmLoadLayoutWithStartupScripts()) {
        if (errorMessage) {
            errorMessage->clear();
        }
        return false;
    }

    return importLayoutObject(doc.object(), errorMessage);
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

void MainWindow::autoArrangeCurrentTabPanes() {
    QWidget* page = currentTabPage();
    TabInfo* info = tabInfoForPage(page);
    if (!page || !info || !info->rootNode) {
        return;
    }

    QList<TerminalPane*> panes;
    collectPanes(info->rootNode, panes);
    if (panes.size() <= 1) {
        statusBar()->showMessage(QStringLiteral("Auto-arrange needs at least two panes."), 2500);
        return;
    }

    for (TerminalPane* pane : panes) {
        pane->setParent(nullptr);
    }

    QWidget* oldRoot = info->rootNode;
    if (page->layout()) {
        page->layout()->removeWidget(oldRoot);
    }
    oldRoot->setParent(nullptr);
    oldRoot->deleteLater();

    const int count = panes.size();
    const int columns = qMax(1, static_cast<int>(std::ceil(std::sqrt(static_cast<double>(count)))));
    const int rows = qMax(1, static_cast<int>(std::ceil(static_cast<double>(count) / columns)));

    auto configureEqualSizes = [](QSplitter* splitter) {
        if (!splitter || splitter->count() <= 0) {
            return;
        }
        QList<int> sizes;
        sizes.reserve(splitter->count());
        for (int i = 0; i < splitter->count(); ++i) {
            splitter->setStretchFactor(i, 1);
            sizes.append(1000);
        }
        splitter->setSizes(sizes);
    };

    QWidget* newRoot = nullptr;
    int paneIndex = 0;
    if (rows == 1) {
        if (count == 2) {
            auto* rowSplitter = new SnapSplitter(Qt::Horizontal, page, page);
            rowSplitter->setChildrenCollapsible(false);
            rowSplitter->addWidget(panes[paneIndex++]);
            rowSplitter->addWidget(panes[paneIndex++]);
            configureEqualSizes(rowSplitter);
            newRoot = rowSplitter;
        } else {
            auto* rowSplitter = new SnapSplitter(Qt::Horizontal, page, page);
            rowSplitter->setChildrenCollapsible(false);
            while (paneIndex < count) {
                rowSplitter->addWidget(panes[paneIndex++]);
            }
            configureEqualSizes(rowSplitter);
            newRoot = rowSplitter;
        }
    } else {
        auto* topSplitter = new SnapSplitter(Qt::Vertical, page, page);
        topSplitter->setChildrenCollapsible(false);

        const int basePerRow = count / rows;
        const int extraRows = count % rows;
        for (int row = 0; row < rows; ++row) {
            const int rowCount = basePerRow + (row < extraRows ? 1 : 0);
            if (rowCount <= 0) {
                continue;
            }

            QWidget* rowNode = nullptr;
            if (rowCount == 1) {
                rowNode = panes[paneIndex++];
            } else {
                auto* rowSplitter = new SnapSplitter(Qt::Horizontal, page, page);
                rowSplitter->setChildrenCollapsible(false);
                for (int c = 0; c < rowCount && paneIndex < count; ++c) {
                    rowSplitter->addWidget(panes[paneIndex++]);
                }
                configureEqualSizes(rowSplitter);
                rowNode = rowSplitter;
            }
            if (rowNode) {
                topSplitter->addWidget(rowNode);
            }
        }

        configureEqualSizes(topSplitter);
        newRoot = topSplitter;
    }

    if (!newRoot) {
        return;
    }

    newRoot->setParent(page);
    page->layout()->addWidget(newRoot);
    info->rootNode = newRoot;
    applySnapScope(newRoot, page);

    setActivePane(panes.first());
    refreshMoveToTabButtonVisibility();
    applyBroadcastAllOverrideState();
    focusActivePaneTerminal();
    statusBar()->showMessage(QStringLiteral("Panes auto-arranged."), 2200);
}

void MainWindow::saveLayoutToFile() {
    if (m_tabWidget->count() == 0) {
        return;
    }

    const QString path = QFileDialog::getSaveFileName(this, QStringLiteral("Save Layout"), QStringLiteral("layout.json"), QStringLiteral("JSON (*.json)"));
    if (path.isEmpty()) {
        return;
    }

    const QJsonObject rootObject = exportLayoutObject();

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

    if (layoutContainsStartupScripts(doc.object()) && !confirmLoadLayoutWithStartupScripts()) {
        statusBar()->showMessage(QStringLiteral("Layout load canceled."), 2500);
        return;
    }

    QString error;
    if (!importLayoutObject(doc.object(), &error)) {
        QMessageBox::critical(this, QStringLiteral("Load failed"), error);
    }
}

void MainWindow::editLayoutJson() {
    const QJsonObject currentLayout = exportLayoutObject();
    LayoutEditorDialog dialog(currentLayout, this);
    if (dialog.exec() != QDialog::Accepted) {
        return;
    }

    QString error;
    if (!importLayoutObject(dialog.layoutObject(), &error)) {
        QMessageBox::critical(this, QStringLiteral("Apply layout failed"), error);
    }
}

void MainWindow::showSettingsDialog() {
    showSettingsDialogAtTab(SettingsDialog::InitialTab::General);
}

void MainWindow::showLlmSettingsDialog() {
    showSettingsDialogAtTab(SettingsDialog::InitialTab::Llm);
}

void MainWindow::showSettingsDialogAtTab(SettingsDialog::InitialTab initialTab) {
    SettingsDialog dialog(m_settings, this, initialTab);
    if (dialog.exec() != QDialog::Accepted) {
        return;
    }

    m_settings = dialog.settings();
    if (m_settings.terminalColorScheme.trimmed().isEmpty()) {
        m_settings.terminalColorScheme = QStringLiteral("WhiteOnBlack");
    }
    if (m_settings.defaultShell.isEmpty()) {
#ifdef Q_OS_WIN
        m_settings.defaultShell = QStringLiteral("powershell.exe");
#else
        m_settings.defaultShell = QStringLiteral("/bin/bash");
#endif
    }

    SettingsStore::save(m_settings);
    applyTheme(QString());
    refreshLlmActionState();
    applyBroadcastAllOverrideState();

    if (m_llmChatDialog) {
        m_llmChatDialog->setSettings(m_settings);
    }
}

void MainWindow::resetWindowLayout() {
    if (!m_activePane) {
        return;
    }

    QList<TerminalPane*> panes;
    collectAllPanes(panes);

    QList<TerminalPane*> closingPanes;
    for (TerminalPane* pane : panes) {
        if (pane != m_activePane) {
            closingPanes.append(pane);
        }
    }

    if (closingPanes.isEmpty()) {
        return;
    }

    QList<TerminalPane*> runningClosingPanes = busyPanes(closingPanes);
    if (!runningClosingPanes.isEmpty() && !confirmCloseBusyPanes(runningClosingPanes, QStringLiteral("window reset"))) {
        return;
    }

    const QMessageBox::StandardButton confirm = QMessageBox::warning(
        this,
        QStringLiteral("Reset Window"),
        QStringLiteral("Reset Window will close all panes and tabs except the active terminal. Continue?"),
        QMessageBox::Yes | QMessageBox::No,
        QMessageBox::No);
    if (confirm != QMessageBox::Yes) {
        return;
    }

    m_activePane->setParent(nullptr);

    while (m_tabWidget->count() > 0) {
        QWidget* page = m_tabWidget->widget(0);
        m_tabInfos.remove(page);
        m_tabWidget->removeTab(0);
        page->deleteLater();
    }

    QWidget* page = createTabPage(QStringLiteral("1"), QStringLiteral("Tab 1"), m_activePane);
    m_tabWidget->setCurrentWidget(page);

    m_usedPaneIds.clear();
    m_usedTabIds.clear();
    m_nextPaneCounter = 1;
    m_nextTabCounter = 1;
    m_usedTabIds.insert(QStringLiteral("1"));
    m_nextTabCounter = 2;
    const QString activePaneId = m_activePane->paneId();
    m_usedPaneIds.insert(activePaneId);

    clearBroadcastState();
    setActivePane(m_activePane);
    syncActivePaneToCurrentTab();
    refreshMoveToTabButtonVisibility();
    applyBroadcastAllOverrideState();
    focusActivePaneTerminal();
}

void MainWindow::showLlmChatDialog() {
    if (!m_settings.hasLlmConfiguration()) {
        QMessageBox::information(this, QStringLiteral("LLM Not Configured"), QStringLiteral("Open Settings -> LLM and configure provider, base URL, and model first."));
        return;
    }

    if (!m_llmChatDialog) {
        m_llmChatDialog = new LlmChatDialog(m_settings, this);
        m_llmChatDialog->setAttribute(Qt::WA_DeleteOnClose, true);
        connect(m_llmChatDialog, &QObject::destroyed, this, [this]() {
            m_llmChatDialog = nullptr;
        });
    } else {
        m_llmChatDialog->setSettings(m_settings);
    }

    m_llmChatDialog->show();
    m_llmChatDialog->raise();
    m_llmChatDialog->activateWindow();
}

void MainWindow::focusActivePaneTerminal() {
    if (!m_activePane || !m_activePane->terminalView()) {
        return;
    }
    m_activePane->terminalView()->focusTerminal();
}

void MainWindow::openProjectGithubPage() {
    QDesktopServices::openUrl(QUrl(QStringLiteral("https://github.com/jeddhor/JTerm")));
}

void MainWindow::showAboutDialog() {
    QDialog dialog(this);
    dialog.setWindowTitle(QStringLiteral("About JTerm"));
    dialog.resize(820, 980);

    auto* rootLayout = new QVBoxLayout(&dialog);
    rootLayout->setContentsMargins(16, 16, 16, 16);
    rootLayout->setSpacing(12);

    QString logoPath = QCoreApplication::applicationDirPath() + QStringLiteral("/assets/JTerm_logo.png");
    if (!QFileInfo::exists(logoPath)) {
        logoPath = QCoreApplication::applicationDirPath() + QStringLiteral("/JTerm_logo.png");
    }
    if (!QFileInfo::exists(logoPath)) {
        logoPath = QStringLiteral("assets/JTerm_logo.png");
    }

    auto* logoLabel = new QLabel(&dialog);
    logoLabel->setAlignment(Qt::AlignCenter);

    QPixmap logo(logoPath);
    if (!logo.isNull()) {
        logoLabel->setPixmap(logo.scaled(760, 760, Qt::KeepAspectRatio, Qt::SmoothTransformation));
    } else {
        logoLabel->setText(QStringLiteral("JTerm"));
    }

    auto* textLabel = new QLabel(
        QStringLiteral("<h2>JTerm</h2>"
                       "<p>JTerm (Jason's Terminal) is a Qt6 split-pane and tabbed terminal emulator with layout JSON editing, startup scripts, and pane-targeted command routing.</p>"
                       "<p><b>License:</b> MIT License (see LICENSE)</p>"),
        &dialog);
    textLabel->setAlignment(Qt::AlignHCenter | Qt::AlignTop);
    textLabel->setWordWrap(true);
    textLabel->setTextFormat(Qt::RichText);

    auto* buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok, &dialog);
    connect(buttonBox, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);

    rootLayout->addWidget(logoLabel, 0, Qt::AlignHCenter);
    rootLayout->addWidget(textLabel, 0, Qt::AlignHCenter);
    rootLayout->addStretch(1);
    rootLayout->addWidget(buttonBox);

    dialog.exec();
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
        view->setTerminalColorScheme(m_settings.terminalColorScheme);
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
    setWindowTitle(QStringLiteral("JTerm"));
    setWindowIcon(qApp->windowIcon());
    resize(1500, 900);

    auto* host = new QWidget(this);
    auto* hostLayout = new QVBoxLayout(host);
    hostLayout->setContentsMargins(8, 8, 8, 8);
    hostLayout->setSpacing(6);

    m_tabWidget = new QTabWidget(host);
    m_tabWidget->setDocumentMode(true);
    m_tabWidget->setMovable(true);
    m_tabWidget->setTabsClosable(true);

    m_newTabButton = new QToolButton(m_tabWidget);
    m_newTabButton->setText(QStringLiteral("+"));
    m_newTabButton->setAutoRaise(true);
    m_newTabButton->setToolTip(QStringLiteral("Create new tab"));
    m_newTabButton->setCursor(Qt::PointingHandCursor);
    connect(m_newTabButton, &QToolButton::clicked, this, &MainWindow::createNewTab);
    m_tabWidget->setCornerWidget(m_newTabButton, Qt::TopRightCorner);

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
    fileMenu->addAction(QStringLiteral("Edit Layout..."), QKeySequence(QStringLiteral("Ctrl+Shift+E")), this, &MainWindow::editLayoutJson);
    fileMenu->addSeparator();
    fileMenu->addAction(QStringLiteral("Save Layout..."), QKeySequence(QStringLiteral("Ctrl+S")), this, &MainWindow::saveLayoutToFile);
    fileMenu->addAction(QStringLiteral("Load Layout..."), QKeySequence(QStringLiteral("Ctrl+O")), this, &MainWindow::loadLayoutFromFile);
    fileMenu->addSeparator();
    fileMenu->addAction(QStringLiteral("Exit"), QKeySequence(QStringLiteral("Ctrl+Q")), this, &QWidget::close);

    auto* editMenu = menuBar()->addMenu(QStringLiteral("&Edit"));
    editMenu->addAction(QStringLiteral("⎘ Copy"), QKeySequence::Copy, this, [this]() {
        if (m_activePane) {
            m_activePane->terminalView()->copy();
        }
    });
    editMenu->addAction(QStringLiteral("📋 Paste"), QKeySequence::Paste, this, [this]() {
        if (m_activePane) {
            m_activePane->terminalView()->paste();
        }
    });
    editMenu->addAction(QStringLiteral("Select All"), QKeySequence::SelectAll, this, [this]() {
        if (m_activePane) {
            m_activePane->terminalView()->selectAll();
        }
    });
    editMenu->addSeparator();
    editMenu->addAction(QStringLiteral("Preferences..."), QKeySequence(QStringLiteral("Ctrl+,")), this, &MainWindow::showSettingsDialog);

    auto* windowMenu = menuBar()->addMenu(QStringLiteral("&Window"));
    auto* windowPaneMenu = windowMenu->addMenu(QStringLiteral("Pane"));
    auto* windowTabMenu = windowMenu->addMenu(QStringLiteral("Tab"));

    windowPaneMenu->addAction(QStringLiteral("Split Horizontally"), QKeySequence(QStringLiteral("Ctrl+Shift+H")), this, &MainWindow::splitActivePaneHorizontal);
    windowPaneMenu->addAction(QStringLiteral("Split Vertically"), QKeySequence(QStringLiteral("Ctrl+Shift+V")), this, &MainWindow::splitActivePaneVertical);
    windowPaneMenu->addAction(QStringLiteral("Rename Pane..."), QKeySequence(QStringLiteral("Ctrl+Shift+R")), this, &MainWindow::renameActivePane);
    windowPaneMenu->addAction(QStringLiteral("Close Terminal"), QKeySequence(QStringLiteral("Ctrl+Shift+W")), this, &MainWindow::closeActivePane);

    windowTabMenu->addAction(QStringLiteral("New Tab"), QKeySequence(QStringLiteral("Ctrl+T")), this, &MainWindow::createNewTab);
    windowTabMenu->addAction(QStringLiteral("Rename Tab..."), QKeySequence(QStringLiteral("Ctrl+Alt+R")), this, &MainWindow::renameCurrentTab);
    windowTabMenu->addAction(QStringLiteral("Close Tab"), QKeySequence(QStringLiteral("Ctrl+W")), this, &MainWindow::closeCurrentTab);
    windowTabMenu->addAction(QStringLiteral("Auto-Arrange Panes"), QKeySequence(QStringLiteral("Ctrl+Shift+A")), this, &MainWindow::autoArrangeCurrentTabPanes);
    windowMenu->addSeparator();
    windowMenu->addAction(QStringLiteral("Reset Window"), this, &MainWindow::resetWindowLayout);
    windowMenu->addSeparator();
    m_toggleMenuBarAction = windowMenu->addAction(QStringLiteral("Show Menu Bar"));
    m_toggleMenuBarAction->setCheckable(true);
    m_toggleMenuBarAction->setChecked(true);
    m_toggleMenuBarAction->setShortcut(QKeySequence(QStringLiteral("Ctrl+Shift+M")));
    connect(m_toggleMenuBarAction, &QAction::toggled, this, &MainWindow::setMenuBarVisible);
    addAction(m_toggleMenuBarAction);

    auto* llmMenu = menuBar()->addMenu(QStringLiteral("&LLM"));
    m_askLlmAction = llmMenu->addAction(QStringLiteral("Ask the LLM..."), this, &MainWindow::showLlmChatDialog);
    m_askLlmAction->setShortcut(QKeySequence(QStringLiteral("Ctrl+Shift+L")));
    m_askLlmAction->setEnabled(false);
    m_askLlmAction->setToolTip(QStringLiteral("Configure LLM settings first."));
    llmMenu->addSeparator();
    llmMenu->addAction(QStringLiteral("LLM Settings..."), this, &MainWindow::showLlmSettingsDialog);

    auto* helpMenu = menuBar()->addMenu(QStringLiteral("&Help"));
    helpMenu->addAction(QStringLiteral("JTerm on GitHub"), this, &MainWindow::openProjectGithubPage);
    helpMenu->addSeparator();
    helpMenu->addAction(QStringLiteral("About JTerm"), this, &MainWindow::showAboutDialog);
}

void MainWindow::setMenuBarVisible(bool visible) {
    if (menuBar()) {
        menuBar()->setVisible(visible);
    }

    if (centralWidget()) {
        centralWidget()->updateGeometry();
        centralWidget()->adjustSize();
    }
    if (m_tabWidget) {
        m_tabWidget->updateGeometry();
        m_tabWidget->update();
    }

    statusBar()->showMessage(
        visible
            ? QStringLiteral("Menu bar shown.")
            : QStringLiteral("Menu bar hidden. Use Ctrl+Shift+M to show it again."),
        2500);
}

void MainWindow::refreshLlmActionState() {
    if (!m_askLlmAction) {
        return;
    }

    const bool enabled = m_settings.hasLlmConfiguration();
    m_askLlmAction->setEnabled(enabled);
    if (enabled) {
        m_askLlmAction->setToolTip(QStringLiteral("Open the modeless LLM chat helper."));
    } else {
        m_askLlmAction->setToolTip(QStringLiteral("Disabled until LLM settings are configured and saved."));
    }
}

void MainWindow::refreshMoveToTabButtonVisibility() {
    for (auto it = m_tabInfos.begin(); it != m_tabInfos.end(); ++it) {
        QList<TerminalPane*> panes;
        collectPanes(it.value().rootNode, panes);
        const bool showMoveControl = panes.size() > 1;
        for (TerminalPane* pane : panes) {
            pane->setMoveToTabVisible(showMoveControl);
        }
    }
}

void MainWindow::toggleBroadcastSource(TerminalPane* pane) {
    if (!pane) {
        return;
    }

    TerminalPane* next = pane;
    if (m_broadcastSourcePane == pane) {
        next = nullptr;
    }

    QList<TerminalPane*> panes;
    collectAllPanes(panes);
    for (TerminalPane* p : panes) {
        p->setBroadcastSourceSelected(p == next);
    }
    m_broadcastSourcePane = next;
}

void MainWindow::applyBroadcastAllOverrideState() {
    QList<TerminalPane*> panes;
    collectAllPanes(panes);

    if (m_settings.broadcastAllOverride) {
        for (TerminalPane* pane : panes) {
            pane->setBroadcastTargetChecked(true);
            pane->setBroadcastTargetEnabled(false);
        }
        m_lastBroadcastAllOverrideState = true;
        return;
    }

    const bool wasOverrideEnabled = m_lastBroadcastAllOverrideState;
    for (TerminalPane* pane : panes) {
        pane->setBroadcastTargetEnabled(true);
        if (wasOverrideEnabled) {
            pane->setBroadcastTargetChecked(false);
        }
    }
    m_lastBroadcastAllOverrideState = false;
}

void MainWindow::clearBroadcastState() {
    QList<TerminalPane*> panes;
    collectAllPanes(panes);
    for (TerminalPane* pane : panes) {
        pane->setBroadcastSourceSelected(false);
    }
    m_broadcastSourcePane = nullptr;
}

void MainWindow::relayBroadcastKeyPress(TerminalPane* sourcePane, int key, int modifiers, const QString& text) {
    if (!sourcePane || m_broadcastRelaying || sourcePane != m_broadcastSourcePane) {
        return;
    }

    QList<TerminalPane*> panes;
    collectAllPanes(panes);

    const Qt::KeyboardModifiers keyMods = static_cast<Qt::KeyboardModifiers>(modifiers);
    m_broadcastRelaying = true;
    for (TerminalPane* pane : panes) {
        if (pane == sourcePane) {
            continue;
        }
        if (!m_settings.broadcastAllOverride && !pane->isBroadcastTargetChecked()) {
            continue;
        }
        pane->terminalView()->sendKeyPress(key, keyMods, text);
    }
    m_broadcastRelaying = false;
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
    applySnapScope(rootNode, page);

    TabInfo info;
    info.id = tabId;
    info.title = tabTitle.trimmed().isEmpty() ? (QStringLiteral("Tab ") + tabId) : tabTitle.trimmed();
    info.rootNode = rootNode;
    m_tabInfos.insert(page, info);

    const int index = m_tabWidget->addTab(page, info.title);
    refreshTabVisual(index);
    refreshMoveToTabButtonVisibility();
    applyBroadcastAllOverrideState();
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

    TerminalView* view = pane->terminalView();
    if (view) {
        view->setTerminalColorScheme(m_settings.terminalColorScheme);
        const TerminalColors colors = ThemeManager::terminalColorsForTheme(QString());
        view->setTerminalColors(colors.foreground, colors.background);
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
        oldNode->setParent(nullptr);
        newNode->setParent(parentSplitter);
        parentSplitter->insertWidget(index, newNode);
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

    if (auto* parentSplitter = qobject_cast<QSplitter*>(parentNode)) {
        if (parentSplitter->orientation() == orientation) {
            const int index = parentSplitter->indexOf(pane);
            if (index < 0) {
                return;
            }

            auto* sibling = createPane();
            parentSplitter->insertWidget(index + 1, sibling);
            parentSplitter->setChildrenCollapsible(false);
            parentSplitter->setStretchFactor(index, 1);
            parentSplitter->setStretchFactor(index + 1, 1);

            QList<int> sizes = parentSplitter->sizes();
            if (index >= 0 && index + 1 < sizes.size()) {
                const int combined = qMax(160, sizes[index] + sizes[index + 1]);
                const int first = qMax(80, combined / 2);
                const int second = qMax(80, combined - first);
                sizes[index] = first;
                sizes[index + 1] = second;
                parentSplitter->setSizes(sizes);
            }
            setActivePane(sibling);
            refreshMoveToTabButtonVisibility();
            return;
        }

        const int index = parentSplitter->indexOf(pane);
        if (index < 0) {
            return;
        }

        const QList<int> parentSizesBefore = parentSplitter->sizes();
        const int targetSize = (index >= 0 && index < parentSizesBefore.size()) ? parentSizesBefore[index] : 400;

        auto* newSplitter = new SnapSplitter(orientation, tabPage, parentSplitter);
        newSplitter->setChildrenCollapsible(false);
        parentSplitter->insertWidget(index, newSplitter);

        auto* sibling = createPane();

        pane->setParent(newSplitter);
        sibling->setParent(newSplitter);
        newSplitter->addWidget(pane);
        newSplitter->addWidget(sibling);
        newSplitter->setStretchFactor(0, 1);
        newSplitter->setStretchFactor(1, 1);

        int allocated = 1000;
        if (index >= 0 && index < parentSizesBefore.size()) {
            allocated = qMax(200, parentSizesBefore[index]);
        }
        const int first = qMax(100, allocated / 2);
        const int second = qMax(100, allocated - first);
        newSplitter->setSizes({first, second});

        QList<int> parentSizesAfter = parentSplitter->sizes();
        if (index >= 0 && index < parentSizesAfter.size()) {
            parentSizesAfter[index] = qMax(200, targetSize);
            parentSplitter->setSizes(parentSizesAfter);
        }

        pane->show();
        sibling->show();
        newSplitter->show();
        parentSplitter->updateGeometry();
        newSplitter->updateGeometry();

        setActivePane(sibling);
        refreshMoveToTabButtonVisibility();
        return;
    }

    if (parentNode == tabPage) {
        auto* sibling = createPane();
        auto* newSplitter = new SnapSplitter(orientation, tabPage, tabPage);
        newSplitter->setChildrenCollapsible(false);

        auto* layout = tabPage->layout();
        layout->replaceWidget(pane, newSplitter);

        pane->setParent(newSplitter);
        sibling->setParent(newSplitter);
        newSplitter->addWidget(pane);
        newSplitter->addWidget(sibling);
        newSplitter->setStretchFactor(0, 1);
        newSplitter->setStretchFactor(1, 1);
        newSplitter->setSizes({500, 500});

        TabInfo* info = tabInfoForPage(tabPage);
        if (info) {
            info->rootNode = newSplitter;
        }

        setActivePane(sibling);
        refreshMoveToTabButtonVisibility();
        return;
    }
}

void MainWindow::closePane(TerminalPane* pane) {
        if (pane == m_broadcastSourcePane) {
            toggleBroadcastSource(pane);
        }

    if (!pane) {
        return;
    }

    if (pane->hasRunningProcess() && !confirmCloseBusyPanes({pane}, QStringLiteral("pane"))) {
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
    refreshMoveToTabButtonVisibility();
    applyBroadcastAllOverrideState();
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
    refreshMoveToTabButtonVisibility();
    applyBroadcastAllOverrideState();
}

void MainWindow::movePaneToNewTab(TerminalPane* pane) {
    if (!pane) {
        return;
    }

    QWidget* sourcePage = nullptr;
    for (int i = 0; i < m_tabWidget->count(); ++i) {
        QWidget* candidate = m_tabWidget->widget(i);
        if (candidate == pane || candidate->isAncestorOf(pane)) {
            sourcePage = candidate;
            break;
        }
    }
    if (!sourcePage) {
        return;
    }

    TabInfo* sourceInfo = tabInfoForPage(sourcePage);
    if (!sourceInfo || !sourceInfo->rootNode) {
        return;
    }

    if (pane == sourceInfo->rootNode) {
        sourceInfo->rootNode = nullptr;
        pane->setParent(nullptr);
    } else {
        auto* parentSplitter = qobject_cast<QSplitter*>(pane->parentWidget());
        if (!parentSplitter) {
            return;
        }

        if (parentSplitter->count() > 2) {
            pane->setParent(nullptr);
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

            if (auto* grandSplitter = qobject_cast<QSplitter*>(grandParent)) {
                const int index = grandSplitter->indexOf(parentSplitter);
                survivor->setParent(grandSplitter);
                grandSplitter->insertWidget(index, survivor);
                parentSplitter->deleteLater();
            } else if (grandParent == sourcePage) {
                sourcePage->layout()->removeWidget(parentSplitter);
                survivor->setParent(sourcePage);
                sourcePage->layout()->addWidget(survivor);
                sourceInfo->rootNode = survivor;
                parentSplitter->deleteLater();
            }
        }
    }

    const QString newTabId = nextTabId();
    const QString newTabTitle = pane->title().trimmed().isEmpty() ? (QStringLiteral("Tab ") + newTabId) : pane->title().trimmed();
    QWidget* newPage = createTabPage(newTabId, newTabTitle, pane);
    m_tabWidget->setCurrentWidget(newPage);

    if (!sourceInfo->rootNode) {
        if (m_activePane && (sourcePage == m_activePane || sourcePage->isAncestorOf(m_activePane))) {
            m_activePane = nullptr;
        }
        const int sourceIndex = m_tabWidget->indexOf(sourcePage);
        m_tabInfos.remove(sourcePage);
        if (sourceIndex >= 0) {
            m_tabWidget->removeTab(sourceIndex);
        }
        sourcePage->deleteLater();
    }

    setActivePane(pane);
}

void MainWindow::editPaneStartupScript(TerminalPane* pane) {
    if (!pane) {
        return;
    }

    StartupScriptDialog dialog(pane->title(), pane->startupScript(), this);
    if (dialog.exec() != QDialog::Accepted) {
        return;
    }

    pane->setStartupScript(dialog.script());
    setActivePane(pane);
    statusBar()->showMessage(QStringLiteral("Startup script saved to current layout state. Choose file destination in Save Layout dialog."), 3500);
    saveLayoutToFile();
}

void MainWindow::closeTabByIndex(int index) {
    if (index < 0 || index >= m_tabWidget->count()) {
        return;
    }

    QWidget* page = m_tabWidget->widget(index);
    if (!page) {
        return;
    }

    if (TabInfo* info = tabInfoForPage(page)) {
        QList<TerminalPane*> tabPanes;
        collectPanes(info->rootNode, tabPanes);
        const QList<TerminalPane*> runningPanes = busyPanes(tabPanes);
        if (!runningPanes.isEmpty() && !confirmCloseBusyPanes(runningPanes, QStringLiteral("tab"))) {
            return;
        }
    }

    if (m_tabWidget->count() <= 1) {
        close();
        return;
    }

    if (m_activePane && (page == m_activePane || page->isAncestorOf(m_activePane))) {
        m_activePane = nullptr;
    }
    if (m_broadcastSourcePane && (page == m_broadcastSourcePane || page->isAncestorOf(m_broadcastSourcePane))) {
        clearBroadcastState();
    }

    m_tabInfos.remove(page);
    m_tabWidget->removeTab(index);
    page->deleteLater();

    if (m_tabWidget->count() == 0) {
        createNewTab();
    }
    syncActivePaneToCurrentTab();
    refreshMoveToTabButtonVisibility();
    applyBroadcastAllOverrideState();
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

QJsonObject MainWindow::exportLayoutObject() const {
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
    if (m_activePane) {
        rootObject.insert(QStringLiteral("currentPaneId"), m_activePane->paneId());
    }

    return rootObject;
}

bool MainWindow::importLayoutObject(const QJsonObject& rootObject, QString* errorMessage) {
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
    clearBroadcastState();

    const int version = rootObject.value(QStringLiteral("version")).toInt(1);
    if (version <= 1) {
        bool ok = true;
        QWidget* rootNode = deserializeNode(rootObject.value(QStringLiteral("root")).toObject(), &ok);
        if (!ok || !rootNode) {
            if (errorMessage) {
                *errorMessage = QStringLiteral("Could not deserialize legacy layout root node.");
            }
            createNewTab();
            return false;
        }
        createTabPage(nextTabId(), QStringLiteral("Tab 1"), rootNode);
        m_tabWidget->setCurrentIndex(0);
        syncActivePaneToCurrentTab();
        applyTheme(QString());
        refreshMoveToTabButtonVisibility();
        applyBroadcastAllOverrideState();
        return true;
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
        if (errorMessage) {
            *errorMessage = QStringLiteral("Layout contains no valid tabs.");
        }
        createNewTab();
        return false;
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

    const QString currentPaneId = rootObject.value(QStringLiteral("currentPaneId")).toString().trimmed();
    if (!currentPaneId.isEmpty()) {
        if (TerminalPane* focusPane = findPaneById(currentPaneId)) {
            for (int i = 0; i < m_tabWidget->count(); ++i) {
                QWidget* page = m_tabWidget->widget(i);
                if (page == focusPane || page->isAncestorOf(focusPane)) {
                    m_tabWidget->setCurrentIndex(i);
                    break;
                }
            }
            setActivePane(focusPane);
            focusPane->terminalView()->focusTerminal();
            applyTheme(QString());
            refreshMoveToTabButtonVisibility();
            applyBroadcastAllOverrideState();
            return true;
        }
    }

    syncActivePaneToCurrentTab();
    applyTheme(QString());
    refreshMoveToTabButtonVisibility();
    applyBroadcastAllOverrideState();
    focusActivePaneTerminal();
    return true;
}

QJsonObject MainWindow::serializeNode(QWidget* node) const {
    QJsonObject nodeObject;

    if (auto* pane = qobject_cast<TerminalPane*>(node)) {
        nodeObject.insert(QStringLiteral("type"), QStringLiteral("pane"));
        nodeObject.insert(QStringLiteral("id"), pane->paneId());
        nodeObject.insert(QStringLiteral("title"), pane->title());
        const QString startupScript = pane->startupScript();
        if (!startupScript.isEmpty()) {
            const QByteArray encoded = startupScript.toUtf8().toBase64();
            nodeObject.insert(QStringLiteral("startupScriptBase64"), QString::fromLatin1(encoded));
        }
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
        TerminalPane* pane = createPane(paneTitle, paneId);

        const QString startupEncoded = nodeObject.value(QStringLiteral("startupScriptBase64")).toString();
        if (!startupEncoded.isEmpty()) {
            const QByteArray decoded = QByteArray::fromBase64(startupEncoded.toLatin1());
            if (!decoded.isEmpty()) {
                pane->setStartupScript(QString::fromUtf8(decoded));
                pane->runStartupScript();
            }
        }

        return pane;
    }

    if (type == QStringLiteral("splitter")) {
        const QString orientationString = nodeObject.value(QStringLiteral("orientation")).toString();
        const Qt::Orientation orientation = orientationString == QStringLiteral("vertical") ? Qt::Vertical : Qt::Horizontal;

        auto* splitter = new SnapSplitter(orientation, nullptr, this);
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

bool MainWindow::nodeContainsStartupScripts(const QJsonObject& nodeObject) const {
    const QString encoded = nodeObject.value(QStringLiteral("startupScriptBase64")).toString();
    if (!encoded.trimmed().isEmpty()) {
        return true;
    }

    const QJsonArray children = nodeObject.value(QStringLiteral("children")).toArray();
    for (const QJsonValue& childValue : children) {
        if (childValue.isObject() && nodeContainsStartupScripts(childValue.toObject())) {
            return true;
        }
    }

    return false;
}

bool MainWindow::layoutContainsStartupScripts(const QJsonObject& layoutObject) const {
    if (nodeContainsStartupScripts(layoutObject.value(QStringLiteral("root")).toObject())) {
        return true;
    }

    const QJsonArray tabsArray = layoutObject.value(QStringLiteral("tabs")).toArray();
    for (const QJsonValue& tabValue : tabsArray) {
        if (!tabValue.isObject()) {
            continue;
        }
        if (nodeContainsStartupScripts(tabValue.toObject().value(QStringLiteral("root")).toObject())) {
            return true;
        }
    }

    return false;
}

bool MainWindow::confirmLoadLayoutWithStartupScripts() const {
    if (!m_settings.warnOnLayoutStartupScripts) {
        return true;
    }

    const QMessageBox::StandardButton choice = QMessageBox::warning(
        const_cast<MainWindow*>(this),
        QStringLiteral("\u26A0 Startup Commands In Layout"),
        QStringLiteral("\u26A0 This layout contains startup commands that will execute automatically when loaded.\n\nOnly continue if you trust the layout file source.\n\nLoad this layout anyway?"),
        QMessageBox::Yes | QMessageBox::No,
        QMessageBox::No);

    return choice == QMessageBox::Yes;
}

void MainWindow::applySnapScope(QWidget* node, QWidget* snapScope) {
    if (!node) {
        return;
    }

    if (auto* snapSplitter = qobject_cast<SnapSplitter*>(node)) {
        snapSplitter->setSnapScope(snapScope);
    }

    if (auto* splitter = qobject_cast<QSplitter*>(node)) {
        for (int i = 0; i < splitter->count(); ++i) {
            applySnapScope(splitter->widget(i), snapScope);
        }
    }
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

QList<TerminalPane*> MainWindow::busyPanes(const QList<TerminalPane*>& panes) const {
    QList<TerminalPane*> running;
    for (TerminalPane* pane : panes) {
        if (pane && pane->hasRunningProcess()) {
            running.append(pane);
        }
    }
    return running;
}

bool MainWindow::confirmCloseBusyPanes(const QList<TerminalPane*>& panes, const QString& scopeName) const {
    if (panes.isEmpty()) {
        return true;
    }

    QStringList labels;
    labels.reserve(panes.size());
    for (TerminalPane* pane : panes) {
        labels.append(QStringLiteral("#") + pane->paneId() + QStringLiteral(" - ") + pane->title());
    }

    QString details;
    constexpr int kMaxShown = 6;
    for (int i = 0; i < labels.size() && i < kMaxShown; ++i) {
        details += QStringLiteral("\n - ") + labels.at(i);
    }
    if (labels.size() > kMaxShown) {
        details += QStringLiteral("\n - ... and ") + QString::number(labels.size() - kMaxShown) + QStringLiteral(" more");
    }

    QString targetText = QStringLiteral("this ") + scopeName;
    if (scopeName == QStringLiteral("app")) {
        targetText = QStringLiteral("JTerm");
    }

    const QMessageBox::StandardButton button = QMessageBox::warning(
        const_cast<MainWindow*>(this),
        QStringLiteral("Running Process Detected"),
        QStringLiteral("One or more terminals appear busy in ") + targetText + QStringLiteral(":") + details
            + QStringLiteral("\n\nClosing may terminate running commands. Close anyway?"),
        QMessageBox::Yes | QMessageBox::No,
        QMessageBox::No);

    return button == QMessageBox::Yes;
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
    connect(pane, &TerminalPane::renameTabRequested, this, [this](TerminalPane* sourcePane) {
        if (sourcePane) {
            setActivePane(sourcePane);
        }
        renameCurrentTab();
    });
    connect(pane, &TerminalPane::moveToNewTabRequested, this, &MainWindow::movePaneToNewTab);
    connect(pane, &TerminalPane::broadcastSourceToggleRequested, this, &MainWindow::toggleBroadcastSource);
    connect(pane, &TerminalPane::broadcastTargetToggled, this, [this](TerminalPane* sourcePane, bool checked) {
        if (!sourcePane || !checked || m_settings.broadcastAllOverride) {
            return;
        }
    });
    connect(pane, &TerminalPane::startupScriptRequested, this, &MainWindow::editPaneStartupScript);
    connect(pane, &TerminalPane::preferencesRequested, this, [this](TerminalPane* sourcePane) {
        if (sourcePane) {
            setActivePane(sourcePane);
        }
        showSettingsDialog();
    });
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
    connect(pane->terminalView(), &TerminalView::keyPressed, this, [this, pane](int key, int modifiers, const QString& text) {
        relayBroadcastKeyPress(pane, key, modifiers, text);
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
