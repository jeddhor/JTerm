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
}

}
