#pragma once

#include <QPointer>
#include <QSplitter>

class SnapSplitter : public QSplitter {
    Q_OBJECT

public:
    explicit SnapSplitter(Qt::Orientation orientation, QWidget* snapScope, QWidget* parent = nullptr);
    void setSnapScope(QWidget* snapScope);

private:
    void onSplitterMoved(int pos, int index);
    int snapThresholdPx() const;
    int maybeSnapPosition(int pos, int index) const;

    bool m_isApplyingSnap = false;
    QPointer<QWidget> m_snapScope;
};
