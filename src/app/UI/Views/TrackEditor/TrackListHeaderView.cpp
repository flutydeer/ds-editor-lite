//
// Created by fluty on 2024/2/5.
//

#include "TrackListHeaderView.h"

#include "Controller/TrackController.h"
#include "Global/TracksEditorGlobal.h"
#include "UI/Controls/ToolButton.h"
#include "UI/Controls/ToolTipFilter.h"

#include <QHBoxLayout>
#include <QEvent>
#include <QPainter>
#include <QPushButton>

TrackListHeaderView::TrackListHeaderView(QWidget *parent) : QWidget(parent) {
    setObjectName("trackListHeaderView");
    setFixedHeight(TracksEditorGlobal::trackViewHeaderHeight);

    const auto btnNewTrack = new ToolButton;
    m_btnNewTrack = btnNewTrack;
    btnNewTrack->setObjectName("btnNewTrack");
    btnNewTrack->setFixedSize(28, 28);
    btnNewTrack->setActionIcon(QStringLiteral(":/svg/icons/add_16_regular.svg"));
    btnNewTrack->setToolTip(tr("New Track"));
    btnNewTrack->installEventFilter(new ToolTipFilter(btnNewTrack));
    connect(btnNewTrack, &QPushButton::clicked, trackController, &TrackController::onNewTrack);
    const auto mainLayout = new QHBoxLayout;
    mainLayout->addSpacerItem(new QSpacerItem(20, 20, QSizePolicy::Expanding));
    mainLayout->addWidget(btnNewTrack);
    mainLayout->setContentsMargins(4, 4, 4, 4);

    setLayout(mainLayout);
}

void TrackListHeaderView::paintEvent(QPaintEvent *event) {
    QWidget::paintEvent(event);
}

void TrackListHeaderView::changeEvent(QEvent *event) {
    QWidget::changeEvent(event);
    if (event->type() == QEvent::LanguageChange)
        m_btnNewTrack->setToolTip(tr("New Track"));
}
