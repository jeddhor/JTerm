#pragma once

#include "AppSettings.h"

#include <QDialog>

class QComboBox;
class QLineEdit;
class QSpinBox;

class SettingsDialog : public QDialog {
    Q_OBJECT

public:
    explicit SettingsDialog(const AppSettings& initialSettings, QWidget* parent = nullptr);

    AppSettings settings() const;

private:
    QComboBox* m_themeCombo;
    QLineEdit* m_shellEdit;
    QSpinBox* m_maxPanesSpin;
};
