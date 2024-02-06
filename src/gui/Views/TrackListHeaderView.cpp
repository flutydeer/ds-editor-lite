//
// Created by fluty on 2024/2/5.
//

#include <QPushButton>
#include <QHBoxLayout>
#include <QPainter>

#include "TrackListHeaderView.h"
#include "Controller/TracksViewController.h"

TrackListHeaderView::TrackListHeaderView(QWidget *parent) {
    setObjectName("trackListHeaderView");

    auto mainLayout = new QHBoxLayout;
    // mainLayout->addWidget();
    mainLayout->setContentsMargins(6, 6, 6, 6);

    setLayout(mainLayout);
}
void TrackListHeaderView::paintEvent(QPaintEvent *event) {
    QPainter painter(this);
    painter.setPen(Qt::NoPen);
    painter.fillRect(rect(), QColor(42, 43, 44));
    QWidget::paintEvent(event);
}