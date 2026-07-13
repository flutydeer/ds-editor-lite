//
// Created by fluty on 2024/2/4.
//

#include "PlaybackView.h"

#include "MainTitleBarIconPalette.h"
#include "UI/Controls/ControlGroup.h"
#include "Controller/AppController.h"
#include "Controller/PlaybackController.h"
#include "Model/AppModel/AppModel.h"
#include "Model/AppStatus/AppStatus.h"
#include "UI/Controls/InlineEditLabel.h"
#include "UI/Utils/IconUtils.h"
#include "Utils/FontManager.h"
#include "TimeSignatureComboBox.h"
#include "Global/AppGlobal.h"

#include <QColor>
#include <QHBoxLayout>
#include <QIcon>
#include <QLabel>
#include <QPushButton>
#include <QShortcut>

#include <cmath>

namespace {
    QColor playAccentColor() {
        return QColor(155, 186, 255, 255);
    }

    QColor pauseAccentColor() {
        return QColor(255, 205, 155, 255);
    }

    QIcon buildActionIcon(const QString &svgPath, const QSize &iconSize) {
        return IconUtils::createTintedSvgIcon(svgPath, iconSize,
                                              MainTitleBarIconPalette::actionPalette());
    }

    QIcon buildToggleIcon(const QString &svgPath, const QSize &iconSize,
                          const QColor &checkedColor) {
        return IconUtils::createTintedSvgIcon(
            svgPath, iconSize, MainTitleBarIconPalette::toggledPalette(checkedColor));
    }
}

