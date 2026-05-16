#include "SettingsDialog.h"

#include "TerminalView.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QEventLoop>
#include <QFormLayout>
#include <QJsonDocument>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QSpinBox>
#include <QTabWidget>
#include <QTimer>
#include <QVBoxLayout>

SettingsDialog::SettingsDialog(const AppSettings& initialSettings, QWidget* parent, InitialTab initialTab)
    : QDialog(parent)
    , m_tabs(new QTabWidget(this))
    , m_colorSchemeCombo(new QComboBox(this))
    , m_shellEdit(new QLineEdit(this))
    , m_maxPanesSpin(new QSpinBox(this))
    , m_startupThrottleSpin(new QSpinBox(this))
    , m_llmProviderCombo(new QComboBox(this))
    , m_llmBaseUrlEdit(new QLineEdit(this))
    , m_llmModelEdit(new QLineEdit(this))
    , m_llmApiKeyEdit(new QLineEdit(this))
    , m_llmSystemPromptEdit(new QPlainTextEdit(this))
    , m_verifyLlmButton(new QPushButton(QStringLiteral("Verify Settings"), this))
    , m_verifyStatusLabel(new QLabel(this))
    , m_network(new QNetworkAccessManager(this)) {
    m_confirmExitCheck = new QCheckBox(QStringLiteral("Confirm before exiting when multiple tabs or panes are open"), this);
    m_warnStartupScriptsCheck = new QCheckBox(QStringLiteral("Warn when loading layouts that contain startup commands"), this);
    m_autoSaveRestoreLayoutCheck = new QCheckBox(QStringLiteral("Restore last session tabs and panes on startup"), this);
    m_broadcastAllOverrideCheck = new QCheckBox(QStringLiteral("Broadcast always targets all panes (override targets)"), this);
    setWindowTitle(QStringLiteral("Settings"));
    resize(700, 520);

    auto* rootLayout = new QVBoxLayout(this);

    auto* generalTab = new QWidget(this);
    auto* generalLayout = new QVBoxLayout(generalTab);

    auto* formLayout = new QFormLayout();
    formLayout->setLabelAlignment(Qt::AlignLeft);

    m_colorSchemeCombo->addItems(TerminalView::availableColorSchemes());
    int schemeIndex = m_colorSchemeCombo->findText(initialSettings.terminalColorScheme);
    if (schemeIndex < 0) {
        schemeIndex = m_colorSchemeCombo->findText(QStringLiteral("WhiteOnBlack"));
    }
    if (schemeIndex >= 0) {
        m_colorSchemeCombo->setCurrentIndex(schemeIndex);
    }

    m_shellEdit->setText(initialSettings.defaultShell);
    m_shellEdit->setPlaceholderText(QStringLiteral("Default shell executable path"));

    m_maxPanesSpin->setMinimum(2);
    m_maxPanesSpin->setMaximum(128);
    m_maxPanesSpin->setValue(initialSettings.maxPanes);

    m_startupThrottleSpin->setMinimum(1);
    m_startupThrottleSpin->setMaximum(300);
    m_startupThrottleSpin->setSuffix(QStringLiteral(" s"));
    m_startupThrottleSpin->setValue(initialSettings.startupScriptThrottleIntervalSeconds);

    formLayout->addRow(QStringLiteral("Terminal color scheme:"), m_colorSchemeCombo);
    formLayout->addRow(QStringLiteral("Default shell:"), m_shellEdit);
    formLayout->addRow(QStringLiteral("Maximum panes:"), m_maxPanesSpin);
    formLayout->addRow(QStringLiteral("Startup throttle interval:"), m_startupThrottleSpin);

    generalLayout->addLayout(formLayout);
    m_confirmExitCheck->setChecked(initialSettings.confirmOnMultiPaneExit);
    m_warnStartupScriptsCheck->setChecked(initialSettings.warnOnLayoutStartupScripts);
    m_autoSaveRestoreLayoutCheck->setChecked(initialSettings.autoSaveRestoreLayout);
    m_broadcastAllOverrideCheck->setChecked(initialSettings.broadcastAllOverride);
    generalLayout->addWidget(m_confirmExitCheck);
    generalLayout->addWidget(m_warnStartupScriptsCheck);
    generalLayout->addWidget(m_autoSaveRestoreLayoutCheck);
    generalLayout->addWidget(m_broadcastAllOverrideCheck);
    generalLayout->addStretch(1);

    auto* llmTab = new QWidget(this);
    auto* llmLayout = new QVBoxLayout(llmTab);
    auto* llmForm = new QFormLayout();
    llmForm->setLabelAlignment(Qt::AlignLeft);

    m_llmProviderCombo->addItem(QStringLiteral("Ollama"), QStringLiteral("ollama"));
    m_llmProviderCombo->addItem(QStringLiteral("OpenAI-Compatible"), QStringLiteral("openai"));
    const QString initialProvider = initialSettings.llmProvider.trimmed().isEmpty()
        ? QStringLiteral("ollama")
        : initialSettings.llmProvider.trimmed().toLower();
    const int providerIndex = m_llmProviderCombo->findData(initialProvider);
    m_llmProviderCombo->setCurrentIndex(providerIndex >= 0 ? providerIndex : 0);

    m_llmBaseUrlEdit->setText(initialSettings.llmBaseUrl.trimmed());
    m_llmBaseUrlEdit->setPlaceholderText(QStringLiteral("http://127.0.0.1:11434"));

    m_llmModelEdit->setText(initialSettings.llmModel.trimmed());
    m_llmModelEdit->setPlaceholderText(QStringLiteral("llama3.1:8b"));

    m_llmApiKeyEdit->setText(initialSettings.llmApiKey);
    m_llmApiKeyEdit->setEchoMode(QLineEdit::Password);
    m_llmApiKeyEdit->setPlaceholderText(QStringLiteral("sk-... (optional for some local endpoints)"));

    const QString configuredPrompt = initialSettings.llmSystemPrompt.trimmed().isEmpty()
        ? defaultSystemPrompt()
        : initialSettings.llmSystemPrompt.trimmed();
    m_llmSystemPromptEdit->setPlainText(configuredPrompt);
    m_llmSystemPromptEdit->setPlaceholderText(defaultSystemPrompt());
    m_llmSystemPromptEdit->setMinimumHeight(160);

    llmForm->addRow(QStringLiteral("Provider:"), m_llmProviderCombo);
    llmForm->addRow(QStringLiteral("Base URL:"), m_llmBaseUrlEdit);
    llmForm->addRow(QStringLiteral("Model:"), m_llmModelEdit);
    llmForm->addRow(QStringLiteral("API Key:"), m_llmApiKeyEdit);
    llmForm->addRow(QStringLiteral("System Prompt:"), m_llmSystemPromptEdit);

    m_verifyStatusLabel->setText(QStringLiteral("Enter provider, URL, and model, then verify."));
    m_verifyStatusLabel->setWordWrap(true);

    llmLayout->addLayout(llmForm);
    llmLayout->addWidget(m_verifyLlmButton, 0, Qt::AlignLeft);
    llmLayout->addWidget(m_verifyStatusLabel);
    llmLayout->addStretch(1);

    m_tabs->addTab(generalTab, QStringLiteral("General"));
    m_tabs->addTab(llmTab, QStringLiteral("LLM"));
    m_tabs->setCurrentIndex(initialTab == InitialTab::Llm ? 1 : 0);

    rootLayout->addWidget(m_tabs, 1);

    auto* buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    connect(buttonBox, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);
    connect(m_verifyLlmButton, &QPushButton::clicked, this, &SettingsDialog::onVerifyLlmSettings);
    connect(m_llmProviderCombo, qOverload<int>(&QComboBox::currentIndexChanged), this, &SettingsDialog::onProviderChanged);

    rootLayout->addWidget(buttonBox);
    refreshLlmFieldState();
}

