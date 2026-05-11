#include "SettingsDialog.h"

#include <QDialogButtonBox>
#include <QFormLayout>
#include <QLineEdit>
#include <QSpinBox>
#include <QVBoxLayout>

SettingsDialog::SettingsDialog(const AppSettings& initialSettings, QWidget* parent)
    : QDialog(parent)
    , m_shellEdit(new QLineEdit(this))
    , m_maxPanesSpin(new QSpinBox(this)) {
    setWindowTitle(QStringLiteral("Settings"));
    resize(460, 240);

    auto* rootLayout = new QVBoxLayout(this);

    auto* formLayout = new QFormLayout();
    formLayout->setLabelAlignment(Qt::AlignLeft);

    m_shellEdit->setText(initialSettings.defaultShell);
    m_shellEdit->setPlaceholderText(QStringLiteral("Default shell executable path"));

    m_maxPanesSpin->setMinimum(2);
    m_maxPanesSpin->setMaximum(128);
    m_maxPanesSpin->setValue(initialSettings.maxPanes);

    formLayout->addRow(QStringLiteral("Default shell:"), m_shellEdit);
    formLayout->addRow(QStringLiteral("Maximum panes:"), m_maxPanesSpin);

    rootLayout->addLayout(formLayout);
    rootLayout->addStretch(1);

    auto* buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    connect(buttonBox, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);

    rootLayout->addWidget(buttonBox);
}

AppSettings SettingsDialog::settings() const {
    AppSettings result;
    result.defaultShell = m_shellEdit->text().trimmed();
    result.maxPanes = m_maxPanesSpin->value();
    return result;
}
