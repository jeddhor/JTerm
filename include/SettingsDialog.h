#pragma once

#include "AppSettings.h"

#include <QDialog>

class QComboBox;
class QCheckBox;
class QLabel;
class QLineEdit;
class QPlainTextEdit;
class QPushButton;
class QSpinBox;
class QTabWidget;
class QNetworkAccessManager;

class SettingsDialog : public QDialog {
    Q_OBJECT

public:
    enum class InitialTab {
        General,
        Llm
    };

    explicit SettingsDialog(const AppSettings& initialSettings, QWidget* parent = nullptr, InitialTab initialTab = InitialTab::General);

    AppSettings settings() const;

private slots:
    void onVerifyLlmSettings();
    void onProviderChanged(int index);

private:
    static QString defaultSystemPrompt();
    static QString joinUrl(const QString& baseUrl, const QString& suffix);
    AppSettings collectSettingsFromUi() const;
    void refreshLlmFieldState();

    QTabWidget* m_tabs;

    QComboBox* m_colorSchemeCombo;
    QLineEdit* m_shellEdit;
    QSpinBox* m_maxPanesSpin;
    QSpinBox* m_startupThrottleSpin;
    QSpinBox* m_longRunningNotificationSpin;
    QCheckBox* m_confirmExitCheck;
    QCheckBox* m_warnStartupScriptsCheck;
    QCheckBox* m_autoSaveRestoreLayoutCheck;
    QCheckBox* m_broadcastAllOverrideCheck;
    QCheckBox* m_confirmRiskyBroadcastCheck;
    QCheckBox* m_safePasteGuardCheck;

    QComboBox* m_llmProviderCombo;
    QLineEdit* m_llmBaseUrlEdit;
    QLineEdit* m_llmModelEdit;
    QLineEdit* m_llmApiKeyEdit;
    QPlainTextEdit* m_llmSystemPromptEdit;
    QPushButton* m_verifyLlmButton;
    QLabel* m_verifyStatusLabel;

    QNetworkAccessManager* m_network;
};
