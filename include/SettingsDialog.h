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
    explicit SettingsDialog(const AppSettings& initialSettings, QWidget* parent = nullptr);

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
    QCheckBox* m_confirmExitCheck;
    QCheckBox* m_warnStartupScriptsCheck;
    QCheckBox* m_autoSaveRestoreLayoutCheck;

    QComboBox* m_llmProviderCombo;
    QLineEdit* m_llmBaseUrlEdit;
    QLineEdit* m_llmModelEdit;
    QLineEdit* m_llmApiKeyEdit;
    QPlainTextEdit* m_llmSystemPromptEdit;
    QPushButton* m_verifyLlmButton;
    QLabel* m_verifyStatusLabel;

    QNetworkAccessManager* m_network;
};
