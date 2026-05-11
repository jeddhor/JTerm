#pragma once

#include <QApplication>
#include <QColor>
#include <QString>
#include <QStringList>

struct TerminalColors {
    QColor foreground;
    QColor background;
    QColor accent;
};

namespace ThemeManager {
QStringList availableThemes();
void applyTheme(QApplication& app, const QString& themeName);
TerminalColors terminalColorsForTheme(const QString& themeName);
}
