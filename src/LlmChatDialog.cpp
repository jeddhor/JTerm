#include "LlmChatDialog.h"

#include <QApplication>
#include <QClipboard>
#include <QHBoxLayout>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QScrollBar>
#include <QTextBrowser>
#include <QVBoxLayout>

LlmChatDialog::LlmChatDialog(const AppSettings& settings, QWidget* parent)
    : QDialog(parent)
    , m_settings(settings)
    , m_transcriptView(new QTextBrowser(this))
    , m_inputEdit(new QPlainTextEdit(this))
    , m_sendButton(new QPushButton(QStringLiteral("Send"), this))
    , m_copyAllButton(new QPushButton(QStringLiteral("Copy Transcript"), this))
    , m_copyLastButton(new QPushButton(QStringLiteral("Copy Last Reply"), this))
    , m_clearButton(new QPushButton(QStringLiteral("Clear"), this))
    , m_network(new QNetworkAccessManager(this))
    , m_reply(nullptr)
    , m_streamingAssistantIndex(-1) {
    setWindowTitle(QStringLiteral("Ask the LLM"));
    setModal(false);
    resize(920, 660);

    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(8, 8, 8, 8);
    root->setSpacing(8);

    auto* tip = new QLabel(QStringLiteral("Responses are rendered as Markdown. You can keep chatting while terminals remain interactive."), this);
    tip->setWordWrap(true);

    m_transcriptView->setOpenExternalLinks(true);
    m_transcriptView->setReadOnly(true);

    m_inputEdit->setPlaceholderText(QStringLiteral("Ask a shell question..."));
    m_inputEdit->setMinimumHeight(88);

    auto* toolsRow = new QHBoxLayout();
    toolsRow->addWidget(m_copyAllButton);
    toolsRow->addWidget(m_copyLastButton);
    toolsRow->addWidget(m_clearButton);
    toolsRow->addStretch(1);

    auto* inputRow = new QHBoxLayout();
    inputRow->addWidget(m_inputEdit, 1);
    inputRow->addWidget(m_sendButton);

    root->addWidget(tip);
    root->addLayout(toolsRow);
    root->addWidget(m_transcriptView, 1);
    root->addLayout(inputRow);

    connect(m_sendButton, &QPushButton::clicked, this, &LlmChatDialog::sendPrompt);
    connect(m_copyAllButton, &QPushButton::clicked, this, &LlmChatDialog::copyTranscript);
    connect(m_copyLastButton, &QPushButton::clicked, this, &LlmChatDialog::copyLastReply);
    connect(m_clearButton, &QPushButton::clicked, this, &LlmChatDialog::clearConversation);

    renderTranscript();
}

void LlmChatDialog::setSettings(const AppSettings& settings) {
    m_settings = settings;
}

QString LlmChatDialog::joinUrl(const QString& baseUrl, const QString& suffix) {
    QString base = baseUrl.trimmed();
    QString tail = suffix.trimmed();
    if (base.endsWith(QLatin1Char('/'))) {
        base.chop(1);
    }
    if (tail.startsWith(QLatin1Char('/'))) {
        tail.remove(0, 1);
    }
    return base + QLatin1Char('/') + tail;
}

QString LlmChatDialog::markdownSystemInstruction() {
    return QStringLiteral("Always format responses using Markdown. Use fenced code blocks for shell commands.");
}

void LlmChatDialog::appendMessage(const QString& role, const QString& content) {
    ChatMessage message;
    message.role = role;
    message.content = content;
    m_messages.append(message);
}

void LlmChatDialog::renderTranscript() {
    QString markdown;
    if (m_messages.isEmpty()) {
        markdown = QStringLiteral("_Start by asking a command-line question._");
    } else {
        for (const ChatMessage& message : m_messages) {
            if (message.role == QStringLiteral("user")) {
                markdown += QStringLiteral("### You\n\n") + message.content + QStringLiteral("\n\n");
            } else if (message.role == QStringLiteral("assistant")) {
                markdown += QStringLiteral("### Shell Helper\n\n") + message.content + QStringLiteral("\n\n");
            }
        }
    }

    m_transcriptView->setMarkdown(markdown);
    scrollTranscriptToBottom();
}