PlaybackView::PlaybackView(QWidget *parent) : QWidget(parent) {
    setAttribute(Qt::WA_StyledBackground);

    // Apply music font to time-related UI elements
    const QFont musicFont = FontManager::instance().musicUIFont(13);

    m_elTempo = new InlineEditLabel;
    m_elTempo->setObjectName("elTempo");
    m_elTempo->setEditRole(InlineEditLabel::Tempo);
    m_elTempo->setText(QString::number(m_tempo));
    m_elTempo->setAlignment(Qt::AlignCenter);
    m_elTempo->setDisplayFont(musicFont);
    m_elTempo->setTextMargins({12, 0, 12, 0});
    m_elTempo->setFixedHeight(m_contentHeight);
    m_elTempo->setCommitValidator([](const QString &text) {
        bool ok = false;
        const auto value = text.trimmed().toDouble(&ok);
        return ok && std::isfinite(value) && value > 0.0;
    });

    m_elTimeSignature = new TimeSignatureComboBox;
    m_elTimeSignature->setObjectName("elTimeSignature");
    m_elTimeSignature->setEditRole(InlineEditLabel::TimeSignature);
    m_elTimeSignature->setAlignment(Qt::AlignCenter);
    m_elTimeSignature->setTimeSignature(m_numerator, m_denominator);
    m_elTimeSignature->setDisplayFont(musicFont);
    m_elTimeSignature->setTextMargins({12, 0, 12, 0});
    m_elTimeSignature->setFixedHeight(m_contentHeight);
    m_elTimeSignature->setCommitValidator([](const QString &text) {
        const auto parts = text.trimmed().split('/');
        if (parts.size() != 2)
            return false;
        bool numeratorOk = false;
        bool denominatorOk = false;
        const auto numerator = parts.at(0).toInt(&numeratorOk);
        const auto denominator = parts.at(1).toInt(&denominatorOk);
        return numeratorOk && denominatorOk && numerator > 0 && denominator > 0 &&
               (denominator & (denominator - 1)) == 0;
    });

    m_btnStop = new QPushButton;
    m_btnStop->setObjectName("btnStop");
    m_btnStop->setIconSize(m_iconSize);
    m_btnStop->setIcon(buildActionIcon(":svg/icons/stop_16_regular.svg", m_iconSize));

    m_btnPlay = new QPushButton;
    m_btnPlay->setObjectName("btnPlay");
    m_btnPlay->setIconSize(m_iconSize);
    m_btnPlay->setIcon(
        buildToggleIcon(":svg/icons/play_16_regular.svg", m_iconSize, playAccentColor()));
    m_btnPlay->setCheckable(true);

    m_btnPlayPause = new QPushButton(this);
    connect(m_btnPlayPause, &QPushButton::pressed, this, [this] {
        if (m_status == Paused || m_status == Stopped)
            playTriggered();
        else if (m_status == Playing)
            pauseTriggered();
    });
    m_btnPlayPause->setFixedSize(0, 0);

    auto playPauseShortcut = new QShortcut(Qt::Key_Space, this);
    playPauseShortcut->setContext(Qt::ApplicationShortcut);
    connect(playPauseShortcut, &QShortcut::activated, this, [this] {
        if (m_status == Paused || m_status == Stopped)
            playTriggered();
        else if (m_status == Playing)
            pauseTriggered();
    });

    m_btnLoop = new QPushButton;
    m_btnLoop->setObjectName("btnLoop");
    m_btnLoop->setIconSize(m_iconSize);
    m_btnLoop->setIcon(buildToggleIcon(":svg/icons/arrow_repeat_all_16_regular.svg", m_iconSize,
                                       playAccentColor()));
    m_btnLoop->setCheckable(true);
    m_btnLoop->setToolTip(tr("Loop"));
    auto loopShortcut = new QShortcut(QKeySequence(Qt::ALT | Qt::Key_L), this);
    loopShortcut->setContext(Qt::ApplicationShortcut);
    connect(loopShortcut, &QShortcut::activated, m_btnLoop, &QPushButton::toggle);
    connect(m_btnLoop, &QPushButton::clicked, this, [this](bool checked) {
        auto settings = appStatus->loopSettings.get();
        settings.enabled = checked;

        // Initialize loop region based on current position when enabling
        if (checked && settings.length == 0) {
            int currentTick = static_cast<int>(playbackController->position());
            int barTicks = AppGlobal::ticksPerWholeNote * m_numerator / m_denominator;

            // Snap to bar line
            int startBar = currentTick / barTicks;
            settings.start = startBar * barTicks;
            settings.length = barTicks; // One bar length
        }

        appStatus->loopSettings.set(settings);
        updateLoopButtonView();
    });

    m_btnPause = new QPushButton;
    m_btnPause->setObjectName("btnPause");
    m_btnPause->setIconSize(m_iconSize);
    m_btnPause->setIcon(
        buildToggleIcon(":svg/icons/pause_16_regular.svg", m_iconSize, pauseAccentColor()));
    m_btnPause->setCheckable(true);

    m_elTime = new InlineEditLabel;
    m_elTime->setObjectName("elTime");
    m_elTime->setEditRole(InlineEditLabel::PlaybackPosition);
    m_elTime->setAlignment(Qt::AlignCenter);
    m_elTime->setText(toFormattedTickTime(m_tick));
    m_elTime->setDisplayFont(musicFont);
    m_elTime->setTextMargins({12, 0, 12, 0});
    m_elTime->setFixedHeight(m_contentHeight);
    m_elTime->setCommitValidator([this](const QString &text) {
        const auto parts = text.trimmed().split(':');
        if (parts.size() != 3)
            return false;
        bool barOk = false;
        bool beatOk = false;
        bool tickOk = false;
        const auto bar = parts.at(0).toInt(&barOk);
        const auto beat = parts.at(1).toInt(&beatOk);
        const auto tick = parts.at(2).toInt(&tickOk);
        const auto beatTicks = AppGlobal::ticksPerWholeNote / m_denominator;
        return barOk && beatOk && tickOk && bar >= 1 && beat >= 1 && beat <= m_numerator &&
               tick >= 0 && tick < beatTicks;
    });

    connect(m_elTempo, &InlineEditLabel::editCompleted, this, [this](const QString &value) {
        auto tempo = value.toDouble();
        if (m_tempo != tempo) {
            m_tempo = tempo;
            emit setTempoTriggered(tempo);
        }
        updateTempoView();
    });

    connect(m_elTimeSignature, &TimeSignatureComboBox::timeSignatureChanged, this,
            [this](int numerator, int denominator) {
                if (m_numerator != numerator || m_denominator != denominator) {
                    m_numerator = numerator;
                    m_denominator = denominator;
                    emit setTimeSignatureTriggered(numerator, denominator);
                }
                updateTimeSignatureView();
            });
    connect(m_elTime, &InlineEditLabel::editCompleted, this, [this](const QString &value) {
        if (!value.contains(':'))
            return;

        auto splitStr = value.split(':');
        if (splitStr.size() != 3)
            return;

        auto tick = fromTickTimeString(splitStr);
        if (tick != m_tick) {
            m_tick = tick;
            emit setPositionTriggered(tick);
        }
        updateTimeView();
    });
    connect(m_btnPlay, &QPushButton::clicked, this, [this] {
        emit playTriggered();
        updatePlaybackControlView();
    });
    connect(m_btnPause, &QPushButton::clicked, this, [this] {
        emit pauseTriggered();
        updatePlaybackControlView();
    });
    connect(m_btnStop, &QPushButton::clicked, this, [this] {
        emit stopTriggered();
        updatePlaybackControlView();
    });

    auto transportLayout = new QHBoxLayout;
    transportLayout->addWidget(m_btnStop);
    transportLayout->addWidget(m_btnPlay);
    transportLayout->addWidget(m_btnPause);
    transportLayout->addWidget(m_btnLoop);
    transportLayout->addWidget(m_elTime);
    transportLayout->setSpacing(1);
    transportLayout->setContentsMargins({});

    auto transportWidget = new ControlGroup;
    transportWidget->setLayout(transportLayout);

    auto scoreGlobalLayout = new QHBoxLayout;
    scoreGlobalLayout->addWidget(m_elTempo);
    scoreGlobalLayout->addWidget(m_elTimeSignature);
    scoreGlobalLayout->setSpacing(1);
    scoreGlobalLayout->setContentsMargins({});

    auto scoreGlobalWidget = new ControlGroup;
    scoreGlobalWidget->setLayout(scoreGlobalLayout);

    auto mainLayout = new QHBoxLayout;
    mainLayout->addWidget(transportWidget);
    mainLayout->addWidget(scoreGlobalWidget);
    mainLayout->setContentsMargins({});
    mainLayout->setSpacing(6);
    setLayout(mainLayout);

    setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);

    connect(this, &PlaybackView::setTempoTriggered, appController, &AppController::onSetTempo);
    connect(this, &PlaybackView::setTimeSignatureTriggered, appController,
            &AppController::onSetTimeSignature);
    connect(this, &PlaybackView::playTriggered, playbackController, &PlaybackController::play);
    connect(this, &PlaybackView::pauseTriggered, playbackController, &PlaybackController::pause);
    connect(this, &PlaybackView::stopTriggered, playbackController, &PlaybackController::stop);
    connect(this, &PlaybackView::setPositionTriggered, playbackController, [](int tick) {
        playbackController->setLastPosition(tick);
        playbackController->setPosition(tick);
    });
    connect(playbackController, &PlaybackController::playbackStatusChanged, this,
            &PlaybackView::onPlaybackStatusChanged);
    connect(playbackController, &PlaybackController::positionChanged, this,
            &PlaybackView::onPositionChanged);
    connect(appModel, &AppModel::modelChanged, this, &PlaybackView::updateView);
    connect(appModel, &AppModel::tempoChanged, this, &PlaybackView::onTempoChanged);
    connect(appModel, &AppModel::timeSignatureChanged, this, &PlaybackView::onTimeSignatureChanged);
    connect(appStatus, &AppStatus::loopSettingsChanged, this, [this] { updateLoopButtonView(); });
}

