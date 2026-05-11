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
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLayout>
#include <QMenu>
#include <QMenuBar>
#include <QMessageBox>
#include <QStatusBar>
#include <QSplitter>
#include <QVBoxLayout>
#include <QtGlobal>

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent)
    , m_rootNode(nullptr)
    , m_activePane(nullptr)
    , m_nextPaneCounter(1)
    , m_settings(SettingsStore::load())
    , m_commandServer(new CommandServer(this)) {
    initializeUi();
    createMenus();
    createInitialPane();

    applyTheme(m_settings.themeName);

    connect(m_commandServer, &CommandServer::commandReceived, this, &MainWindow::handleRemoteCommand);
    if (!m_commandServer->startListening()) {
        QMessageBox::warning(this, QStringLiteral("IPC unavailable"), QStringLiteral("Another instance may already hold the command socket."));
    }
}

void MainWindow::splitActivePaneHorizontal() {
    if (!m_activePane) {
        return;
    }
    splitPane(m_activePane, Qt::Horizontal);
}

void MainWindow::splitActivePaneVertical() {
    if (!m_activePane) {
        return;
    }
    splitPane(m_activePane, Qt::Vertical);
}

void MainWindow::saveLayoutToFile() {
    if (!m_rootNode) {
        return;
    }

    const QString path = QFileDialog::getSaveFileName(this, QStringLiteral("Save Layout"), QStringLiteral("layout.json"), QStringLiteral("JSON (*.json)"));
    if (path.isEmpty()) {
        return;
    }

    QJsonObject rootObject;
    rootObject.insert(QStringLiteral("version"), 1);
    rootObject.insert(QStringLiteral("root"), serializeNode(m_rootNode));

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
    const QJsonObject rootNodeObject = rootObject.value(QStringLiteral("root")).toObject();
    if (rootNodeObject.isEmpty()) {
        QMessageBox::critical(this, QStringLiteral("Load failed"), QStringLiteral("Layout file missing root node."));
        return;
    }

    bool ok = true;
    m_usedPaneIds.clear();
    m_nextPaneCounter = 1;
    QWidget* loadedNode = deserializeNode(rootNodeObject, &ok);
    if (!ok || !loadedNode) {
        QMessageBox::critical(this, QStringLiteral("Load failed"), QStringLiteral("Could not deserialize layout."));
        if (loadedNode) {
            loadedNode->deleteLater();
        }
        return;
    }

    if (m_rootNode) {
        m_rootNode->deleteLater();
    }

    m_rootNode = loadedNode;
    centralWidget()->layout()->addWidget(m_rootNode);

    QList<TerminalPane*> panes;
    collectPanes(m_rootNode, panes);
    if (!panes.isEmpty()) {
        setActivePane(panes.first());
    }
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
    applyTheme(m_settings.themeName);
}

void MainWindow::applyTheme(const QString& themeName) {
    m_settings.themeName = themeName;
    ThemeManager::applyTheme(*qApp, themeName);

    const TerminalColors colors = ThemeManager::terminalColorsForTheme(themeName);
    QList<TerminalPane*> panes;
    if (m_rootNode) {
        collectPanes(m_rootNode, panes);
    }

    for (TerminalPane* pane : panes) {
        TerminalView* view = pane->terminalView();
        if (!view) {
            continue;
        }
        view->setTerminalColors(colors.foreground, colors.background);
    }
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
    setActivePane(target);
    statusBar()->showMessage(QStringLiteral("Remote command sent to pane ") + target->paneId(), 2500);
}

void MainWindow::initializeUi() {
    setWindowTitle(QStringLiteral("SplitTerm"));
    resize(1400, 900);

    auto* host = new QWidget(this);
    auto* hostLayout = new QVBoxLayout(host);
    hostLayout->setContentsMargins(8, 8, 8, 8);
    hostLayout->setSpacing(8);
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
    paneMenu->addAction(QStringLiteral("Split Horizontal"), this, &MainWindow::splitActivePaneHorizontal, QKeySequence(QStringLiteral("Ctrl+Shift+H")));
    paneMenu->addAction(QStringLiteral("Split Vertical"), this, &MainWindow::splitActivePaneVertical, QKeySequence(QStringLiteral("Ctrl+Shift+V")));

    auto* settingsMenu = menuBar()->addMenu(QStringLiteral("&Settings"));
    settingsMenu->addAction(QStringLiteral("Preferences..."), this, &MainWindow::showSettingsDialog, QKeySequence(QStringLiteral("Ctrl+,")));
}

void MainWindow::createInitialPane() {
    TerminalPane* pane = createPane();
    m_rootNode = pane;
    centralWidget()->layout()->addWidget(m_rootNode);
    setActivePane(pane);
}