void LlmChatDialog::scrollTranscriptToBottom() {
    QScrollBar* scrollBar = m_transcriptView->verticalScrollBar();
    if (!scrollBar) {
        return;
    }
    scrollBar->setValue(scrollBar->maximum());
}

void LlmChatDialog::beginRequest() {
    if (!m_settings.hasLlmConfiguration()) {
        return;
    }

    QJsonArray requestMessages;

    QJsonObject systemMessage;
    systemMessage.insert(QStringLiteral("role"), QStringLiteral("system"));
    QString sysPrompt = m_settings.llmSystemPrompt.trimmed();
    if (sysPrompt.isEmpty()) {
        sysPrompt = QStringLiteral("You are JTerm Shell Helper.");
    }
    systemMessage.insert(QStringLiteral("content"), sysPrompt + QStringLiteral("\n\n") + markdownSystemInstruction());
    requestMessages.append(systemMessage);

    for (const ChatMessage& item : m_messages) {
        if (item.role != QStringLiteral("user") && item.role != QStringLiteral("assistant")) {
            continue;
        }
        QJsonObject msg;
        msg.insert(QStringLiteral("role"), item.role);
        msg.insert(QStringLiteral("content"), item.content);
        requestMessages.append(msg);
    }

    QJsonObject payload;
    QString endpoint;
    if (m_settings.llmProvider.trimmed().toLower() == QStringLiteral("ollama")) {
        endpoint = joinUrl(m_settings.llmBaseUrl, QStringLiteral("api/chat"));
        payload.insert(QStringLiteral("model"), m_settings.llmModel);
        payload.insert(QStringLiteral("messages"), requestMessages);
        payload.insert(QStringLiteral("stream"), true);
    } else {
        endpoint = joinUrl(m_settings.llmBaseUrl, QStringLiteral("chat/completions"));
        payload.insert(QStringLiteral("model"), m_settings.llmModel);
        payload.insert(QStringLiteral("messages"), requestMessages);
        payload.insert(QStringLiteral("stream"), true);
    }

    QNetworkRequest request(QUrl(endpoint));
    request.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/json"));
    if (m_settings.llmProvider.trimmed().toLower() == QStringLiteral("openai") && !m_settings.llmApiKey.trimmed().isEmpty()) {
        request.setRawHeader("Authorization", QStringLiteral("Bearer %1").arg(m_settings.llmApiKey.trimmed()).toUtf8());
    }

    m_streamBuffer.clear();
    m_reply = m_network->post(request, QJsonDocument(payload).toJson(QJsonDocument::Compact));
    connect(m_reply, &QNetworkReply::readyRead, this, &LlmChatDialog::onNetworkReadyRead);
    connect(m_reply, &QNetworkReply::finished, this, &LlmChatDialog::onNetworkFinished);
}

void LlmChatDialog::sendPrompt() {
    if (m_reply) {
        return;
    }

    const QString question = m_inputEdit->toPlainText().trimmed();
    if (question.isEmpty()) {
        return;
    }
    if (!m_settings.hasLlmConfiguration()) {
        appendMessage(QStringLiteral("assistant"), QStringLiteral("LLM settings are incomplete. Open Settings -> LLM and configure provider, URL, and model."));
        renderTranscript();
        return;
    }

    appendMessage(QStringLiteral("user"), question);
    appendMessage(QStringLiteral("assistant"), QString());
    m_streamingAssistantIndex = m_messages.size() - 1;
    renderTranscript();

    m_inputEdit->clear();
    m_sendButton->setEnabled(false);
    beginRequest();
}

void LlmChatDialog::copyTranscript() {
    QApplication::clipboard()->setText(m_transcriptView->toPlainText());
}

void LlmChatDialog::copyLastReply() {
    for (int i = m_messages.size() - 1; i >= 0; --i) {
        if (m_messages[i].role == QStringLiteral("assistant") && !m_messages[i].content.trimmed().isEmpty()) {
            QApplication::clipboard()->setText(m_messages[i].content);
            return;
        }
    }
}

