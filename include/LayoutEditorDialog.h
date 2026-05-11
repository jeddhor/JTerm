#pragma once

#include <QDialog>
#include <QJsonObject>

class QLabel;
class QPlainTextEdit;
class QPushButton;
class QTimer;

class LayoutEditorDialog : public QDialog {
    Q_OBJECT

public:
    explicit LayoutEditorDialog(const QJsonObject& initialLayout, QWidget* parent = nullptr);

    QJsonObject layoutObject() const;

private slots:
    void validateJson();
    void applyAndClose();
    void saveToFile();
    void formatJson();
    void scheduleValidation();
    void validateJsonLive();

private:
    bool parseEditorJson(QJsonObject* parsedObject, QString* errorMessage, qsizetype* errorOffset = nullptr) const;
    void setStatusMessage(const QString& text, bool isError, bool isWarning = false);
    QString validationHintForLayout(const QJsonObject& rootObject) const;
    static QPair<int, int> lineColumnFromOffset(const QString& text, qsizetype offset);
    void moveCursorToOffset(qsizetype offset);
    void clearErrorHighlights();
    void setErrorHighlightAtOffset(qsizetype offset);

    QPlainTextEdit* m_editor;
    QLabel* m_statusLabel;
    QPushButton* m_validateButton;
    QPushButton* m_formatButton;
    QPushButton* m_applyButton;
    QPushButton* m_saveButton;
    QTimer* m_liveValidationTimer;
    QJsonObject m_layoutObject;
};
