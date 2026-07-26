//
// Created by fluty on 2024/2/5.
//

#include "TrackListHeaderView.h"

#include "Controller/TrackController.h"
#include "Global/TracksEditorGlobal.h"
#include <lite/GUI/Controls/ToolButton.h>
#include <lite/GUI/Controls/ToolTipFilter.h>

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

    const auto btnToggleTimeSignatureLane = new ToolButton;
    m_btnToggleTimeSignatureLane = btnToggleTimeSignatureLane;
    btnToggleTimeSignatureLane->setObjectName("btnToggleTimeSignatureLane");
    btnToggleTimeSignatureLane->setFixedSize(28, 28);
    btnToggleTimeSignatureLane->setCheckable(true);
    btnToggleTimeSignatureLane->setChecked(true);
    btnToggleTimeSignatureLane->setToolTip(tr("Show Time Signature Track"));
    btnToggleTimeSignatureLane->installEventFilter(
        new ToolTipFilter(btnToggleTimeSignatureLane));
    connect(btnToggleTimeSignatureLane, &QPushButton::toggled, this,
            &TrackListHeaderView::timeSignatureLaneToggled);
    rebuildToggleIcons();

    const auto mainLayout = new QHBoxLayout;
    mainLayout->addWidget(btnNewTrack);
    mainLayout->addSpacerItem(new QSpacerItem(20, 20, QSizePolicy::Expanding));
    mainLayout->addWidget(btnToggleTimeSignatureLane);
    mainLayout->setContentsMargins(4, 4, 4, 4);

    setLayout(mainLayout);
}

bool TrackListHeaderView::timeSignatureLaneVisible() const {
    return m_btnToggleTimeSignatureLane->isChecked();
}

void TrackListHeaderView::paintEvent(QPaintEvent *event) {
    QWidget::paintEvent(event);
}

void TrackListHeaderView::changeEvent(QEvent *event) {
    QWidget::changeEvent(event);
    if (event->type() == QEvent::LanguageChange) {
        m_btnNewTrack->setToolTip(tr("New Track"));
        m_btnToggleTimeSignatureLane->setToolTip(tr("Show Time Signature Track"));
    }
}

QColor TrackListHeaderView::iconCheckedColor() const {
    return m_iconCheckedColor;
}

void TrackListHeaderView::setIconCheckedColor(const QColor &color) {
    if (m_iconCheckedColor == color)
        return;
    m_iconCheckedColor = color;
    rebuildToggleIcons();
}

void TrackListHeaderView::rebuildToggleIcons() {
    const auto button = qobject_cast<ToolButton *>(m_btnToggleTimeSignatureLane);
    if (!button)
        return;
    // Placeholder icon until the lane toggles get a dedicated design
    button->setToggleIcon(QStringLiteral(":/svg/icons/music_note_2_16_filled.svg"), QSize(16, 16),
                          m_iconCheckedColor);
}
