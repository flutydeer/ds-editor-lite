//
// Created by fluty on 2024/1/29.
//

#include "TrackControlView.h"

#include "Controller/Actions/AppModel/SpeakerMix/SpeakerMixActions.h"
#include "Controller/TrackController.h"
#include "Model/AppModel/Track.h"
#include "Model/AppOptions/AppOptions.h"
#include "Model/SpeakerMixPreset/SpeakerMixPresetStore.h"
#include "Modules/Audio/AudioContext.h"
#include "Modules/History/HistoryManager.h"
#include "UI/Controls/Button.h"
#include "UI/Controls/InlineEditLabel.h"
#include "UI/Controls/LevelMeter.h"
#include "UI/Controls/Menu.h"
#include "UI/Controls/Toast.h"
#include "UI/Controls/TrackColorSwatchWidget.h"
#include "UI/Controls/SvsSeekbar.h"
#include "UI/Dialogs/Base/Dialog.h"
#include "UI/Dialogs/SpeakerMix/SpeakerMixDialog.h"
#include "UI/Utils/AppColorPalette.h"
#include "UI/Utils/IconUtils.h"
#include "UI/Utils/SpeakerMixDisplayUtils.h"

#include <QContextMenuEvent>
#include <QDialog>
#include <QHBoxLayout>
#include <QInputDialog>
#include <QLabel>
#include <QMWidgets/cmenu.h>
#include <QWidgetAction>

#include <optional>

#include "UI/Controls/SvsSeekbar.h"
#include "UI/Views/Common/LanguageComboBox.h"

#include "Modules/PackageManager/PackageManager.h"
#include "Modules/Inference/Models/SingerIdentifier.h"

using namespace SVS;
using SpeakerMixModel::SpeakerMixData;

namespace {

    QIcon menuIcon(const QString &path) {
        return IconUtils::menuIcon(path);
    }
}

