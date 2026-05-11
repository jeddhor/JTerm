#pragma once

#include <QObject>

class QLocalServer;

class CommandServer : public QObject {
    Q_OBJECT

public:
    explicit CommandServer(QObject* parent = nullptr);

    bool startListening();
    QString serverName() const;

    static QString defaultServerName();
    static bool sendCommandToRunningInstance(const QString& paneId, const QString& paneTitle, const QString& command, QString* errorMessage = nullptr);

signals:
    void commandReceived(const QString& paneId, const QString& paneTitle, const QString& command);

private slots:
    void onNewConnection();

private:
    QLocalServer* m_server;
    QString m_serverName;
};
