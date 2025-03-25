//
// Created by fluty on 2024/1/29.
//

#include "TrackControlView.h"

#include "Controller/TrackController.h"
#include "Model/AppModel/Track.h"
#include "Modules/Audio/AudioContext.h"
#include "UI/Controls/Button.h"
#include "UI/Controls/EditLabel.h"
#include "UI/Controls/LevelMeter.h"
#include "UI/Controls/LineEdit.h"

#include <QContextMenuEvent>
#include <QHBoxLayout>
#include <QLabel>
#include <QMWidgets/cmenu.h>

#include "UI/Controls/SvsSeekbar.h"
#include "UI/Views/Common/LanguageComboBox.h"

using namespace SVS;

TrackControlView::TrackControlView(QListWidgetItem *item, Track *track, QWidget *parent)
    : QWidget(parent), ITrack(track->id()), m_track(track) {
    m_item = item;
    setAttribute(Qt::WA_StyledBackground);

    lbTrackIndex = new QLabel("1");
    lbTrackIndex->setObjectName("lbTrackIndex");
    lbTrackIndex->setAlignment(Qt::AlignCenter);

    btnMute = new Button("M");
    btnMute->setObjectName("btnMute");
    btnMute->setCheckable(true);
    btnMute->setChecked(false);
    btnMute->setContentsMargins(0, 0, 0, 0);
    connect(btnMute, &QPushButton::clicked, this, [&] { changeTrackProperty(); });

    btnSolo = new Button("S");
    btnSolo->setObjectName("btnSolo");
    btnSolo->setCheckable(true);
    btnSolo->setChecked(false);
    connect(btnSolo, &QPushButton::clicked, this, [&] { changeTrackProperty(); });

    leTrackName = new EditLabel();
    leTrackName->setObjectName("leTrackName");
    leTrackName->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Fixed);
    connect(leTrackName, &EditLabel::editCompleted, this, [&] { changeTrackProperty(); });

    muteSoloTrackNameLayout = new QHBoxLayout;
    muteSoloTrackNameLayout->setObjectName("muteSoloTrackNameLayout");
    muteSoloTrackNameLayout->addWidget(leTrackName);
    muteSoloTrackNameLayout->addWidget(btnMute);
    muteSoloTrackNameLayout->addWidget(btnSolo);
    // m_muteSoloTrackNameLayout->addWidget(m_cbLanguage);
    muteSoloTrackNameLayout->setSpacing(4);
    muteSoloTrackNameLayout->setContentsMargins(4, 8, 4, 8);

    cbSinger = new ComboBox;
    cbSinger->setObjectName("cbSinger");
    cbSinger->addItems({"Singer1", "Singer2", "Singer3"});

    cbLanguage = new LanguageComboBox("unknown");
    cbLanguage->setObjectName("cbLanguage");

    sbPan = new SeekBar;
    sbPan->setObjectName("m_sbarPan");
    sbPan->setTracking(false);
    connect(sbPan, &SeekBar::sliderMoved, this, &TrackControlView::onPanMoved);
    connect(sbPan, &SeekBar::valueChanged, this, &TrackControlView::onSliderReleased);
    //        m_panSlider->setValue(50);

    lePan = new EditLabel();
    lePan->setText("C");
    lePan->setObjectName("lePan");
    lePan->setFixedWidth(2 * m_buttonSize);
    lePan->setFixedHeight(m_buttonSize);
    lePan->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Minimum);
    lePan->label->setAlignment(Qt::AlignCenter);
    lePan->setEnabled(false);
    connect(sbPan, &SeekBar::valueChanged, lePan,
            [=](int value) { lePan->setText(panValueToString(value)); });

    sbGain = new SeekBar;
    sbGain->setObjectName("m_sbarGain");
    sbGain->setMaximum(100); // +6dB
    sbGain->setMinimum(0);   // -inf
    sbGain->setDefaultValue(79.4328234724);
    sbGain->setValue(79.4328234724);
    sbGain->setTracking(false);
    connect(sbGain, &SeekBar::sliderMoved, this, &TrackControlView::onGainMoved);
    connect(sbGain, &SeekBar::valueChanged, this, &TrackControlView::onSliderReleased);

    leGain = new EditLabel();
    leGain->setText("0.0dB");
    leGain->setObjectName("leGain");
    leGain->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Minimum);
    leGain->label->setAlignment(Qt::AlignCenter);
    leGain->setFixedWidth(2 * m_buttonSize);
    leGain->setFixedHeight(m_buttonSize);
    leGain->setEnabled(false);
    connect(sbGain, &SeekBar::valueChanged, leGain,
            [=](double value) { leGain->setText(gainValueToString(value)); });

    panVolumeSpacer = new QSpacerItem(20, 40, QSizePolicy::Minimum, QSizePolicy::Expanding);

    panVolumeLayout = new QHBoxLayout;
    panVolumeLayout->addWidget(sbPan);
    panVolumeLayout->addWidget(lePan);
    panVolumeLayout->addWidget(sbGain);
    panVolumeLayout->addWidget(leGain);
    // panVolumeLayout->addWidget(cbSinger);
    // panVolumeLayout->addWidget(cbLanguage);
    panVolumeLayout->setSpacing(4);
    panVolumeLayout->setContentsMargins(4, 0, 4, 8);

    controlWidgetLayout = new QVBoxLayout;
    controlWidgetLayout->addLayout(muteSoloTrackNameLayout);
    controlWidgetLayout->addLayout(panVolumeLayout);
    controlWidgetLayout->addItem(panVolumeSpacer);

    m_levelMeter = new LevelMeter();
    m_levelMeter->initBuffer(8);
    // m_levelMeter->setFixedWidth(12);

    // m_levelMeter->resize(32, 256);
    // m_levelMeter->setWindowFlags(Qt::Tool | Qt::WindowStaysOnTopHint);
    // m_levelMeter->setStyleSheet("LevelMeter { background-color: #21242B; qproperty-padding: 0 }");
    // m_levelMeter->show();
    // connect(this, &QObject::destroyed, m_levelMeter, [=] { m_levelMeter->close(); });

    mainLayout = new QHBoxLayout;
    mainLayout->setObjectName("TrackControlPanel");
    mainLayout->addWidget(lbTrackIndex);
    mainLayout->addLayout(controlWidgetLayout);
    mainLayout->addWidget(m_levelMeter);
    mainLayout->setSpacing(0);
    mainLayout->setContentsMargins({});

    setLayout(mainLayout);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);

    setName(track->name());
    setControl(track->control());
    setLanguage(track->defaultLanguage());
}

