#pragma once

#include "AppSettings.h"

#include <QDialog>
#include <QList>

class QNetworkAccessManager;
class QNetworkReply;
class QPlainTextEdit;
class QPushButton;
class QTextBrowser;

class LlmChatDialog : public QDialog {
    Q_OBJECT

public:
    explicit LlmChatDialog(const AppSettings& settings, QWidget* parent = nullptr);

    void setSettings(const AppSettings& settings);

private slots:
    void sendPrompt();
    void copyTranscript();
    void copyLastReply();
    void clearConversation();
    void onNetworkReadyRead();
    void onNetworkFinished();

private:
    struct ChatMessage {
        QString role;
        QString content;
    };

    static QString joinUrl(const QString& baseUrl, const QString& suffix);
    static QString markdownSystemInstruction();

    void appendMessage(const QString& role, const QString& content);
    void renderTranscript();
    void scrollTranscriptToBottom();
    void beginRequest();
    void processOllamaChunk(const QByteArray& line);
    void processOpenAiSseLine(const QByteArray& line);

    AppSettings m_settings;
    QList<ChatMessage> m_messages;

    QTextBrowser* m_transcriptView;
    QPlainTextEdit* m_inputEdit;
    QPushButton* m_sendButton;
    QPushButton* m_copyAllButton;
    QPushButton* m_copyLastButton;
    QPushButton* m_clearButton;

    QNetworkAccessManager* m_network;
    QNetworkReply* m_reply;
    QByteArray m_streamBuffer;
    int m_streamingAssistantIndex;
};
