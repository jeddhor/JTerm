#include "ThemeManager.h"

#include <QPalette>
#include <QStyle>

namespace ThemeManager {

QStringList availableThemes() {
    return {};
}

void applyTheme(QApplication& app, const QString&) {
    app.setPalette(app.style()->standardPalette());
    app.setStyleSheet(QString());
}

TerminalColors terminalColorsForTheme(const QString&) {
    return {
        QColor(216, 216, 216),
        QColor(8, 10, 12),
        QColor(98, 193, 110)
    };
}

}