void PlaybackView::updateView() {
    m_tempo = appModel->tempo();
    m_numerator = appModel->timeSignature().numerator;
    m_denominator = appModel->timeSignature().denominator;
    m_tick = static_cast<int>(playbackController->position());
    m_status = playbackController->playbackStatus();

    updateTempoView();
    updateTimeSignatureView();
    updateTimeView();
    updatePlaybackControlView();
    updateLoopButtonView();
}

void PlaybackView::onTempoChanged(double tempo) {
    m_tempo = tempo;
    updateTempoView();
}

void PlaybackView::onTimeSignatureChanged(int numerator, int denominator) {
    m_numerator = numerator;
    m_denominator = denominator;
    updateTimeSignatureView();
    updateTimeView();
}

void PlaybackView::onPositionChanged(double tick) {
    m_tick = static_cast<int>(tick);
    updateTimeView();
}

void PlaybackView::onPlaybackStatusChanged(PlaybackStatus status) {
    m_status = status;
    updatePlaybackControlView();
}

QString PlaybackView::toFormattedTickTime(int ticks) const {
    int barTicks = AppGlobal::ticksPerWholeNote * m_numerator / m_denominator;
    int beatTicks = AppGlobal::ticksPerWholeNote / m_denominator;
    auto bar = ticks / barTicks + 1;
    auto beat = ticks % barTicks / beatTicks + 1;
    auto tick = ticks % barTicks % beatTicks;
    auto str = QString::asprintf("%03d", bar) + ":" + QString::asprintf("%02d", beat) + ":" +
               QString::asprintf("%03d", tick);
    return str;
}

int PlaybackView::fromTickTimeString(const QStringList &splitStr) const {
    auto bar = splitStr.at(0).toInt();
    auto beat = splitStr.at(1).toInt();
    auto tick = splitStr.at(2).toInt();
    return (bar - 1) * AppGlobal::ticksPerWholeNote * m_numerator / m_denominator +
           (beat - 1) * AppGlobal::ticksPerWholeNote / m_denominator + tick;
}

void PlaybackView::updateTempoView() {
    m_elTempo->setText(QString::number(m_tempo));
}

void PlaybackView::updateTimeSignatureView() {
    m_elTimeSignature->setTimeSignature(m_numerator, m_denominator);
}

void PlaybackView::updateTimeView() {
    m_elTime->setText(toFormattedTickTime(m_tick));
}

void PlaybackView::updatePlaybackControlView() {
    if (m_status == Playing) {
        m_btnPlay->setChecked(true);

        m_btnPause->setChecked(false);
    } else if (m_status == Paused) {
        m_btnPlay->setChecked(false);

        m_btnPause->setChecked(true);
    } else {
        m_btnPlay->setChecked(false);

        m_btnPause->setChecked(false);
    }
}

void PlaybackView::updateLoopButtonView() {
    const bool enabled = appStatus->loopSettings.get().enabled;
    m_btnLoop->setChecked(enabled);
}
