#pragma once

#include <QString>

struct AppSettings {
    QString defaultShell;
    int maxPanes = 32;
};

namespace SettingsStore {
AppSettings load();
void save(const AppSettings& settings);
}
