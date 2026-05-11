#include "CommandServer.h"
#include "MainWindow.h"

#include <QApplication>
#include <QCommandLineOption>
#include <QCommandLineParser>
#include <QMessageBox>

int main(int argc, char* argv[]) {
    QApplication app(argc, argv);
    QApplication::setApplicationName(QStringLiteral("SplitTerm"));
    QApplication::setOrganizationName(QStringLiteral("SplitTerm"));

    QCommandLineParser parser;
    parser.setApplicationDescription(QStringLiteral("SplitTerm - split-pane terminal emulator"));
    parser.addHelpOption();

    QCommandLineOption sendOption(QStringList() << QStringLiteral("s") << QStringLiteral("send"), QStringLiteral("Send command to running instance."), QStringLiteral("command"));
    QCommandLineOption paneIdOption(QStringList() << QStringLiteral("pane-id"), QStringLiteral("Target pane by id."), QStringLiteral("paneId"));
    QCommandLineOption paneTitleOption(QStringList() << QStringLiteral("pane-title"), QStringLiteral("Target pane by title."), QStringLiteral("paneTitle"));

    parser.addOption(sendOption);
    parser.addOption(paneIdOption);
    parser.addOption(paneTitleOption);
    parser.process(app);

    if (parser.isSet(sendOption)) {
        const QString command = parser.value(sendOption);
        const QString paneId = parser.value(paneIdOption);
        const QString paneTitle = parser.value(paneTitleOption);

        QString error;
        if (!CommandServer::sendCommandToRunningInstance(paneId, paneTitle, command, &error)) {
            QMessageBox::critical(nullptr, QStringLiteral("SplitTerm CLI"), error);
            return 1;
        }
        return 0;
    }

    MainWindow window;
    window.show();
    return app.exec();
}
