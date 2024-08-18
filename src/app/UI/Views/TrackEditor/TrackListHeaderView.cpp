//
// Created by fluty on 2024/2/5.
//

#include "TrackListHeaderView.h"

#include "Controller/TracksViewController.h"
#include "Global/TracksEditorGlobal.h"
#include "UI/Controls/ToolTipFilter.h"

#include <QHBoxLayout>
#include <QPainter>
#include <QPushButton>

TrackListHeaderView::TrackListHeaderView(QWidget *parent) : QWidget(parent) {
    setObjectName("trackListHeaderView");
    setFixedHeight(TracksEditorGlobal::trackViewHeaderHeight);
    setFixedWidth(TracksEditorGlobal::trackListWidth);

    auto btnNewTrack = new QPushButton;
    btnNewTrack->setObjectName("btnNewTrack");
    btnNewTrack->setFixedSize(24, 24);
    btnNewTrack->setToolTip(tr("New Track"));
    btnNewTrack->installEventFilter(new ToolTipFilter(btnNewTrack));
    // btnNewTrack->setText(tr("New Track"));
    connect(btnNewTrack, &QPushButton::clicked, trackController, &TracksViewController::onNewTrack);
    auto mainLayout = new QHBoxLayout;
    mainLayout->addSpacerItem(new QSpacerItem(20, 20, QSizePolicy::Expanding));
    mainLayout->addWidget(btnNewTrack);
    mainLayout->setContentsMargins(6, 6, 6, 6);

    setLayout(mainLayout);
}

void TrackListHeaderView::paintEvent(QPaintEvent *event) {
    // QPainter painter(this);
    // painter.setPen(Qt::NoPen);
    // painter.fillRect(rect(), QColor(42, 43, 44));
    QWidget::paintEvent(event);
}