int TrackControlView::trackIndex() const {
    return lbTrackIndex->text().toInt();
}

void TrackControlView::setTrackIndex(int i) {
    lbTrackIndex->setText(QString::number(i));
}

QString TrackControlView::name() const {
    return leTrackName->text();
}

void TrackControlView::setName(const QString &name) {
    leTrackName->setText(name);
    // changeTrackProperty();
}

TrackControl TrackControlView::control() const {
    TrackControl control;
    auto gain = gainFromSliderValue(sbGain->value());
    control.setGain(gain);
    control.setPan(sbPan->value());
    control.setMute(btnMute->isChecked());
    control.setSolo(btnSolo->isChecked());
    return control;
}

void TrackControlView::setControl(const TrackControl &control) {
    m_notifyBarrier = true;
    auto barValue = gainToSliderValue(control.gain());
    sbGain->setValue(barValue);
    leGain->setText(gainValueToString(barValue));
    sbPan->setValue(control.pan());
    lePan->setText(panValueToString(control.pan()));
    btnMute->setChecked(control.mute());
    btnSolo->setChecked(control.solo());
    m_notifyBarrier = false;
}

void TrackControlView::setNarrowMode(bool on) {
    if (on) {
        for (int i = 0; i < panVolumeLayout->count(); ++i) {
            QWidget *w = panVolumeLayout->itemAt(i)->widget();
            if (w != nullptr)
                w->setVisible(false);
            panVolumeLayout->setContentsMargins(0, 0, 0, 0);
        }
    } else {
        for (int i = 0; i < panVolumeLayout->count(); ++i) {
            QWidget *w = panVolumeLayout->itemAt(i)->widget();
            if (w != nullptr)
                w->setVisible(true);
            panVolumeLayout->setContentsMargins(4, 0, 4, 8);
        }
    }
}

void TrackControlView::setLanguage(const QString &language) {
    cbLanguage->setCurrentText(language);
}

LevelMeter *TrackControlView::levelMeter() const {
    return m_levelMeter;
}

void TrackControlView::onPanMoved(double value) {
    lePan->setText(panValueToString(value));
    audioContext->handlePanSliderMoved(m_track, value);
}

void TrackControlView::onGainMoved(double value) {
    leGain->setText(gainValueToString(value));
    audioContext->handleGainSliderMoved(m_track, gainFromSliderValue(value));
}

void TrackControlView::onSliderReleased() {
    if (!m_notifyBarrier)
        changeTrackProperty();
}

void TrackControlView::contextMenuEvent(QContextMenuEvent *event) {
    auto actionInsert = new QAction("Insert new track", this);
    connect(actionInsert, &QAction::triggered, this, [&] { emit insertNewTrackTriggered(); });
    auto actionRemove = new QAction("Delete", this);
    connect(actionRemove, &QAction::triggered, this, [&] { emit removeTrackTriggered(id()); });

    CMenu menu(this);
    menu.addAction(actionInsert);
    menu.addAction(actionRemove);
    menu.exec(event->globalPos());
    event->accept();
}

void TrackControlView::changeTrackProperty() {
    // qDebug() << "TrackControlWidget::changeTrackProperty";
    Track::TrackProperties args(*this);
    trackController->changeTrackProperty(args);
}

QString TrackControlView::panValueToString(double value) {
    if (value < 0)
        return "L" + QString::number(-qRound(value));
    if (value == 0)
        return "C";
    return "R" + QString::number(qRound(value));
}

QString TrackControlView::gainValueToString(double value) {
    auto gain = 60 * std::log10(1.0 * value) - 114;
    if (gain == -70)
        return "-inf";
    auto absVal = QString::number(qAbs(gain), 'f', 1);
    QString sig = "";
    if (gain > 0) {
        sig = "+";
    } else if (gain < 0 && gain <= -0.1) {
        sig = "-";
    }
    return sig + absVal + "dB";
}

double TrackControlView::gainToSliderValue(double gain) {
    return std::pow(10, (114 + gain) / 60);
}

double TrackControlView::gainFromSliderValue(double value) {
    return 60 * std::log10(value) - 114;
}