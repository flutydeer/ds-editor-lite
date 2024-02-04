//
// Created by fluty on 2024/2/4.
//

#include "PlaybackView.h"

#include <QHBoxLayout>
#include <qvalidator.h>

PlaybackView::PlaybackView(QWidget *parent) {
    auto controller = PlaybackController::instance();

    m_elTempo = new EditLabel;
    m_elTempo->setObjectName("elTempo");
    m_elTempo->setText(QString::number(m_tempo));
    auto doubleValidator = new QDoubleValidator(m_elTempo->lineEdit);
    m_elTempo->lineEdit->setValidator(doubleValidator);
    m_elTempo->lineEdit->setAlignment(Qt::AlignCenter);
    m_elTempo->label->setAlignment(Qt::AlignCenter);
    m_elTempo->setFixedSize(2 * m_contentHeight, m_contentHeight);

    m_elTimeSignature = new EditLabel;
    m_elTimeSignature->setObjectName("elTimeSignature");
    m_elTimeSignature->lineEdit->setAlignment(Qt::AlignCenter);
    m_elTimeSignature->label->setAlignment(Qt::AlignCenter);
    m_elTimeSignature->setText(QString::number(m_numerator) + "/" + QString::number(m_denominator));
    m_elTimeSignature->setFixedSize(2 * m_contentHeight, m_contentHeight);

    m_btnStop = new QPushButton;
    m_btnStop->setObjectName("btnStop");
    m_btnStop->setFixedSize(2 * m_contentHeight, m_contentHeight);
    m_btnStop->setText("Stop"); // TODO: Use icon

    m_btnPlayPause = new QPushButton;
    m_btnPlayPause->setObjectName("btnPlayPause");
    m_btnPlayPause->setFixedSize(2 * m_contentHeight, m_contentHeight);
    m_btnPlayPause->setText("Play");

    auto btnPause = new QPushButton;
    btnPause->setObjectName("btnPause");
    btnPause->setFixedSize(2 * m_contentHeight, m_contentHeight);
    btnPause->setText("Pause");

    m_elTime = new EditLabel;
    m_elTime->setObjectName("elTime");
    m_elTime->lineEdit->setAlignment(Qt::AlignCenter);
    m_elTime->label->setAlignment(Qt::AlignCenter);
    m_elTime->setText(toFormattedTickTime(m_tick));
    m_elTime->setFixedSize(120, m_contentHeight);

    connect(m_elTempo, &EditLabel::valueChanged, this, [=](const QString &value) {
        auto tempo = value.toDouble();
        if (tempo > 0)
            emit changeTempoTriggered(tempo);
    });
    // connect(m_elTime, &EditLabel::valueChanged, this, [=](const QString &value) {
    //
    // });
    connect(m_btnPlayPause, &QPushButton::clicked, controller, [=] {
        // TODO: run project check (overlapping)
        PlaybackController::instance()->play();
    });
    connect(btnPause, &QPushButton::clicked, controller, &PlaybackController::pause);
    connect(m_btnStop, &QPushButton::clicked, controller, &PlaybackController::stop);

    connect(controller, &PlaybackController::positionChanged, m_elTime,
            [=](double tick) { m_elTime->setText(toFormattedTickTime(tick)); });

    auto mainLayout = new QHBoxLayout;
    mainLayout->addWidget(m_elTempo);
    mainLayout->addWidget(m_elTimeSignature);
    mainLayout->addWidget(m_btnStop);
    mainLayout->addWidget(m_btnPlayPause);
    mainLayout->addWidget(btnPause);
    mainLayout->addWidget(m_elTime);
    setLayout(mainLayout);
    setContentsMargins({});
    setMaximumHeight(44);
    setMaximumWidth(480);

    setStyleSheet("QPushButton {padding: 0px;} "
                  "QLabel { color: #F0F0F0;}");
}
void PlaybackView::onTempoChanged(double tempo) {
    m_tempo = tempo;
    m_elTempo->setText(QString::number(m_tempo));
}
void PlaybackView::onTimeSignatureChanged(int numerator, int denominator) {
    m_numerator = numerator;
    m_denominator = denominator;
    m_elTimeSignature->setText(QString::number(m_numerator) + "/" + QString::number(m_denominator));
}
void PlaybackView::onPlaybackStatusChanged(PlaybackController::PlaybackStatus status) {
}
QString PlaybackView::toFormattedTickTime(int ticks) {
    int barTicks = 1920 * m_numerator / m_denominator;
    int beatTicks = 1920 / m_denominator;
    auto bar = ticks / barTicks + 1;
    auto beat = ticks % barTicks / beatTicks + 1;
    auto tick = ticks % barTicks % beatTicks;
    auto str = QString::asprintf("%03d", bar) + ":" + QString::asprintf("%02d", beat) + ":" +
               QString::asprintf("%03d", tick);
    return str;
}