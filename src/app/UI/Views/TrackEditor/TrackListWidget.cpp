//
// Created by fluty on 2024/7/11.
//

#include "TrackListWidget.h"

#include <QScroller>
#include <QMouseEvent>

#include "Global/TracksEditorGlobal.h"

TrackListWidget::TrackListWidget(QWidget *parent) : QListWidget(parent) {

    setObjectName("TrackListWidget");
    setMinimumWidth(TracksEditorGlobal::trackListWidth);
    setMaximumWidth(TracksEditorGlobal::trackListWidth);
    setViewMode(QListView::ListMode);
    setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    setVerticalScrollMode(QListWidget::ScrollPerPixel);
    setStyleSheet("QListWidget#TrackListWidget { background: transparent; border: none; "
                  "border-right: 1px solid #202020; outline:0px;"
                  "border-top: 1px solid #202020;"
                  "margin-bottom: 16px } "
                  "QListWidget::item { border-radius: 0px; padding: 0px; }"
                  "QListWidget::item:hover { background: #05FFFFFF }"
                  "QListWidget::item:selected { background: #10FFFFFF }");
    setSelectionMode(QAbstractItemView::SingleSelection);
    installEventFilter(this);
    QScroller::grabGesture(this, QScroller::TouchGesture);
}

void TrackListWidget::mousePressEvent(QMouseEvent *event) {
    QListWidget::mousePressEvent(event);
    event->ignore();
}