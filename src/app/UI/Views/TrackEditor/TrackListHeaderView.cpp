//
// Created by fluty on 2024/2/5.
//

#include "TrackListHeaderView.h"

#include "Controller/TrackController.h"
#include "Global/TracksEditorGlobal.h"
#include "UI/Controls/ToolTipFilter.h"
#include "UI/Utils/IconUtils.h"

#include <QHBoxLayout>
#include <QPainter>
#include <QPushButton>

TrackListHeaderView::TrackListHeaderView(QWidget *parent) : QWidget(parent) {
    setObjectName("trackListHeaderView");
    setFixedHeight(TracksEditorGlobal::trackViewHeaderHeight);

    const auto btnNewTrack = new QPushButton;
    btnNewTrack->setObjectName("btnNewTrack");
    btnNewTrack->setFixedSize(24, 24);
    const auto iconColor = btnNewTrack->palette().color(QPalette::ButtonText);
    btnNewTrack->setIcon(IconUtils::createTintedSvgIcon(
        QStringLiteral(":/svg/icons/add_16_filled.svg"), QSize(16, 16), iconColor, iconColor));
    btnNewTrack->setIconSize(QSize(16, 16));
    btnNewTrack->setToolTip(tr("New Track"));
    btnNewTrack->installEventFilter(new ToolTipFilter(btnNewTrack));
    connect(btnNewTrack, &QPushButton::clicked, trackController, &TrackController::onNewTrack);
    const auto mainLayout = new QHBoxLayout;
    mainLayout->addSpacerItem(new QSpacerItem(20, 20, QSizePolicy::Expanding));
    mainLayout->addWidget(btnNewTrack);
    mainLayout->setContentsMargins(6, 6, 6, 6);

    setLayout(mainLayout);
}

void TrackListHeaderView::paintEvent(QPaintEvent *event) {
    QWidget::paintEvent(event);
}
