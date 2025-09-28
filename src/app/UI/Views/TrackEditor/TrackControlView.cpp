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
#include <QInputDialog>
#include <QLabel>
#include <QMWidgets/cmenu.h>

#include "UI/Controls/SvsSeekbar.h"
#include "UI/Views/Common/LanguageComboBox.h"

#include "Modules/PackageManager/PackageManager.h"
#include "Modules/Inference/Models/SingerIdentifier.h"

using namespace SVS;

namespace {
    constexpr auto SingerInfoRole = Qt::UserRole;
    constexpr auto SingerIdentifierRole = Qt::UserRole + 1;
}

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

    cbSinger = new TwoLevelComboBox;
    cbSinger->setObjectName("cbSinger");
    auto setCbSinger = [this](QList<PackageInfo> packages) {
        cbSinger->clear();
        cbSinger->addGroup("(no singer)");
        cbSinger->addItemToGroup("(no singer)", "(no singer)", {}, {});
        for (const auto &package : std::as_const(packages)) {
            const auto singers = package.singers();
            for (const auto &singer : singers) {
                QString singerText = singer.name();
                cbSinger->addGroup(singerText);
                for (const auto &spk : singer.speakers())
                    cbSinger->addItemToGroup(singerText, spk.id(), singer, spk);
            }
        }
    };

    setCbSinger(packageManager->installedPackages().successfulPackages);
    connect(packageManager, &PackageManager::packagesRefreshed, cbSinger, setCbSinger);

    connect(cbSinger, &TwoLevelComboBox::currentTextChanged, this, [this] {
        const auto currentText = cbSinger->currentText();
        const auto singerInfo = cbSinger->currentSinger();
        const auto speakerInfo = cbSinger->currentSpeaker();
        qDebug().noquote().nospace() << "Singer clicked: " << currentText;
        if (!m_track) {
            return;
        }
        m_track->setSingerInfo(singerInfo);
        m_track->setSpeakerInfo(speakerInfo);
    });

    cbLanguage = new LanguageComboBox("unknown");
    cbLanguage->setObjectName("cbLanguage");

    singerLanguageLayout = new QHBoxLayout;
    singerLanguageLayout->addWidget(cbSinger);
    singerLanguageLayout->addWidget(cbLanguage);

    controlWidgetLayout = new QVBoxLayout;
    controlWidgetLayout->addLayout(muteSoloTrackNameLayout);
    controlWidgetLayout->addLayout(singerLanguageLayout);

    m_levelMeter = new LevelMeter();

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
    auto control = m_control;
    control.setMute(btnMute->isChecked());
    control.setSolo(btnSolo->isChecked());
    return control;
}

void TrackControlView::setControl(const TrackControl &control) {
    m_notifyBarrier = true;
    m_control = control;
    btnMute->setChecked(control.mute());
    btnSolo->setChecked(control.solo());
    m_notifyBarrier = false;
}

void TrackControlView::setNarrowMode(bool on) {
    if (on) {
        for (int i = 0; i < singerLanguageLayout->count(); ++i) {
            QWidget *w = singerLanguageLayout->itemAt(i)->widget();
            if (w != nullptr)
                w->setVisible(false);
            singerLanguageLayout->setContentsMargins(0, 0, 0, 0);
        }
    } else {
        for (int i = 0; i < singerLanguageLayout->count(); ++i) {
            QWidget *w = singerLanguageLayout->itemAt(i)->widget();
            if (w != nullptr)
                w->setVisible(true);
            singerLanguageLayout->setContentsMargins(4, 0, 4, 8);
        }
    }
}

void TrackControlView::setLanguage(const QString &language) {
    cbLanguage->setCurrentText(language);
}

LevelMeter *TrackControlView::levelMeter() const {
    return m_levelMeter;
}

void TrackControlView::contextMenuEvent(QContextMenuEvent *event) {
    auto actionInsert = new QAction("Insert new track", this);
    connect(actionInsert, &QAction::triggered, this, [this] { emit insertNewTrackTriggered(); });
    auto actionRemove = new QAction("Delete", this);
    connect(actionRemove, &QAction::triggered, this, [this] { emit removeTrackTriggered(id()); });

    CMenu menu(this);
    menu.addAction(actionInsert);
    menu.addAction(actionRemove);

    auto singerMenu = menu.addMenu("Select track singer");
    const auto packages = packageManager->installedPackages().successfulPackages;
    for (const auto &package : std::as_const(packages)) {
        const auto singers = package.singers();
        for (const auto &singer : singers) {
            QString singerText = singer.name() + " (" + singer.identifier().packageId + " v" +
                                 singer.identifier().packageVersion.toString() + ')';
            auto singerAction = singerMenu->addAction(singerText);
            connect(singerAction, &QAction::triggered, this, [singerAction, singer, this] {
                qDebug().noquote().nospace() << "Singer clicked: " << singerAction->text();
                if (!m_track) {
                    return;
                }
                // auto comboBoxIndex = cbSinger->findData(QVariant::fromValue(singer.identifier()),
                //                                         SingerIdentifierRole);
                // if (comboBoxIndex != -1) {
                //     cbSinger->setCurrentIndex(comboBoxIndex);
                // }
            });
        }
    }
    auto actionSetSpeaker = new QAction("Set speaker", this);
    connect(actionSetSpeaker, &QAction::triggered, this, [this] {
        const auto singerInfo = m_track->singerInfo();
        const auto speakers = singerInfo.speakers();
        QStringList speakerLabels;
        QHash<QString, SpeakerInfo> speakerInfoMapping;
        speakerLabels.reserve(speakers.size() + 1);
        speakerInfoMapping.reserve(speakers.size());
        speakerLabels.emplace_back("(not specified)");
        int listCurrentIndex = 0; // 0 as not specified
        int currentIndex = 0;
        for (const auto &speaker : speakers) {
            const auto speakerId = speaker.id();
            const auto speakerName = speaker.name();
            const auto speakerLabel = speakerName + " (" + speakerId + ")";
            speakerLabels.emplace_back(speakerLabel);
            speakerInfoMapping[speakerLabel] = speaker;
            ++currentIndex;
            if (m_track->speakerInfo() == speaker) {
                listCurrentIndex = currentIndex;
            }
        }
        bool ok = false;
        auto currentSpeakerLabel = QInputDialog::getItem(
            this, "Input select", "Please select speaker id for singer " + cbSinger->currentText(),
            speakerLabels, listCurrentIndex, false, &ok);
        if (ok) {
            if (const auto it = speakerInfoMapping.find(currentSpeakerLabel);
                it != speakerInfoMapping.end()) {
                m_track->setSpeakerInfo(it.value());
                qDebug() << "Speaker for track" << m_track->id() << "set to" << it.value().name()
                         << " (" << it.value().id() << ")";
            } else {
                m_track->setSpeakerInfo({});
                qDebug() << "Speaker for track" << m_track->id() << "cleared";
            }
        }
    });
    menu.addAction(actionSetSpeaker);

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