void LlmChatDialog::clearConversation() {
    if (m_reply) {
        m_reply->abort();
    }
    m_messages.clear();
    m_streamingAssistantIndex = -1;
    renderTranscript();
}

void LlmChatDialog::processOllamaChunk(const QByteArray& line) {
    const QByteArray trimmed = line.trimmed();
    if (trimmed.isEmpty()) {
        return;
    }

    const QJsonDocument json = QJsonDocument::fromJson(trimmed);
    if (!json.isObject()) {
        return;
    }

    const QJsonObject root = json.object();
    const QJsonObject messageObject = root.value(QStringLiteral("message")).toObject();
    const QString contentChunk = messageObject.value(QStringLiteral("content")).toString();
    if (!contentChunk.isEmpty() && m_streamingAssistantIndex >= 0 && m_streamingAssistantIndex < m_messages.size()) {
        m_messages[m_streamingAssistantIndex].content += contentChunk;
        renderTranscript();
    }
}

void LlmChatDialog::processOpenAiSseLine(const QByteArray& line) {
    const QByteArray trimmed = line.trimmed();
    if (trimmed.isEmpty() || !trimmed.startsWith("data:")) {
        return;
    }

    QByteArray payload = trimmed.mid(5).trimmed();
    if (payload == "[DONE]") {
        return;
    }

    const QJsonDocument json = QJsonDocument::fromJson(payload);
    if (!json.isObject()) {
        return;
    }

    const QJsonArray choices = json.object().value(QStringLiteral("choices")).toArray();
    if (choices.isEmpty() || !choices.first().isObject()) {
        return;
    }

    const QJsonObject delta = choices.first().toObject().value(QStringLiteral("delta")).toObject();
    const QString chunk = delta.value(QStringLiteral("content")).toString();
    if (!chunk.isEmpty() && m_streamingAssistantIndex >= 0 && m_streamingAssistantIndex < m_messages.size()) {
        m_messages[m_streamingAssistantIndex].content += chunk;
        renderTranscript();
    }
}

void LlmChatDialog::onNetworkReadyRead() {
    if (!m_reply) {
        return;
    }

    m_streamBuffer += m_reply->readAll();

    int newline = m_streamBuffer.indexOf('\n');
    while (newline >= 0) {
        const QByteArray line = m_streamBuffer.left(newline);
        m_streamBuffer.remove(0, newline + 1);

        if (m_settings.llmProvider.trimmed().toLower() == QStringLiteral("ollama")) {
            processOllamaChunk(line);
        } else {
            processOpenAiSseLine(line);
        }

        newline = m_streamBuffer.indexOf('\n');
    }
}

void LlmChatDialog::onNetworkFinished() {
    if (!m_reply) {
        m_sendButton->setEnabled(true);
        return;
    }

    if (m_reply->error() != QNetworkReply::NoError) {
        const QString message = QStringLiteral("Request failed: %1").arg(m_reply->errorString());
        if (m_streamingAssistantIndex >= 0 && m_streamingAssistantIndex < m_messages.size()) {
            if (m_messages[m_streamingAssistantIndex].content.trimmed().isEmpty()) {
                m_messages[m_streamingAssistantIndex].content = message;
            } else {
                m_messages[m_streamingAssistantIndex].content += QStringLiteral("\n\n") + message;
            }
        } else {
            appendMessage(QStringLiteral("assistant"), message);
        }
    }

    if (!m_streamBuffer.isEmpty()) {
        if (m_settings.llmProvider.trimmed().toLower() == QStringLiteral("ollama")) {
            processOllamaChunk(m_streamBuffer);
        } else {
            processOpenAiSseLine(m_streamBuffer);
        }
    }

    m_reply->deleteLater();
    m_reply = nullptr;
    m_streamBuffer.clear();
    m_streamingAssistantIndex = -1;
    m_sendButton->setEnabled(true);
    renderTranscript();
}
