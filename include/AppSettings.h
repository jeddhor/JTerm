#pragma once

#include <QString>

struct AppSettings {
    QString defaultShell;
    QString terminalColorScheme;
    int maxPanes = 32;
    bool confirmOnMultiPaneExit = true;
    bool warnOnLayoutStartupScripts = true;
    bool autoSaveRestoreLayout = false;

    QString llmProvider;
    QString llmBaseUrl;
    QString llmModel;
    QString llmApiKey;
    QString llmSystemPrompt;

    bool hasLlmConfiguration() const;
};

namespace SettingsStore {
AppSettings load();
void save(const AppSettings& settings);
}
