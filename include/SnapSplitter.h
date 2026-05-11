#pragma once

#include <QPointer>
#include <QSplitter>

class SnapSplitter : public QSplitter {
    Q_OBJECT

public:
    explicit SnapSplitter(Qt::Orientation orientation, QWidget* snapScope, QWidget* parent = nullptr);
    void setSnapScope(QWidget* snapScope);

protected:
    void moveSplitter(int pos, int index) override;

private:
    int snapThresholdPx() const;
    int maybeSnapPosition(int pos, int index) const;

    QPointer<QWidget> m_snapScope;
};
