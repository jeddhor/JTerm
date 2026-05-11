#pragma once

#include <QString>

struct AppSettings {
    QString defaultShell;
    QString terminalColorScheme;
    int maxPanes = 32;
};

namespace SettingsStore {
AppSettings load();
void save(const AppSettings& settings);
}
