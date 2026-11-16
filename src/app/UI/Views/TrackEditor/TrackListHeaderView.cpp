#include "TrackListHeaderView.h"

#include "Controller/TrackController.h"
#include "Global/TracksEditorGlobal.h"
#include <lite/GUI/Controls/Menu.h>
#include <lite/GUI/Controls/ToolButton.h>
#include <lite/GUI/Controls/ToolTipFilter.h>

#include <QHBoxLayout>
#include <QAction>
#include <QEvent>
#include <QMenu>
#include <QPainter>
#include <QPushButton>
#include <QSignalBlocker>

TrackListHeaderView::TrackListHeaderView(QWidget *parent) : QWidget(parent) {
    setAttribute(Qt::WA_StyledBackground);
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

    // Both info lanes are toggled from a single overflow-style menu instead of
    // dedicating one toolbar slot to each of them.
    m_laneMenu = new Menu(this);
    m_laneMenu->setObjectName("laneVisibilityMenu");

    // The seconds axis sits directly under the bar/beat ruler, so it leads the
    // menu the same way it leads the row order.
    m_actionTimeLane = m_laneMenu->addAction(tr("Time (s) Track"));
    m_actionTimeLane->setObjectName("actionTimeLane");
    m_actionTimeLane->setCheckable(true);
    m_actionTimeLane->setChecked(false);

    m_actionTempoLane = m_laneMenu->addAction(tr("Tempo Track"));
    m_actionTempoLane->setObjectName("actionTempoLane");
    m_actionTempoLane->setCheckable(true);
    m_actionTempoLane->setChecked(true);

    m_actionTimeSignatureLane = m_laneMenu->addAction(tr("Time Signature Track"));
    m_actionTimeSignatureLane->setObjectName("actionTimeSignatureLane");
    m_actionTimeSignatureLane->setCheckable(true);
    m_actionTimeSignatureLane->setChecked(true);

    connect(m_actionTempoLane, &QAction::toggled, this,
            &TrackListHeaderView::tempoLaneToggled);
    connect(m_actionTimeSignatureLane, &QAction::toggled, this,
            &TrackListHeaderView::timeSignatureLaneToggled);
    connect(m_actionTimeLane, &QAction::toggled, this, &TrackListHeaderView::timeLaneToggled);

    const auto btnLaneOptions = new ToolButton;
    m_btnLaneOptions = btnLaneOptions;
    btnLaneOptions->setObjectName("btnLaneOptions");
    btnLaneOptions->setFixedSize(28, 28);
    btnLaneOptions->setActionIcon(
        QStringLiteral(":/svg/icons/more_horizontal_24_regular.svg"));
    const auto laneVisibilityText = tr("Lane Visibility");
    btnLaneOptions->setToolTip(laneVisibilityText);
    btnLaneOptions->setAccessibleName(laneVisibilityText);
    btnLaneOptions->installEventFilter(new ToolTipFilter(btnLaneOptions));
    btnLaneOptions->setMenu(m_laneMenu);

    const auto mainLayout = new QHBoxLayout;
    mainLayout->addWidget(btnNewTrack);
    mainLayout->addSpacerItem(new QSpacerItem(20, 20, QSizePolicy::Expanding));
    mainLayout->addWidget(btnLaneOptions);
    mainLayout->setContentsMargins(4, 4, 4, 4);

    setLayout(mainLayout);
}

bool TrackListHeaderView::tempoLaneVisible() const {
    return m_actionTempoLane->isChecked();
}

bool TrackListHeaderView::timeSignatureLaneVisible() const {
    return m_actionTimeSignatureLane->isChecked();
}

bool TrackListHeaderView::timeLaneVisible() const {
    return m_actionTimeLane->isChecked();
}

void TrackListHeaderView::paintEvent(QPaintEvent *event) {
    QWidget::paintEvent(event);
}

void TrackListHeaderView::changeEvent(QEvent *event) {
    QWidget::changeEvent(event);
    if (event->type() == QEvent::LanguageChange) {
        m_btnNewTrack->setToolTip(tr("New Track"));
        const auto laneVisibilityText = tr("Lane Visibility");
        m_btnLaneOptions->setToolTip(laneVisibilityText);
        m_btnLaneOptions->setAccessibleName(laneVisibilityText);
        m_actionTempoLane->setText(tr("Tempo Track"));
        m_actionTimeSignatureLane->setText(tr("Time Signature Track"));
        m_actionTimeLane->setText(tr("Time (s) Track"));
    }
}
