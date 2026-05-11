#include "SettingsDialog.h"

#include "TerminalView.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QLineEdit>
#include <QSpinBox>
#include <QVBoxLayout>

SettingsDialog::SettingsDialog(const AppSettings& initialSettings, QWidget* parent)
    : QDialog(parent)
    , m_colorSchemeCombo(new QComboBox(this))
    , m_shellEdit(new QLineEdit(this))
    , m_maxPanesSpin(new QSpinBox(this)) {
    m_confirmExitCheck = new QCheckBox(QStringLiteral("Confirm before exiting when multiple tabs or panes are open"), this);
    m_warnStartupScriptsCheck = new QCheckBox(QStringLiteral("Warn when loading layouts that contain startup commands"), this);
    setWindowTitle(QStringLiteral("Settings"));
    resize(520, 280);

    auto* rootLayout = new QVBoxLayout(this);

    auto* formLayout = new QFormLayout();
    formLayout->setLabelAlignment(Qt::AlignLeft);

    m_colorSchemeCombo->addItems(TerminalView::availableColorSchemes());
    int schemeIndex = m_colorSchemeCombo->findText(initialSettings.terminalColorScheme);
    if (schemeIndex < 0) {
        schemeIndex = m_colorSchemeCombo->findText(QStringLiteral("WhiteOnBlack"));
    }
    if (schemeIndex >= 0) {
        m_colorSchemeCombo->setCurrentIndex(schemeIndex);
    }

    m_shellEdit->setText(initialSettings.defaultShell);
    m_shellEdit->setPlaceholderText(QStringLiteral("Default shell executable path"));

    m_maxPanesSpin->setMinimum(2);
    m_maxPanesSpin->setMaximum(128);
    m_maxPanesSpin->setValue(initialSettings.maxPanes);

    formLayout->addRow(QStringLiteral("Terminal color scheme:"), m_colorSchemeCombo);
    formLayout->addRow(QStringLiteral("Default shell:"), m_shellEdit);
    formLayout->addRow(QStringLiteral("Maximum panes:"), m_maxPanesSpin);

    rootLayout->addLayout(formLayout);
    m_confirmExitCheck->setChecked(initialSettings.confirmOnMultiPaneExit);
    m_warnStartupScriptsCheck->setChecked(initialSettings.warnOnLayoutStartupScripts);
    rootLayout->addWidget(m_confirmExitCheck);
    rootLayout->addWidget(m_warnStartupScriptsCheck);
    rootLayout->addStretch(1);

    auto* buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    connect(buttonBox, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);

    rootLayout->addWidget(buttonBox);
}

AppSettings SettingsDialog::settings() const {
    AppSettings result;
    result.terminalColorScheme = m_colorSchemeCombo->currentText();
    result.defaultShell = m_shellEdit->text().trimmed();
    result.maxPanes = m_maxPanesSpin->value();
    result.confirmOnMultiPaneExit = m_confirmExitCheck->isChecked();
    result.warnOnLayoutStartupScripts = m_warnStartupScriptsCheck->isChecked();
    return result;
}