TrackControlView::TrackControlView(QListWidgetItem *item, Track *track, QWidget *parent)
    : QWidget(parent), ITrack(track->id()), m_track(track) {
    m_item = item;
    setAttribute(Qt::WA_StyledBackground);

    lbTrackIndex = new QLabel("1");
    lbTrackIndex->setObjectName("lbTrackIndex");
    lbTrackIndex->setAlignment(Qt::AlignCenter);
    lbTrackIndex->setCursor(Qt::OpenHandCursor);

    btnMute = new Button("M");
    btnMute->setObjectName("btnMute");
    btnMute->setCheckable(true);
    btnMute->setChecked(false);
    btnMute->setFixedSize(m_buttonSize, m_buttonSize);
    btnMute->setContentsMargins(0, 0, 0, 0);
    connect(btnMute, &QPushButton::clicked, this, [&] { changeTrackProperty(); });

    btnSolo = new Button("S");
    btnSolo->setObjectName("btnSolo");
    btnSolo->setCheckable(true);
    btnSolo->setChecked(false);
    btnSolo->setFixedSize(m_buttonSize, m_buttonSize);
    connect(btnSolo, &QPushButton::clicked, this, [&] { changeTrackProperty(); });

    leTrackName = new InlineEditLabel();
    leTrackName->setObjectName("leTrackName");
    leTrackName->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    leTrackName->setFixedHeight(m_buttonSize);
    connect(leTrackName, &InlineEditLabel::editCompleted, this, [&] { changeTrackProperty(); });

    muteSoloTrackNameLayout = new QHBoxLayout;
    muteSoloTrackNameLayout->setObjectName("muteSoloTrackNameLayout");
    muteSoloTrackNameLayout->addWidget(leTrackName, 1);
    muteSoloTrackNameLayout->addWidget(btnMute);
    muteSoloTrackNameLayout->addWidget(btnSolo);
    muteSoloTrackNameLayout->setSpacing(4);
    muteSoloTrackNameLayout->setContentsMargins(4, 8, 8, 4);

    cbSinger = new TwoLevelComboBox;
    cbSinger->setObjectName("cbSinger");
    cbSinger->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    if (appStatus->packageModuleStatus == AppStatus::ModuleStatus::Ready) {
        cbSinger->setItems(packageManager->installedPackages().successfulPackages);
    } else {
        cbSinger->setEnabled(false);
        cbSinger->setLoadingText(tr("(Scanning packages...)"));
    }
    connect(packageManager, &PackageManager::packagesRefreshed, this, [this] {
        cbSinger->setItems(packageManager->installedPackages().successfulPackages);
        refreshSingerComboPresentation();
    });
    connect(appStatus, &AppStatus::moduleStatusChanged, this,
            [this](AppStatus::ModuleType module, AppStatus::ModuleStatus status) {
                if (module == AppStatus::ModuleType::Package &&
                    status == AppStatus::ModuleStatus::Ready) {
                    cbSinger->setEnabled(true);
                    cbSinger->setLoadingText({});
                    cbSinger->setItems(packageManager->installedPackages().successfulPackages);
                    refreshSingerComboPresentation();
                }
            });

    connect(cbSinger, &TwoLevelComboBox::currentTextChanged, this, [this] {
        const auto currentText = cbSinger->currentText();
        const auto singerInfo = cbSinger->currentSinger();
        const auto speakerInfo = cbSinger->currentSpeaker();
        qDebug().noquote().nospace() << "Singer clicked: " << currentText;
        if (!m_track) {
            return;
        }
        if (m_track->singerInfo() == singerInfo && m_track->speakerInfo() == speakerInfo &&
            SpeakerMixModel::isSpeakerMixDataSingle(m_track->speakerMixData())) {
            return;
        }

        const auto actions = new SpeakerMixActions;
        actions->selectTrackSingleSpeaker(singerInfo, speakerInfo, m_track);
        actions->execute();
        historyManager->record(actions);
        refreshSingerComboPresentation();
    });
    connect(cbSinger, &TwoLevelComboBox::itemsPopulated, this,
            &TrackControlView::refreshSingerComboPresentation);

    cbLanguage = new LanguageComboBox("unknown");
    cbLanguage->setObjectName("cbLanguage");
    cbLanguage->setMaximumWidth(144);

    singerLanguageLayout = new QHBoxLayout;
    singerLanguageLayout->addWidget(cbSinger);
    singerLanguageLayout->addWidget(cbLanguage);
    singerLanguageLayout->setSpacing(4);
    singerLanguageLayout->setContentsMargins(4, 0, 4, 0);

    controlWidgetLayout = new QVBoxLayout;
    controlWidgetLayout->addLayout(muteSoloTrackNameLayout);
    controlWidgetLayout->addLayout(singerLanguageLayout);
    controlWidgetLayout->addStretch();
    controlWidgetLayout->setSpacing(0);

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
    refreshSingerComboPresentation();
    setLanguage(track->defaultLanguage());
    updateTrackColor();

    // Nullify m_track when the Track is destroyed (e.g. during
    // AppModel::dispose()). Without this, a queued signal from the background
    // PackageManager thread (packagesRefreshed) may arrive after the Track has
    // been deleted, causing refreshSingerComboPresentation() to dereference a
    // dangling pointer and crash in QSharedDataPointer's fetch_add.
    connect(track, &QObject::destroyed, this, [this](QObject *) { m_track = nullptr; });

    connect(track, &Track::singerOrSpeakerChanged, this,
            &TrackControlView::refreshSingerComboPresentation);
    connect(track, &Track::speakerMixChanged, this,
            [this](const SpeakerMixData &) { refreshSingerComboPresentation(); });

    // 预设变化时刷新下拉框（如其他轨道保存/删除了同名预设）
    connect(appOptions, &AppOptions::optionsChanged, this,
            [this](AppOptionsGlobal::Option option) {
                if (option == AppOptionsGlobal::Option::General ||
                    option == AppOptionsGlobal::Option::All)
                    refreshSingerComboPresentation();
            });
}

int TrackControlView::trackIndex() const {
    return lbTrackIndex->text().toInt();
}

void TrackControlView::setTrackIndex(const int i) const {
    lbTrackIndex->setText(QString::number(i));
}

QString TrackControlView::name() const {
    return leTrackName->text();
}

