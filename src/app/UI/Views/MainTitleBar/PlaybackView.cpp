//
// Created by fluty on 2024/2/4.
//


#include "PlaybackView.h"

#include "TitleControlGroup.h"
#include "Controller/AppController.h"
#include "Controller/PlaybackController.h"
#include "Model/AppModel/AppModel.h"
#include "UI/Controls/ComboBox.h"
#include "UI/Controls/EditLabel.h"
#include "UI/Controls/LineEdit.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>

PlaybackView::PlaybackView(QWidget *parent) : QWidget(parent) {
    setAttribute(Qt::WA_StyledBackground);
    m_elTempo = new EditLabel;
    m_elTempo->setObjectName("elTempo");
    m_elTempo->setText(QString::number(m_tempo));
    m_elTempo->label->setAlignment(Qt::AlignCenter);

    m_elTimeSignature = new EditLabel;
    m_elTimeSignature->setObjectName("elTimeSignature");
    m_elTimeSignature->label->setAlignment(Qt::AlignCenter);
    m_elTimeSignature->setText(QString::number(m_numerator) + "/" + QString::number(m_denominator));

    m_btnStop = new QPushButton;
    m_btnStop->setObjectName("btnStop");
    m_btnStop->setIcon(icoStopWhite);

    m_btnPlay = new QPushButton;
    m_btnPlay->setObjectName("btnPlay");
    m_btnPlay->setIcon(icoPlayWhite);
    // m_btnPlay->setText("Play");
    m_btnPlay->setCheckable(true);

    m_btnPlayPause = new QPushButton(this);
    connect(m_btnPlayPause, &QPushButton::pressed, this, [this] {
        if (m_status == Paused || m_status == Stopped)
            playTriggered();
        else if (m_status == Playing)
            pauseTriggered();
    });
    m_btnPlayPause->setShortcut(Qt::Key_Space);
    m_btnPlayPause->setFixedSize(0, 0);

    m_btnPause = new QPushButton;
    m_btnPause->setObjectName("btnPause");
    m_btnPause->setIcon(icoPauseWhite);
    // m_btnPause->setText("Pause");
    m_btnPause->setCheckable(true);

    m_elTime = new EditLabel;
    m_elTime->setObjectName("elTime");
    m_elTime->label->setAlignment(Qt::AlignCenter);
    m_elTime->setText(toFormattedTickTime(m_tick));

    connect(m_elTempo, &EditLabel::editCompleted, this, [this](const QString &value) {
        const auto tempo = value.toDouble();
        if (m_tempo != tempo) {
            m_tempo = tempo;
            emit setTempoTriggered(tempo);
        }
    });

    connect(m_elTimeSignature, &EditLabel::editCompleted, this, [this](const QString &value) {
        if (!value.contains('/'))
            return;

        auto splitStr = value.split('/');

        if (splitStr.size() != 2) {
            return;
        }

        bool isValidNumerator = false;
        bool isValidDenominator = false;

        const auto numerator = splitStr.first().toInt(&isValidNumerator);
        const auto denominator = splitStr.last().toInt(&isValidDenominator);

        // If the numerator or denominator are not positive integers, ignore the change.
        if (!(isValidNumerator && isValidDenominator && numerator > 0 && denominator > 0)) {
            return;
        }

        if (m_numerator != numerator || m_denominator != denominator) {
            m_numerator = numerator;
            m_denominator = denominator;
            emit setTimeSignatureTriggered(numerator, denominator);
        }
    });
    connect(m_elTime, &EditLabel::editCompleted, this, [this](const QString &value) {
        if (!value.contains(':'))
            return;

        const auto splitStr = value.split(':');
        if (splitStr.size() != 3)
            return;

        const auto tick = fromTickTimeString(splitStr);
        if (tick != m_tick) {
            m_tick = tick;
            emit setPositionTriggered(tick);
        }
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

    m_cbQuantize = new ComboBox(true);
    m_cbQuantize->addItems(quantizeStrings);
    m_cbQuantize->setCurrentIndex(3);

    connect(m_cbQuantize, &QComboBox::currentIndexChanged, this, [this](const int index) {
        const auto value = quantizeValues.at(index);
        emit setQuantizeTriggered(value);
    });

    const auto transportLayout = new QHBoxLayout;
    transportLayout->addWidget(m_btnStop);
    transportLayout->addWidget(m_btnPlay);
    transportLayout->addWidget(m_btnPause);
    transportLayout->addWidget(m_elTime);
    transportLayout->setSpacing(1);
    transportLayout->setContentsMargins({});

    const auto transportWidget = new TitleControlGroup;
    transportWidget->setLayout(transportLayout);

    const auto scoreGlobalLayout = new QHBoxLayout;
    scoreGlobalLayout->addWidget(m_elTempo);
    scoreGlobalLayout->addWidget(m_elTimeSignature);
    scoreGlobalLayout->setSpacing(1);
    scoreGlobalLayout->setContentsMargins({});

    const auto scoreGlobalWidget = new TitleControlGroup;
    scoreGlobalWidget->setLayout(scoreGlobalLayout);

    const auto mainLayout = new QHBoxLayout;
    mainLayout->addWidget(m_cbQuantize);
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
    connect(this, &PlaybackView::setPositionTriggered, playbackController, [](const int tick) {
        playbackController->setLastPosition(tick);
        playbackController->setPosition(tick);
    });
    connect(this, &PlaybackView::setQuantizeTriggered, appController,
            &AppController::onSetQuantize);
    connect(playbackController, &PlaybackController::playbackStatusChanged, this,
            &PlaybackView::onPlaybackStatusChanged);
    connect(playbackController, &PlaybackController::positionChanged, this,
            &PlaybackView::onPositionChanged);
    connect(appModel, &AppModel::modelChanged, this, &PlaybackView::updateView);
    connect(appModel, &AppModel::tempoChanged, this, &PlaybackView::onTempoChanged);
    connect(appModel, &AppModel::timeSignatureChanged, this, &PlaybackView::onTimeSignatureChanged);
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
}

void PlaybackView::onTempoChanged(const double tempo) {
    m_tempo = tempo;
    updateTempoView();
}

void PlaybackView::onTimeSignatureChanged(const int numerator, const int denominator) {
    m_numerator = numerator;
    m_denominator = denominator;
    updateTimeSignatureView();
    updateTimeView();
}

void PlaybackView::onPositionChanged(const double tick) {
    m_tick = static_cast<int>(tick);
    updateTimeView();
}

void PlaybackView::onPlaybackStatusChanged(const PlaybackStatus status) {
    m_status = status;
    updatePlaybackControlView();
}

QString PlaybackView::toFormattedTickTime(const int ticks) const {
    const int barTicks = 1920 * m_numerator / m_denominator;
    const int beatTicks = 1920 / m_denominator;
    const auto bar = ticks / barTicks + 1;
    const auto beat = ticks % barTicks / beatTicks + 1;
    const auto tick = ticks % barTicks % beatTicks;
    auto str = QString::asprintf("%03d", bar) + ":" + QString::asprintf("%02d", beat) + ":" +
               QString::asprintf("%03d", tick);
    return str;
}

int PlaybackView::fromTickTimeString(const QStringList &splitStr) const {
    const auto bar = splitStr.at(0).toInt();
    const auto beat = splitStr.at(1).toInt();
    const auto tick = splitStr.at(2).toInt();
    return (bar - 1) * 1920 * m_numerator / m_denominator + (beat - 1) * 1920 / m_denominator +
           tick;
}

void PlaybackView::updateTempoView() const {
    m_elTempo->setText(QString::number(m_tempo));
}

void PlaybackView::updateTimeSignatureView() const {
    m_elTimeSignature->setText(QString::number(m_numerator) + "/" + QString::number(m_denominator));
}

void PlaybackView::updateTimeView() const {
    m_elTime->setText(toFormattedTickTime(m_tick));
}

void PlaybackView::updatePlaybackControlView() const {
    if (m_status == Playing) {
        m_btnPlay->setChecked(true);
        m_btnPlay->setIcon(icoPlayBlack);

        m_btnPause->setChecked(false);
        m_btnPause->setIcon(icoPauseWhite);
    } else if (m_status == Paused) {
        m_btnPlay->setChecked(false);
        m_btnPlay->setIcon(icoPlayWhite);

        m_btnPause->setChecked(true);
        m_btnPause->setIcon(icoPauseBlack);
    } else {
        m_btnPlay->setChecked(false);
        m_btnPlay->setIcon(icoPlayWhite);

        m_btnPause->setChecked(false);
        m_btnPause->setIcon(icoPauseWhite);
    }
}
