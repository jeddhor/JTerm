#pragma once

#include <QString>

struct AppSettings {
    QString defaultShell;
    QString terminalColorScheme;
    int maxPanes = 32;
    bool confirmOnMultiPaneExit = true;
    bool warnOnLayoutStartupScripts = true;
    bool autoSaveRestoreLayout = true;
    bool broadcastAllOverride = false;
    bool confirmRiskyBroadcastCommands = true;
    bool safePasteGuard = true;
    int startupScriptThrottleIntervalSeconds = 1;
    int longRunningNotificationSeconds = 20;

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
