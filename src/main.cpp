#include "CommandServer.h"
#include "MainWindow.h"

#include <QApplication>
#include <QCommandLineOption>
#include <QCommandLineParser>
#include <QFileInfo>
#include <QFile>
#include <QIcon>
#include <QMessageBox>
#include <QTextStream>

int main(int argc, char* argv[]) {
    QApplication app(argc, argv);
    QApplication::setApplicationName(QStringLiteral("JTerm"));
    QApplication::setOrganizationName(QStringLiteral("JTerm"));

    const QString appDir = QApplication::applicationDirPath();
    QString logoPath = appDir + QStringLiteral("/assets/JTerm_logo.png");
    if (!QFileInfo::exists(logoPath)) {
        logoPath = appDir + QStringLiteral("/JTerm_logo.png");
    }
    if (!QFileInfo::exists(logoPath)) {
        logoPath = QStringLiteral("assets/JTerm_logo.png");
    }
    if (QFileInfo::exists(logoPath)) {
        app.setWindowIcon(QIcon(logoPath));
    }

    QCommandLineParser parser;
    parser.setApplicationDescription(QStringLiteral("JTerm - split-pane terminal emulator"));
    parser.addHelpOption();

    QCommandLineOption sendOption(QStringList() << QStringLiteral("s") << QStringLiteral("send"), QStringLiteral("Send command to running instance."), QStringLiteral("command"));
    QCommandLineOption paneIdOption(QStringList() << QStringLiteral("pane-id"), QStringLiteral("Target pane by id."), QStringLiteral("paneId"));
    QCommandLineOption paneTitleOption(QStringList() << QStringLiteral("pane-title"), QStringLiteral("Target pane by title."), QStringLiteral("paneTitle"));
    QCommandLineOption layoutOption(QStringList() << QStringLiteral("layout"), QStringLiteral("Load layout JSON file at startup."), QStringLiteral("layoutPath"));
    QCommandLineOption encodeStartupScriptOption(
        QStringList() << QStringLiteral("encode-startup-script"),
        QStringLiteral("Read script file and output base64 for startupScriptBase64."),
        QStringLiteral("scriptFile"));

    parser.addOption(sendOption);
    parser.addOption(paneIdOption);
    parser.addOption(paneTitleOption);
    parser.addOption(layoutOption);
    parser.addOption(encodeStartupScriptOption);
    parser.process(app);

    if (parser.isSet(encodeStartupScriptOption)) {
        const QString inputPath = parser.value(encodeStartupScriptOption);
        QFile file(inputPath);
        if (!file.open(QIODevice::ReadOnly)) {
            QTextStream(stderr) << "Failed to open script file: " << inputPath << "\n";
            return 1;
        }

        const QByteArray encoded = file.readAll().toBase64();
        QTextStream(stdout) << QString::fromLatin1(encoded) << "\n";
        return 0;
    }

    if (parser.isSet(sendOption)) {
        const QString command = parser.value(sendOption);
        const QString paneId = parser.value(paneIdOption);
        const QString paneTitle = parser.value(paneTitleOption);

        QString error;
        if (!CommandServer::sendCommandToRunningInstance(paneId, paneTitle, command, &error)) {
            QMessageBox::critical(nullptr, QStringLiteral("JTerm CLI"), error);
            return 1;
        }
        return 0;
    }

    MainWindow window;
    if (parser.isSet(layoutOption)) {
        QString error;
        if (!window.loadLayoutFromPath(parser.value(layoutOption), &error)) {
            QMessageBox::critical(nullptr, QStringLiteral("JTerm CLI"), error);
            return 1;
        }
    }
    window.show();
    return app.exec();
}
