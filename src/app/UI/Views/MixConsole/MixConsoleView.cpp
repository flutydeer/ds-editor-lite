//
// Created by FlutyDeer on 2025/6/3.
//

#include "MixConsoleView.h"

#include "Controller/AppController.h"
#include "Controller/TrackController.h"
#include "Model/AppModel/Track.h"
#include "Modules/Audio/AudioContext.h"
#include "UI/Controls/Fader.h"
#include "UI/Controls/LevelMeter.h"
#include "UI/Controls/PanSlider.h"
#include "UI/Views/MixConsole/ChannelView.h"

#include <QHBoxLayout>
#include <QListWidget>

QString MixConsoleView::tabId() const {
    return "MixConsole";
}

QString MixConsoleView::tabName() const {
    return "混音";
}

AppGlobal::PanelType MixConsoleView::panelType() const {
    return AppGlobal::Generic;
}

QWidget *MixConsoleView::toolBar() {
    return m_placeHolder;
}

QWidget *MixConsoleView::content() {
    return this;
}

MixConsoleView::MixConsoleView(QWidget *parent) : QWidget(parent) {
    m_channelListView = new QListWidget;
    m_channelListView->setObjectName("channelListView");
    m_channelListView->setViewMode(QListView::ListMode);
    m_channelListView->setHorizontalScrollMode(QAbstractItemView::ScrollPerPixel);
    m_channelListView->setSelectionMode(QAbstractItemView::SingleSelection);
    m_channelListView->setFlow(QListView::LeftToRight);

    m_masterChannel = new ChannelView;
    m_masterChannel->setName(tr("Master"));
    m_masterChannel->setIsMasterChannel(true);

    m_placeHolder = new QWidget;

    auto mainLayout = new QHBoxLayout;
    mainLayout->addWidget(m_channelListView);
    mainLayout->addWidget(m_masterChannel);
    mainLayout->setContentsMargins({});
    setLayout(mainLayout);
    setContentsMargins({1, 1, 1, 1});

    onModelChanged();
    connect(AudioContext::instance(), &AudioContext::levelMeterUpdated, this,
            &MixConsoleView::onLevelMetersUpdated);
    connect(appModel, &AppModel::modelChanged, this, &MixConsoleView::onModelChanged);
    connect(appModel, &AppModel::trackChanged, this, &MixConsoleView::onTrackChanged);
    connect(appModel, &AppModel::masterControlChanged, this,
            &MixConsoleView::onMasterControlChanged);

    connect(m_masterChannel->fader(), &Fader::sliderMoved, this, [=](double gain) {
        audioContext->handleMasterGainSliderMoved(gain);
    });
    connect(m_masterChannel->panSlider(), &PanSlider::sliderMoved, this, [=](double pan) {
        audioContext->handleMasterPanSliderMoved(pan);
    });
    connect(m_masterChannel, &ChannelView::controlChanged, this, [&](const TrackControl &control) {
        appController->editMasterControl(control);
    });
}

void MixConsoleView::onModelChanged() {
    onMasterControlChanged(appModel->masterControl());

    for (auto i = m_channelListView->count() - 1; i >= 0; i--) {
        onTrackRemoved(i);
    }
    int index = 0;
    for (const auto track : appModel->tracks()) {
        onTrackInserted(track, index);
        index++;
    }
}

void MixConsoleView::onTrackChanged(AppModel::TrackChangeType type, qsizetype index, Track *track) {
    if (type == AppModel::Insert)
        onTrackInserted(track, index);
    else if (type == AppModel::Remove)
        onTrackRemoved(index);
}

void MixConsoleView::onMasterControlChanged(const TrackControl &control) {
    m_masterChannel->setControl(control);
}

void MixConsoleView::onLevelMetersUpdated(const AppModel::LevelMetersUpdatedArgs &args) const {
    auto states = args.trackMeterStates;
    if (states.size() > 1)
        for (int i = 0; i < m_channelListView->count(); i++) {
            auto state = states.at(i);
            auto item = m_channelListView->item(i);
            auto channelView = qobject_cast<ChannelView *>(m_channelListView->itemWidget(item));
            auto meter = channelView->levelMeter();
            meter->setValue(state.valueL, state.valueR);
        }
    auto state = args.trackMeterStates.last();
    m_masterChannel->levelMeter()->setValue(state.valueL, state.valueR);
}

void MixConsoleView::onTrackInserted(Track *dsTrack, qsizetype trackIndex) {
    connect(dsTrack, &Track::propertyChanged, this, [=] {
        onTrackPropertyChanged();
    });

    auto newTrackItem = new QListWidgetItem;
    newTrackItem->setSizeHint({97, 420});
    auto channelView = new ChannelView(*dsTrack);
    channelView->setChannelIndex(trackIndex + 1);
    connect(channelView->fader(), &Fader::sliderMoved, this, [=](double gain) {
        audioContext->handleGainSliderMoved(&channelView->context(), gain);
    });
    connect(channelView->panSlider(), &PanSlider::sliderMoved, this, [=](double pan) {
        audioContext->handlePanSliderMoved(&channelView->context(), pan);
    });
    connect(channelView, &ChannelView::controlChanged, this, [=](const TrackControl &control) {
        const Track::TrackProperties args(*channelView);
        trackController->changeTrackProperty(args);
    });
    m_channelListView->insertItem(trackIndex, newTrackItem);
    m_channelListView->setItemWidget(newTrackItem, channelView);

    if (trackIndex < m_channelListView->count()) // needs to update existed tracks' index
        for (int i = trackIndex + 1; i < m_channelListView->count(); i++) {
            // Update track list items' index
            auto item = m_channelListView->item(i);
            auto widget = m_channelListView->itemWidget(item);
            auto trackWidget = dynamic_cast<ChannelView *>(widget);
            trackWidget->setChannelIndex(i + 1);
        }
}

void MixConsoleView::onTrackRemoved(qsizetype index) {
    // remove from view
    auto item = m_channelListView->takeItem(index);
    m_channelListView->removeItemWidget(item);
    delete item;
    // update index
    if (index < m_channelListView->count()) // needs to update existed tracks' index
        for (int i = index; i < m_channelListView->count(); i++) {
            // Update track list items' index
            auto widgetItem = m_channelListView->item(i);
            auto widget = m_channelListView->itemWidget(widgetItem);
            auto trackWidget = dynamic_cast<ChannelView *>(widget);
            trackWidget->setChannelIndex(i + 1);
        }
}

void MixConsoleView::onTrackPropertyChanged() const {
    auto tracksModel = appModel->tracks();
    for (int i = 0; i < m_channelListView->count(); i++) {
        auto item = m_channelListView->item(i);
        auto channelView = qobject_cast<ChannelView *>(m_channelListView->itemWidget(item));
        const QSignalBlocker blocker(channelView);
        auto track = tracksModel.at(i);
        channelView->setName(track->name());
        channelView->setControl(track->control());
    }
}