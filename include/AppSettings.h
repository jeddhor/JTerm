#pragma once

#include <QString>

struct AppSettings {
    QString defaultShell;
    QString terminalColorScheme;
    int maxPanes = 32;
    bool confirmOnMultiPaneExit = true;
};

namespace SettingsStore {
AppSettings load();
void save(const AppSettings& settings);
}
