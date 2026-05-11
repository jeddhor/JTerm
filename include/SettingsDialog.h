#pragma once

#include "AppSettings.h"

#include <QDialog>

class QComboBox;
class QCheckBox;
class QLineEdit;
class QSpinBox;

class SettingsDialog : public QDialog {
    Q_OBJECT

public:
    explicit SettingsDialog(const AppSettings& initialSettings, QWidget* parent = nullptr);

    AppSettings settings() const;

private:
    QComboBox* m_colorSchemeCombo;
    QLineEdit* m_shellEdit;
    QSpinBox* m_maxPanesSpin;
    QCheckBox* m_confirmExitCheck;
    QCheckBox* m_warnStartupScriptsCheck;
};