TerminalPane* MainWindow::createPane(const QString& title) {
    TerminalPane* pane = new TerminalPane(nextPaneId(), m_settings.defaultShell, this);
    if (!title.isEmpty()) {
        pane->setTitle(title);
    }
    wirePaneSignals(pane);
    return pane;
}

void MainWindow::replaceNodeInParent(QWidget* oldNode, QWidget* newNode) {
    QWidget* parent = oldNode->parentWidget();
    if (!parent) {
        m_rootNode = newNode;
        return;
    }

    if (auto* parentSplitter = qobject_cast<QSplitter*>(parent)) {
        const int index = parentSplitter->indexOf(oldNode);
        oldNode->setParent(nullptr);
        newNode->setParent(parentSplitter);
        parentSplitter->insertWidget(index, newNode);
        return;
    }

    if (parent == centralWidget()) {
        auto* layout = centralWidget()->layout();
        layout->removeWidget(oldNode);
        oldNode->setParent(nullptr);
        layout->addWidget(newNode);
        m_rootNode = newNode;
    }
}

void MainWindow::splitPane(TerminalPane* pane, Qt::Orientation orientation) {
    if (!pane) {
        return;
    }

    QList<TerminalPane*> panes;
    collectPanes(m_rootNode, panes);
    if (panes.size() >= m_settings.maxPanes) {
        QMessageBox::information(this, QStringLiteral("Pane limit reached"), QStringLiteral("Increase pane limit in Settings to split further."));
        return;
    }

    QWidget* parentNode = pane->parentWidget();
    auto* sibling = createPane();

    if (auto* parentSplitter = qobject_cast<QSplitter*>(parentNode)) {
        if (parentSplitter->orientation() == orientation) {
            const int index = parentSplitter->indexOf(pane);
            parentSplitter->insertWidget(index + 1, sibling);
            QList<int> sizes = parentSplitter->sizes();
            if (index >= 0 && index < sizes.size()) {
                int current = sizes[index];
                int half = qMax(1, current / 2);
                sizes[index] = half;
                sizes.insert(index + 1, current - half);
                parentSplitter->setSizes(sizes);
            }
            setActivePane(sibling);
            return;
        }
    }

    auto* newSplitter = new QSplitter(orientation, this);
    replaceNodeInParent(pane, newSplitter);
    pane->setParent(newSplitter);
    sibling->setParent(newSplitter);
    newSplitter->addWidget(pane);
    newSplitter->addWidget(sibling);
    newSplitter->setSizes({1, 1});
    setActivePane(sibling);
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

        TerminalPane* pane = new TerminalPane(paneId, m_settings.defaultShell, this);
        if (!paneTitle.isEmpty()) {
            pane->setTitle(paneTitle);
        }

        wirePaneSignals(pane);
        return pane;
    }

    if (type == QStringLiteral("splitter")) {
        const QString orientationString = nodeObject.value(QStringLiteral("orientation")).toString();
        Qt::Orientation orientation = orientationString == QStringLiteral("vertical") ? Qt::Vertical : Qt::Horizontal;

        auto* splitter = new QSplitter(orientation, this);
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

TerminalPane* MainWindow::findPaneById(const QString& paneId) const {
    QList<TerminalPane*> panes;
    collectPanes(m_rootNode, panes);
    for (TerminalPane* pane : panes) {
        if (pane->paneId() == paneId) {
            return pane;
        }
    }
    return nullptr;
}

TerminalPane* MainWindow::findPaneByTitle(const QString& paneTitle) const {
    QList<TerminalPane*> panes;
    collectPanes(m_rootNode, panes);
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

void MainWindow::setActivePane(TerminalPane* pane) {
    m_activePane = pane;
    if (!pane) {
        return;
    }

    QList<TerminalPane*> panes;
    collectPanes(m_rootNode, panes);
    for (TerminalPane* p : panes) {
        if (p == pane) {
            p->setStyleSheet(QStringLiteral(
                "TerminalPane { border: 2px solid rgba(61, 174, 233, 0.85); border-radius: 8px; background: rgba(250, 251, 253, 0.92); }"));
        } else {
            p->setStyleSheet(QStringLiteral(
                "TerminalPane { border: 1px solid rgba(92, 107, 129, 0.4); border-radius: 8px; background: rgba(250, 251, 253, 0.78); }"));
        }
    }
}

void MainWindow::wirePaneSignals(TerminalPane* pane) {
    connect(pane, &TerminalPane::splitRequested, this, &MainWindow::splitPane);
    connect(pane, &TerminalPane::activated, this, &MainWindow::setActivePane);
}
