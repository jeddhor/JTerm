#include "ThemeManager.h"

#include <QPalette>

namespace {
void applyBreezeLight(QApplication& app) {
    app.setStyle(QStringLiteral("Fusion"));

    QPalette palette;
    palette.setColor(QPalette::Window, QColor(243, 244, 246));
    palette.setColor(QPalette::WindowText, QColor(40, 44, 52));
    palette.setColor(QPalette::Base, QColor(255, 255, 255));
    palette.setColor(QPalette::AlternateBase, QColor(234, 236, 240));
    palette.setColor(QPalette::ToolTipBase, QColor(255, 255, 255));
    palette.setColor(QPalette::ToolTipText, QColor(38, 41, 50));
    palette.setColor(QPalette::Text, QColor(40, 44, 52));
    palette.setColor(QPalette::Button, QColor(232, 234, 238));
    palette.setColor(QPalette::ButtonText, QColor(45, 49, 56));
    palette.setColor(QPalette::Highlight, QColor(61, 174, 233));
    palette.setColor(QPalette::HighlightedText, QColor(255, 255, 255));
    app.setPalette(palette);
}

void applyBreezeDark(QApplication& app) {
    app.setStyle(QStringLiteral("Fusion"));

    QPalette palette;
    palette.setColor(QPalette::Window, QColor(35, 38, 41));
    palette.setColor(QPalette::WindowText, QColor(239, 240, 241));
    palette.setColor(QPalette::Base, QColor(27, 30, 32));
    palette.setColor(QPalette::AlternateBase, QColor(44, 48, 52));
    palette.setColor(QPalette::ToolTipBase, QColor(27, 30, 32));
    palette.setColor(QPalette::ToolTipText, QColor(239, 240, 241));
    palette.setColor(QPalette::Text, QColor(239, 240, 241));
    palette.setColor(QPalette::Button, QColor(49, 54, 59));
    palette.setColor(QPalette::ButtonText, QColor(239, 240, 241));
    palette.setColor(QPalette::Highlight, QColor(61, 174, 233));
    palette.setColor(QPalette::HighlightedText, QColor(255, 255, 255));
    app.setPalette(palette);
}

void applySolarized(QApplication& app) {
    app.setStyle(QStringLiteral("Fusion"));

    QPalette palette;
    palette.setColor(QPalette::Window, QColor(253, 246, 227));
    palette.setColor(QPalette::WindowText, QColor(88, 110, 117));
    palette.setColor(QPalette::Base, QColor(238, 232, 213));
    palette.setColor(QPalette::AlternateBase, QColor(247, 241, 220));
    palette.setColor(QPalette::Text, QColor(88, 110, 117));
    palette.setColor(QPalette::Button, QColor(238, 232, 213));
    palette.setColor(QPalette::ButtonText, QColor(88, 110, 117));
    palette.setColor(QPalette::Highlight, QColor(38, 139, 210));
    palette.setColor(QPalette::HighlightedText, QColor(253, 246, 227));
    app.setPalette(palette);
}
}

namespace ThemeManager {

QStringList availableThemes() {
    return {
        QStringLiteral("Breeze Light"),
        QStringLiteral("Breeze Dark"),
        QStringLiteral("Solarized Light")
    };
}

void applyTheme(QApplication& app, const QString& themeName) {
    if (themeName == QStringLiteral("Breeze Dark")) {
        applyBreezeDark(app);
    } else if (themeName == QStringLiteral("Solarized Light")) {
        applySolarized(app);
    } else {
        applyBreezeLight(app);
    }

    app.setStyleSheet(QStringLiteral(
        "QMainWindow {"
        "  background: qlineargradient(x1:0, y1:0, x2:1, y2:1, stop:0 #f5f7fa, stop:1 #e9edf4);"
        "}"
        "QMenuBar, QMenu, QToolBar {"
        "  font-size: 10.5pt;"
        "}"
        "QSplitter::handle {"
        "  background: rgba(75, 85, 99, 0.12);"
        "  border-radius: 2px;"
        "}"
        "QLineEdit, QPlainTextEdit, QComboBox, QSpinBox {"
        "  border: 1px solid rgba(107, 114, 128, 0.45);"
        "  border-radius: 6px;"
        "  padding: 4px;"
        "}"
    ));
}

TerminalColors terminalColorsForTheme(const QString& themeName) {
    if (themeName == QStringLiteral("Breeze Dark")) {
        return {QColor(220, 221, 222), QColor(23, 25, 28), QColor(61, 174, 233)};
    }
    if (themeName == QStringLiteral("Solarized Light")) {
        return {QColor(88, 110, 117), QColor(253, 246, 227), QColor(38, 139, 210)};
    }
    return {QColor(35, 38, 41), QColor(255, 255, 255), QColor(61, 174, 233)};
}

}
