#include "CommandServer.h"

#include <QDataStream>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLocalServer>
#include <QLocalSocket>

namespace {
QString sanitizeName(const QString& input) {
    QString out = input;
    for (QChar& c : out) {
        if (!c.isLetterOrNumber()) {
            c = QLatin1Char('_');
        }
    }
    if (out.isEmpty()) {
        out = QStringLiteral("user");
    }
    return out;
}
}

CommandServer::CommandServer(QObject* parent)
    : QObject(parent)
    , m_server(new QLocalServer(this))
    , m_serverName(defaultServerName()) {
    connect(m_server, &QLocalServer::newConnection, this, &CommandServer::onNewConnection);
}

bool CommandServer::startListening() {
    QLocalServer::removeServer(m_serverName);
    return m_server->listen(m_serverName);
}

QString CommandServer::serverName() const {
    return m_serverName;
}

QString CommandServer::defaultServerName() {
    const QString username = qEnvironmentVariable("USER").isEmpty()
        ? qEnvironmentVariable("USERNAME")
        : qEnvironmentVariable("USER");
    return QStringLiteral("jterm_ipc_") + sanitizeName(username);
}

bool CommandServer::sendCommandToRunningInstance(const QString& paneId, const QString& paneTitle, const QString& command, QString* errorMessage) {
    QLocalSocket socket;
    socket.connectToServer(defaultServerName());
    if (!socket.waitForConnected(800)) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("Could not connect to running JTerm instance.");
        }
        return false;
    }

    QJsonObject commandObject;
    commandObject.insert(QStringLiteral("paneId"), paneId);
    commandObject.insert(QStringLiteral("paneTitle"), paneTitle);
    commandObject.insert(QStringLiteral("command"), command);

    const QByteArray payload = QJsonDocument(commandObject).toJson(QJsonDocument::Compact) + '\n';
    socket.write(payload);
    if (!socket.waitForBytesWritten(800)) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("Timed out writing command to running instance.");
        }
        return false;
    }

    socket.flush();
    socket.disconnectFromServer();
    return true;
}

void CommandServer::onNewConnection() {
    QLocalSocket* client = m_server->nextPendingConnection();
    if (!client) {
        return;
    }

    connect(client, &QLocalSocket::readyRead, this, [this, client]() {
        const QByteArray payload = client->readAll();
        const QJsonDocument doc = QJsonDocument::fromJson(payload);
        if (!doc.isObject()) {
            client->disconnectFromServer();
            return;
        }

        const QJsonObject obj = doc.object();
        const QString paneId = obj.value(QStringLiteral("paneId")).toString();
        const QString paneTitle = obj.value(QStringLiteral("paneTitle")).toString();
        const QString command = obj.value(QStringLiteral("command")).toString();
        emit commandReceived(paneId, paneTitle, command);

        client->disconnectFromServer();
    });

    connect(client, &QLocalSocket::disconnected, client, &QLocalSocket::deleteLater);
}