void TrackControlView::setName(const QString &name) {
    leTrackName->setText(name);
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

void TrackControlView::setTrackNameOverlayParent(QWidget *parent) {
    leTrackName->setOverlayParent(parent);
}

void TrackControlView::finishTrackNameEditing() {
    leTrackName->finishEditing();
}

void TrackControlView::setNarrowMode(const bool on) const {
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

void TrackControlView::setLanguage(const QString &language) const {
    cbLanguage->setCurrentText(language);
}

LevelMeter *TrackControlView::levelMeter() const {
    return m_levelMeter;
}

bool TrackControlView::isInDragArea(const QPoint &pos) const {
    // Check if the position is within the track index label area
    const QRect indexRect = lbTrackIndex->geometry();
    return indexRect.contains(pos);
}

void TrackControlView::contextMenuEvent(QContextMenuEvent *event) {
    const auto actionInsert = new QAction("Insert new track", this);
    actionInsert->setIcon(menuIcon(QStringLiteral(":/svg/icons/add_16_regular.svg")));
    connect(actionInsert, &QAction::triggered, this, [this] { emit insertNewTrackTriggered(); });
    const auto actionRemove = new QAction("Delete", this);
    actionRemove->setIcon(menuIcon(QStringLiteral(":/svg/icons/delete_16_regular.svg")));
    connect(actionRemove, &QAction::triggered, this, [this] { emit removeTrackTriggered(id()); });

    Menu menu(this);
    menu.addAction(actionInsert);
    menu.addAction(actionRemove);

    auto colorMenu = new Menu("Track color", &menu);
    colorMenu->setIcon(menuIcon(QStringLiteral(":/svg/icons/color_16_regular.svg")));
    menu.addMenu(colorMenu);
    int originalColorIndex = m_track ? m_track->colorIndex() : 0;
    auto colorSwatch = new TrackColorSwatchWidget(originalColorIndex);
    auto colorAction = new QWidgetAction(colorMenu);
    colorAction->setDefaultWidget(colorSwatch);
    colorMenu->addAction(colorAction);
    bool colorConfirmed = false;
    connect(colorSwatch, &TrackColorSwatchWidget::colorIndexHovered, this, [this](int idx) {
        if (m_track) {
            m_track->setColorIndex(idx);
            emit m_track->propertyChanged();
        }
    });
    connect(colorSwatch, &TrackColorSwatchWidget::previewCancelled, this,
            [this, originalColorIndex] {
                if (m_track) {
                    m_track->setColorIndex(originalColorIndex);
                    emit m_track->propertyChanged();
                }
            });
    connect(colorSwatch, &TrackColorSwatchWidget::colorIndexSelected, this,
            [this, &menu, &colorConfirmed](int idx) {
                if (m_track) {
                    m_track->setColorIndex(idx);
                    emit m_track->propertyChanged();
                }
                colorConfirmed = true;
                menu.close();
            });

    menu.exec(event->globalPos());

    if (!colorConfirmed && m_track && m_track->colorIndex() != originalColorIndex) {
        m_track->setColorIndex(originalColorIndex);
        emit m_track->propertyChanged();
    }
    event->accept();
}

void TrackControlView::changeTrackProperty() const {
    const Track::TrackProperties args(*this);
    trackController->changeTrackProperty(args);
}

void TrackControlView::refreshSingerComboPresentation() const {
    if (!m_track || !cbSinger)
        return;

    cbSinger->setCurrentData(m_track->singerInfo(), m_track->speakerInfo());
    if (const auto text = SpeakerMixDisplayUtils::comboDisplayText(m_track->singerInfo(),
                                                                   m_track->speakerMixData());
        !text.isEmpty()) {
        cbSinger->setDisplayTextOverride(text);
    } else {
        cbSinger->clearDisplayTextOverride();
    }
    cbSinger->setToolTip(cbSinger->currentText());
    populatePresetMenus();
}

void TrackControlView::populatePresetMenus() const {
    if (!cbSinger)
        return;

    cbSinger->clearInjectedActions();
    const auto sourcePreset = m_track ? SpeakerMixPresetStore::sourcePresetForData(
                                            m_track->singerInfo(), m_track->speakerMixData())
                                      : std::optional<SpeakerMixPreset>();
    QAction *checkedPresetAction = nullptr;
    const auto packages = packageManager->installedPackages().successfulPackages;
    for (const auto &package : packages) {
        for (const auto &singerInfo : package.singers()) {
            if (singerInfo.speakers().size() < 2 || !cbSinger->groupMenuForSinger(singerInfo))
                continue;

            cbSinger->addInjectedSeparatorToSinger(singerInfo);
            const auto presets = SpeakerMixPresetStore::presetsForSinger(singerInfo);
            for (const auto &preset : presets) {
                const auto action = cbSinger->addInjectedActionToSinger(singerInfo, preset.name);
                if (!action)
                    continue;
                if (sourcePreset && preset.id == sourcePreset->id)
                    checkedPresetAction = action;
                connect(action, &QAction::triggered, this,
                        [this, presetId = preset.id] { onPresetApplied(presetId); });
            }

            cbSinger->addInjectedSeparatorToSinger(singerInfo);
            if (const auto action =
                    cbSinger->addInjectedActionToSinger(singerInfo, tr("Manage mix presets..."))) {
                connect(action, &QAction::triggered, this,
                        [this, singerInfo] { onManagePresetsAction(singerInfo); });
            }
        }
    }
    cbSinger->setCheckedInjectedAction(checkedPresetAction);
}

void TrackControlView::onPresetApplied(const QString &presetId) const {
    if (!m_track)
        return;

    const auto preset = SpeakerMixPresetStore::findPreset(presetId);
    if (!preset)
        return;
    const auto singerInfo = packageManager->findSingerByIdentifier(preset->singerIdentifier());
    if (singerInfo.isEmpty())
        return;

    const auto data = SpeakerMixPresetStore::speakerMixDataFromPreset(*preset, singerInfo);
    if (SpeakerMixModel::isSpeakerMixDataSingle(data)) {
        Toast::show(tr("Preset speakers are unavailable"));
        return;
    }

    const auto actions = new SpeakerMixActions;
    actions->applyTrackSpeakerMixPreset(singerInfo, data.sources.first().speaker, data, m_track);
    actions->execute();
    historyManager->record(actions);
    refreshSingerComboPresentation();
}

void TrackControlView::onManagePresetsAction(const SingerInfo &singerInfo) const {
    if (singerInfo.speakers().size() < 2)
        return;

    const auto initialData =
        m_track && m_track->singerInfo().identifier() == singerInfo.identifier()
            ? m_track->speakerMixData()
            : SpeakerMixData();
    SpeakerMixDialog dialog(singerInfo, initialData,
                            Dialog::globalParent() ? Dialog::globalParent()
                                                   : const_cast<TrackControlView *>(this));
    if (dialog.exec() == QDialog::Accepted && m_track) {
        const auto data = dialog.speakerMixData();
        if (!SpeakerMixModel::isSpeakerMixDataSingle(data)) {
            const auto actions = new SpeakerMixActions;
            actions->applyTrackSpeakerMixPreset(singerInfo, data.sources.first().speaker, data,
                                                m_track);
            actions->execute();
            historyManager->record(actions);
        }
    }
    refreshSingerComboPresentation();
}

QString TrackControlView::panValueToString(const double value) {
    if (value < 0)
        return "L" + QString::number(-qRound(value));
    if (value == 0)
        return "C";
    return "R" + QString::number(qRound(value));
}

QString TrackControlView::gainValueToString(const double value) {
    const auto gain = 60 * std::log10(1.0 * value) - 114;
    if (gain == -70)
        return "-inf";
    const auto absVal = QString::number(qAbs(gain), 'f', 1);
    QString sig = "";
    if (gain > 0) {
        sig = "+";
    } else if (gain < 0 && gain <= -0.1) {
        sig = "-";
    }
    return sig + absVal + "dB";
}

double TrackControlView::gainFromSliderValue(const double value) {
    return 60 * std::log10(value) - 114;
}

double TrackControlView::gainToSliderValue(const double gain) {
    return std::pow(10, (114 + gain) / 60);
}

int TrackControlView::colorIndex() const {
    if (m_track)
        return m_track->colorIndex();
    return 0;
}

void TrackControlView::setColorIndex(int colorIndex) {
    if (m_track)
        m_track->setColorIndex(colorIndex);
    updateTrackColor();
}

void TrackControlView::updateTrackColor() {
    int ci = m_track ? m_track->colorIndex() : 0;
    auto &palette = *AppColorPalette::instance();
    auto bg = palette.trackHeaderColor(ci);
    auto fg = palette.clipForeground(ci);
    auto css = QStringLiteral("background-color: %1; color: %2;").arg(bg.name(), fg.name());
    lbTrackIndex->setStyleSheet(css);
}
