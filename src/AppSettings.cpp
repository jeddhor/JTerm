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
    QSettings settings(QStringLiteral("SplitTerm"), QStringLiteral("SplitTerm"));

    AppSettings result;
    result.defaultShell = settings.value(QStringLiteral("terminal/defaultShell"), defaultShellForPlatform()).toString();
    result.terminalColorScheme = settings.value(QStringLiteral("terminal/colorScheme"), QStringLiteral("WhiteOnBlack")).toString();
    result.maxPanes = settings.value(QStringLiteral("layout/maxPanes"), 32).toInt();
    if (result.maxPanes < 2) {
        result.maxPanes = 2;
    }
    if (result.maxPanes > 128) {
        result.maxPanes = 128;
    }
    return result;
}

void save(const AppSettings& settingsData) {
    QSettings settings(QStringLiteral("SplitTerm"), QStringLiteral("SplitTerm"));

    settings.setValue(QStringLiteral("terminal/defaultShell"), settingsData.defaultShell);
    settings.setValue(QStringLiteral("terminal/colorScheme"), settingsData.terminalColorScheme);
    settings.setValue(QStringLiteral("layout/maxPanes"), settingsData.maxPanes);
}

}
