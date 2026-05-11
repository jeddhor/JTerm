#include "SnapSplitter.h"

#include <QPoint>
#include <QSplitterHandle>
#include <QtGlobal>

SnapSplitter::SnapSplitter(Qt::Orientation orientation, QWidget* snapScope, QWidget* parent)
    : QSplitter(orientation, parent)
    , m_snapScope(snapScope) {}

void SnapSplitter::setSnapScope(QWidget* snapScope) {
    m_snapScope = snapScope;
}

void SnapSplitter::moveSplitter(int pos, int index) {
    const int snappedPos = maybeSnapPosition(pos, index);
    QSplitter::moveSplitter(snappedPos, index);
}

int SnapSplitter::snapThresholdPx() const {
    return 10;
}

int SnapSplitter::maybeSnapPosition(int pos, int index) const {
    if (!m_snapScope) {
        return pos;
    }

    const bool isHorizontal = orientation() == Qt::Horizontal;
    const int proposedGlobal = isHorizontal
        ? mapToGlobal(QPoint(pos, 0)).x()
        : mapToGlobal(QPoint(0, pos)).y();

    int bestTargetGlobal = proposedGlobal;
    int bestDistance = snapThresholdPx() + 1;

    const QList<QSplitter*> splitters = m_snapScope->findChildren<QSplitter*>();
    for (QSplitter* splitter : splitters) {
        if (!splitter || splitter->orientation() != orientation()) {
            continue;
        }

        for (int handleIndex = 1; handleIndex < splitter->count(); ++handleIndex) {
            QSplitterHandle* handle = splitter->handle(handleIndex);
            if (!handle || !handle->isVisible()) {
                continue;
            }

            const QPoint handleCenter = handle->rect().center();
            const int targetGlobal = isHorizontal
                ? handle->mapToGlobal(handleCenter).x()
                : handle->mapToGlobal(handleCenter).y();

            const int distance = qAbs(targetGlobal - proposedGlobal);
            if (distance > snapThresholdPx() || distance >= bestDistance) {
                continue;
            }

            if (splitter == this && handleIndex == index) {
                continue;
            }

            bestDistance = distance;
            bestTargetGlobal = targetGlobal;
        }
    }

    if (bestDistance > snapThresholdPx()) {
        return pos;
    }

    return isHorizontal
        ? mapFromGlobal(QPoint(bestTargetGlobal, 0)).x()
        : mapFromGlobal(QPoint(0, bestTargetGlobal)).y();
}
