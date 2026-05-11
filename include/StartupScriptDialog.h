#pragma once

#include <QDialog>

class QLabel;
class QPlainTextEdit;
class QPushButton;

class StartupScriptDialog : public QDialog {
    Q_OBJECT

public:
    explicit StartupScriptDialog(const QString& paneTitle, const QString& initialScript, QWidget* parent = nullptr);

    QString script() const;

private slots:
    void applyAndClose();

private:
    QPlainTextEdit* m_editor;
    QLabel* m_infoLabel;
    QPushButton* m_applyButton;
    QString m_script;
};
