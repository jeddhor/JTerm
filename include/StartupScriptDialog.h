#pragma once

#include <QDialog>

class QLabel;
class QCheckBox;
class QPlainTextEdit;
class QPushButton;

class StartupScriptDialog : public QDialog {
    Q_OBJECT

public:
    explicit StartupScriptDialog(const QString& paneTitle, const QString& initialScript, bool initialThrottleEnabled, QWidget* parent = nullptr);

    QString script() const;
    bool throttleEnabled() const;

private slots:
    void saveAndClose();

private:
    QPlainTextEdit* m_editor;
    QLabel* m_infoLabel;
    QCheckBox* m_throttleCheck;
    QPushButton* m_saveButton;
    QString m_script;
    bool m_throttleEnabled;
};