AppSettings SettingsDialog::settings() const {
    return collectSettingsFromUi();
}

QString SettingsDialog::defaultSystemPrompt() {
    return QStringLiteral(
        "You are JTerm Shell Helper, a practical terminal assistant. "
        "Help with shell commands, diagnostics, and safe fixes. "
        "Always respond in Markdown and include commands in fenced code blocks when appropriate.");
}

QString SettingsDialog::joinUrl(const QString& baseUrl, const QString& suffix) {
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

AppSettings SettingsDialog::collectSettingsFromUi() const {
    AppSettings result;
    result.terminalColorScheme = m_colorSchemeCombo->currentText();
    result.defaultShell = m_shellEdit->text().trimmed();
    result.maxPanes = m_maxPanesSpin->value();
    result.startupScriptThrottleIntervalSeconds = m_startupThrottleSpin->value();
    result.confirmOnMultiPaneExit = m_confirmExitCheck->isChecked();
    result.warnOnLayoutStartupScripts = m_warnStartupScriptsCheck->isChecked();
    result.autoSaveRestoreLayout = m_autoSaveRestoreLayoutCheck->isChecked();
    result.broadcastAllOverride = m_broadcastAllOverrideCheck->isChecked();

    result.llmProvider = m_llmProviderCombo->currentData().toString().trimmed().toLower();
    result.llmBaseUrl = m_llmBaseUrlEdit->text().trimmed();
    result.llmModel = m_llmModelEdit->text().trimmed();
    result.llmApiKey = m_llmApiKeyEdit->text();
    result.llmSystemPrompt = m_llmSystemPromptEdit->toPlainText().trimmed();
    if (result.llmSystemPrompt.isEmpty()) {
        result.llmSystemPrompt = defaultSystemPrompt();
    }

    return result;
}

void SettingsDialog::refreshLlmFieldState() {
    const QString provider = m_llmProviderCombo->currentData().toString();
    const bool isOpenAiCompat = provider == QStringLiteral("openai");
    m_llmApiKeyEdit->setEnabled(isOpenAiCompat);

    if (isOpenAiCompat) {
        if (m_llmBaseUrlEdit->text().trimmed().isEmpty() || m_llmBaseUrlEdit->text().contains(QStringLiteral("11434"))) {
            m_llmBaseUrlEdit->setText(QStringLiteral("http://127.0.0.1:8000/v1"));
        }
        m_llmModelEdit->setPlaceholderText(QStringLiteral("gpt-4.1-mini"));
    } else {
        if (m_llmBaseUrlEdit->text().trimmed().isEmpty() || m_llmBaseUrlEdit->text().contains(QStringLiteral("/v1"))) {
            m_llmBaseUrlEdit->setText(QStringLiteral("http://127.0.0.1:11434"));
        }
        m_llmModelEdit->setPlaceholderText(QStringLiteral("llama3.1:8b"));
    }
}

void SettingsDialog::onProviderChanged(int) {
    refreshLlmFieldState();
}

void SettingsDialog::onVerifyLlmSettings() {
    const AppSettings cfg = collectSettingsFromUi();
    if (!cfg.hasLlmConfiguration()) {
        QMessageBox::warning(this, QStringLiteral("Verify LLM Settings"), QStringLiteral("Provider, Base URL, and Model are required before verification."));
        return;
    }

    QString endpoint;
    if (cfg.llmProvider == QStringLiteral("ollama")) {
        endpoint = joinUrl(cfg.llmBaseUrl, QStringLiteral("api/tags"));
    } else {
        endpoint = joinUrl(cfg.llmBaseUrl, QStringLiteral("models"));
    }

    QNetworkRequest request{QUrl(endpoint)};
    request.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/json"));
    if (cfg.llmProvider == QStringLiteral("openai") && !cfg.llmApiKey.trimmed().isEmpty()) {
        request.setRawHeader("Authorization", QStringLiteral("Bearer %1").arg(cfg.llmApiKey.trimmed()).toUtf8());
    }

    m_verifyLlmButton->setEnabled(false);
    m_verifyStatusLabel->setText(QStringLiteral("Verifying..."));

    QNetworkReply* reply = m_network->get(request);
    QEventLoop loop;
    QTimer timeout;
    timeout.setSingleShot(true);
    connect(&timeout, &QTimer::timeout, &loop, &QEventLoop::quit);
    connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
    timeout.start(8000);
    loop.exec();

    bool success = false;
    QString detail;

    if (!reply->isFinished()) {
        reply->abort();
        detail = QStringLiteral("Timed out while connecting to %1").arg(endpoint);
    } else if (reply->error() != QNetworkReply::NoError) {
        detail = QStringLiteral("Connection failed: %1").arg(reply->errorString());
    } else {
        const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        const QByteArray payload = reply->readAll();
        if (status >= 200 && status < 300) {
            success = true;
            detail = QStringLiteral("Verification succeeded (%1).").arg(endpoint);
        } else {
            const QString bodySummary = QString::fromUtf8(payload).left(180);
            detail = QStringLiteral("Server responded with HTTP %1. %2").arg(status).arg(bodySummary);
        }
    }

    reply->deleteLater();
    m_verifyLlmButton->setEnabled(true);
    m_verifyStatusLabel->setText(detail);

    if (success) {
        QMessageBox::information(this, QStringLiteral("Verify LLM Settings"), detail);
    } else {
        QMessageBox::warning(this, QStringLiteral("Verify LLM Settings"), detail);
    }
}
