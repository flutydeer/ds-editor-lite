//
// Created by fluty on 2024/7/11.
//

#include "TrackListView.h"

#include "TracksGraphicsView.h"
#include "Global/TracksEditorGlobal.h"

#include <QScroller>
#include <QWheelEvent>

TrackListView::TrackListView(QWidget *parent) : QListWidget(parent) {
    setObjectName("TrackListWidget");
    setFixedWidth(TracksEditorGlobal::trackListWidth);
    setViewMode(ListMode);
    setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    setVerticalScrollMode(ScrollPerPixel);
    setSelectionMode(SingleSelection);
    QScroller::grabGesture(this, QScroller::TouchGesture);
}

void TrackListView::setGraphicsView(TracksGraphicsView *view) {
    m_view = view;
}

void TrackListView::mousePressEvent(QMouseEvent *event) {
    QListWidget::mousePressEvent(event);
    event->ignore();
}

void TrackListView::wheelEvent(QWheelEvent *event) {
    if (!m_view)
        return;

    auto modifiers = event->modifiers();
    if (modifiers == Qt::AltModifier)
        m_view->onWheelVerScale(event);
    else if (modifiers == Qt::NoModifier)
        m_view->onWheelVerScroll(event);

    // if (modifiers == Qt::AltModifier || modifiers == Qt::NoModifier) {
    //     if (targetWidget) {
    //         auto localPos = event->position().toPoint();
    //         auto globalPos = targetWidget->mapToGlobal(localPos);
    //         auto e = new QWheelEvent(localPos, globalPos, event->pixelDelta(),
    //         event->angleDelta(),
    //                                  event->buttons(), event->modifiers(), event->phase(),
    //                                  event->inverted(), event->source());
    //         qDebug() << "post event" << localPos << globalPos;
    //         QCoreApplication::postEvent(targetWidget, e);
    //     }
    // }
    // QListWidget::wheelEvent(event);
}