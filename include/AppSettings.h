#pragma once

#include <QString>

struct AppSettings {
    QString defaultShell;
    QString themeName;
    int maxPanes = 32;
};

namespace SettingsStore {
AppSettings load();
void save(const AppSettings& settings);
}
