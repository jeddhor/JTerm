#include "AppSettings.h"

#include <QSettings>

namespace {
QString defaultShellForPlatform() {
#ifdef Q_OS_WIN
    return QStringLiteral("powershell.exe");
#else
    return QStringLiteral("/bin/bash");
#endif
}

QString defaultLlmSystemPrompt() {
    return QStringLiteral(
        "You are JTerm Shell Helper, a practical terminal assistant. "
        "Help the user with shell commands, troubleshooting, and safe command usage. "
        "Prefer concise, actionable steps and include exact commands when useful. "
        "Always format your response in Markdown.");
}
}

namespace SettingsStore {

AppSettings load() {
    QSettings settings(QStringLiteral("JTerm"), QStringLiteral("JTerm"));

    AppSettings result;
    result.defaultShell = settings.value(QStringLiteral("terminal/defaultShell"), defaultShellForPlatform()).toString();
    result.terminalColorScheme = settings.value(QStringLiteral("terminal/colorScheme"), QStringLiteral("WhiteOnBlack")).toString();
    result.maxPanes = settings.value(QStringLiteral("layout/maxPanes"), 32).toInt();
    result.confirmOnMultiPaneExit = settings.value(QStringLiteral("ui/confirmOnMultiPaneExit"), true).toBool();
    result.warnOnLayoutStartupScripts = settings.value(QStringLiteral("ui/warnOnLayoutStartupScripts"), true).toBool();
    result.llmProvider = settings.value(QStringLiteral("llm/provider"), QString()).toString().trimmed().toLower();
    result.llmBaseUrl = settings.value(QStringLiteral("llm/baseUrl"), QString()).toString().trimmed();
    result.llmModel = settings.value(QStringLiteral("llm/model"), QString()).toString().trimmed();
    result.llmApiKey = settings.value(QStringLiteral("llm/apiKey"), QString()).toString();
    result.llmSystemPrompt = settings.value(QStringLiteral("llm/systemPrompt"), defaultLlmSystemPrompt()).toString().trimmed();
    if (!result.llmProvider.isEmpty() && result.llmProvider != QStringLiteral("ollama") && result.llmProvider != QStringLiteral("openai")) {
        result.llmProvider.clear();
    }
    if (result.llmSystemPrompt.isEmpty()) {
        result.llmSystemPrompt = defaultLlmSystemPrompt();
    }
    if (result.maxPanes < 2) {
        result.maxPanes = 2;
    }
    if (result.maxPanes > 128) {
        result.maxPanes = 128;
    }
    return result;
}

void save(const AppSettings& settingsData) {
    QSettings settings(QStringLiteral("JTerm"), QStringLiteral("JTerm"));

    settings.setValue(QStringLiteral("terminal/defaultShell"), settingsData.defaultShell);
    settings.setValue(QStringLiteral("terminal/colorScheme"), settingsData.terminalColorScheme);
    settings.setValue(QStringLiteral("layout/maxPanes"), settingsData.maxPanes);
    settings.setValue(QStringLiteral("ui/confirmOnMultiPaneExit"), settingsData.confirmOnMultiPaneExit);
    settings.setValue(QStringLiteral("ui/warnOnLayoutStartupScripts"), settingsData.warnOnLayoutStartupScripts);
    settings.setValue(QStringLiteral("llm/provider"), settingsData.llmProvider.trimmed().toLower());
    settings.setValue(QStringLiteral("llm/baseUrl"), settingsData.llmBaseUrl.trimmed());
    settings.setValue(QStringLiteral("llm/model"), settingsData.llmModel.trimmed());
    settings.setValue(QStringLiteral("llm/apiKey"), settingsData.llmApiKey);
    settings.setValue(QStringLiteral("llm/systemPrompt"), settingsData.llmSystemPrompt.trimmed());
}

}

bool AppSettings::hasLlmConfiguration() const {
    return !llmProvider.trimmed().isEmpty()
        && !llmBaseUrl.trimmed().isEmpty()
        && !llmModel.trimmed().isEmpty();
}
