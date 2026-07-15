//
// Created by fluty on 2024/2/10.
//

#include "ClipEditorToolBarView.h"
#include "ClipEditorToolBarView_p.h"

#include "Controller/Actions/AppModel/SpeakerMix/SpeakerMixActions.h"
#include "Controller/TrackController.h"
#include "Model/AppModel/AppModel.h"
#include "Model/AppModel/SingingClip.h"
#include "Model/AppOptions/AppOptions.h"
#include "Model/SpeakerMixPreset/SpeakerMixPresetStore.h"
#include "Model/AppStatus/AppStatus.h"
#include "Modules/History/HistoryManager.h"
#include "Modules/Inference/Models/SingerIdentifier.h"
#include "Modules/PackageManager/PackageManager.h"
#include "UI/Controls/Button.h"
#include "UI/Controls/ComboBox.h"
#include "UI/Controls/ControlGroup.h"
#include "UI/Controls/InlineEditLabel.h"
#include "UI/Controls/SvsSeekbar.h"
#include "UI/Controls/ToolTipFilter.h"
#include "UI/Controls/Toast.h"
#include "UI/Controls/TwoLevelComboBox.h"
#include "UI/Dialogs/SpeakerMix/SpeakerMixDialog.h"
#include "UI/Utils/IconUtils.h"
#include "UI/Utils/SpeakerMixDisplayUtils.h"
#include "UI/Views/Common/LanguageComboBox.h"

#include <QButtonGroup>
#include <QDialog>
#include <QHBoxLayout>
#include <QLabel>
#include <QSignalBlocker>

#include <optional>

namespace {

    const QStringList quantizeStrings = {"1/2", "1/4", "1/8", "1/16", "1/32", "1/64", "1/128"};
    const QList<int> quantizeValues = {2, 4, 8, 16, 32, 64, 128};

    int quantizeIndex(int quantize) {
        const int index = quantizeValues.indexOf(quantize);
        return index >= 0 ? index : quantizeValues.indexOf(16);
    }

} // namespace

using SpeakerMixModel::SpeakerMixData;

ClipEditorToolBarView::ClipEditorToolBarView(QWidget *parent)
    : QWidget(parent), d_ptr(new ClipEditorToolBarViewPrivate(this)) {
    Q_D(ClipEditorToolBarView);
    setObjectName("ClipEditorToolBarView");
    setFocusPolicy(Qt::ClickFocus);

    d->m_leClipName = new InlineEditLabel;
    d->m_leClipName->setObjectName("leClipName");
    d->m_leClipName->setEditRole(InlineEditLabel::ClipName);
    d->m_leClipName->installEventFilter(new ToolTipFilter(d->m_leClipName, 500));
    d->m_leClipName->setToolTip(tr("Clip Name"));
    d->m_leClipName->setFixedWidth(128);
    d->m_leClipName->setFixedHeight(d->m_contentHeight);
    d->m_leClipName->setEnabled(false);
    d->m_leClipName->setText("");
    d->m_leClipName->setOverlayParent(this);
    connect(d->m_leClipName, &InlineEditLabel::editCompleted, d,
            &ClipEditorToolBarViewPrivate::onClipNameEdited);
    connect(d->m_leClipName, &InlineEditLabel::editingStarted, d,
            [d] { d->m_editingClipId = d->m_clip ? d->m_clip->id() : -1; });

    d->m_cbSinger = new TwoLevelComboBox;
    d->m_cbSinger->setObjectName("cbClipSinger");
    d->m_cbSinger->setFixedWidth(128);
    d->m_cbSinger->installEventFilter(new ToolTipFilter(d->m_cbSinger, 500));
    d->m_cbSinger->setToolTip(tr("Clip Singer"));

    d->m_cbSinger->setShowInheritItem(true);
    if (appStatus->packageModuleStatus == AppStatus::ModuleStatus::Ready) {
        d->m_cbSinger->setItems(packageManager->installedPackages().successfulPackages);
    } else {
        d->m_cbSinger->setEnabled(false);
        d->m_cbSinger->setLoadingText(tr("(Scanning packages...)"));
    }
    connect(packageManager, &PackageManager::packagesRefreshed, d, [d] {
        d->m_cbSinger->setItems(packageManager->installedPackages().successfulPackages);
        d->refreshSingerComboPresentation();
    });
    connect(appStatus, &AppStatus::moduleStatusChanged, d,
            [d](AppStatus::ModuleType module, AppStatus::ModuleStatus status) {
                if (module == AppStatus::ModuleType::Package &&
                    status == AppStatus::ModuleStatus::Ready) {
                    d->m_cbSinger->setEnabled(true);
                    d->m_cbSinger->setLoadingText({});
                    d->m_cbSinger->setItems(packageManager->installedPackages().successfulPackages);
                    d->refreshSingerComboPresentation();
                }
            });
    connect(d->m_cbSinger, &TwoLevelComboBox::currentDataChanged, d,
            &ClipEditorToolBarViewPrivate::onSingerEdited);
    connect(d->m_cbSinger, &TwoLevelComboBox::itemsPopulated, d,
            &ClipEditorToolBarViewPrivate::refreshSingerComboPresentation);

    // 预设变化时刷新下拉框（如其他轨道保存/删除了同名预设）
    connect(appOptions, &AppOptions::optionsChanged, d,
            [d](AppOptionsGlobal::Option option) {
                if (option == AppOptionsGlobal::Option::General ||
                    option == AppOptionsGlobal::Option::All)
                    d->refreshSingerComboPresentation();
            });

    d->m_cbClipLanguage = new LanguageComboBox("unknown", true);
    d->m_cbClipLanguage->setObjectName("cbClipLanguage");
    d->m_cbClipLanguage->installEventFilter(new ToolTipFilter(d->m_cbClipLanguage, 500));
    d->m_cbClipLanguage->setToolTip(tr("Clip Default Language"));

    d->m_cbPianoRollQuantize = new ComboBox(true);
    d->m_cbPianoRollQuantize->setObjectName("cbPianoRollQuantize");
    d->m_cbPianoRollQuantize->addItems(quantizeStrings);
    d->m_cbPianoRollQuantize->setCurrentIndex(quantizeIndex(appStatus->pianoRollQuantize));
    d->m_cbPianoRollQuantize->setFixedHeight(d->m_contentHeight);
    d->m_cbPianoRollQuantize->setToolTip(tr("Piano Roll Quantize"));
    connect(d->m_cbPianoRollQuantize, &QComboBox::currentIndexChanged, this,
            [](int index) { appStatus->pianoRollQuantize = quantizeValues.at(index); });
    connect(appStatus, &AppStatus::pianoRollQuantizeChanged, d->m_cbPianoRollQuantize,
            [combo = d->m_cbPianoRollQuantize](int quantize) {
                const QSignalBlocker blocker(combo);
                combo->setCurrentIndex(quantizeIndex(quantize));
            });


    d->m_btnArrow =
        d->buildToolButton("btnArrow", ":svg/icons/cursor_24_filled.svg", tr("Select"), Qt::Key_V);
    d->m_btnArrow->setChecked(true);
    d->m_btnBeam = d->buildToolButton("btnBeam", ":svg/icons/beam_24_filled.svg",
                                      tr("Interval Select"), Qt::Key_B);
    const auto notePencilDesc =
        tr("Drag in the blank: Draw a new note\nDrag on a note: Edit the note");
    d->m_btnNotePencil = d->buildToolButton("btnNotePencil", ":svg/icons/edit_24_filled.svg",
                                            tr("Draw Note"), Qt::Key_N, notePencilDesc);
    d->m_btnNoteEraser = d->buildToolButton("btnNoteEraser", ":svg/icons/eraser_24_filled.svg",
                                            tr("Erase Note"), Qt::Key_M);
    d->m_btnNoteSplit =
        d->buildToolButton("btnNoteSplit", ":svg/icons/cut_20_filled.svg", tr("Split Note"),
                           QKeySequence(), tr("Split note at quantize line"));
    d->m_btnPitchAnchor = d->buildToolButton(
        "btnPitchAnchor", ":svg/icons/pitch_anchor_24_filled.svg", tr("Pitch Anchor"), Qt::Key_F);
    const auto pitchPencilDesc = tr("Left drag: Draw\nRight drag: Erase");
    d->m_btnPitchPencil =
        d->buildToolButton("btnPitchPencil", ":svg/icons/pitch_edit_24_filled.svg",
                           tr("Draw Pitch"), Qt::Key_G, pitchPencilDesc);
    d->m_btnPitchEraser = d->buildToolButton(
        "btnPitchEraser", ":svg/icons/pitch_erase_24_filled.svg", tr("Erase Pitch"), Qt::Key_H);
    auto freezePitchDesc = tr("Copy automatic pitch inference results to edited pitch");

    d->m_toolButtonGroup = new QButtonGroup;
    d->m_toolButtonGroup->setExclusive(true);
    d->m_toolButtonGroup->addButton(d->m_btnArrow);
    d->m_toolButtonGroup->addButton(d->m_btnBeam);
    d->m_toolButtonGroup->addButton(d->m_btnNotePencil);
    d->m_toolButtonGroup->addButton(d->m_btnNoteEraser);
    d->m_toolButtonGroup->addButton(d->m_btnNoteSplit);
    d->m_toolButtonGroup->addButton(d->m_btnPitchAnchor);
    d->m_toolButtonGroup->addButton(d->m_btnPitchPencil);
    d->m_toolButtonGroup->addButton(d->m_btnPitchEraser);
    connect(d->m_toolButtonGroup, &QButtonGroup::buttonToggled, d,
            &ClipEditorToolBarViewPrivate::onPianoRollToolButtonToggled);

    auto clipInfoLayout = new QHBoxLayout;
    clipInfoLayout->addWidget(d->m_leClipName);
    clipInfoLayout->addWidget(d->m_cbSinger);
    clipInfoLayout->addWidget(d->m_cbClipLanguage);
    clipInfoLayout->setSpacing(1);
    clipInfoLayout->setContentsMargins({});

    auto clipInfoGroup = new ControlGroup;
    clipInfoGroup->setLayout(clipInfoLayout);

    auto toolButtonLayout = new QHBoxLayout;
    toolButtonLayout->addWidget(d->m_btnArrow);
    toolButtonLayout->addWidget(d->m_btnBeam);
    toolButtonLayout->addWidget(d->m_btnNotePencil);
    toolButtonLayout->addWidget(d->m_btnNoteEraser);
    toolButtonLayout->addWidget(d->m_btnNoteSplit);
    toolButtonLayout->addWidget(d->m_btnPitchAnchor);
    toolButtonLayout->addWidget(d->m_btnPitchPencil);
    toolButtonLayout->addWidget(d->m_btnPitchEraser);
    toolButtonLayout->setSpacing(1);
    toolButtonLayout->setContentsMargins({});

    auto toolButtonGroup = new ControlGroup;
    toolButtonGroup->setLayout(toolButtonLayout);

    auto quantizeLayout = new QHBoxLayout;
    quantizeLayout->addWidget(d->m_cbPianoRollQuantize);
    quantizeLayout->setSpacing(1);
    quantizeLayout->setContentsMargins({});

    auto quantizeGroup = new ControlGroup;
    quantizeGroup->setLayout(quantizeLayout);


    const auto mainLayout = new QHBoxLayout;
    mainLayout->addWidget(clipInfoGroup);
    mainLayout->addSpacing(16);
    mainLayout->addWidget(toolButtonGroup);
    mainLayout->addSpacing(16);
    mainLayout->addWidget(quantizeGroup);
    mainLayout->addSpacing(16);
    mainLayout->addStretch();

    mainLayout->setContentsMargins({});
    mainLayout->setSpacing(1);
    setLayout(mainLayout);
    setFixedHeight(d->m_contentHeight);

    d->moveToNullClipState();
}

void ClipEditorToolBarView::setDataContext(Clip *clip) {
    Q_D(ClipEditorToolBarView);
    // Commit any in-progress edit before switching clips
    d->m_leClipName->finishEditing();
    d->m_editingClipId = -1;

    if (d->m_clip)
        disconnect(d->m_clip, nullptr, d, nullptr);

    d->m_clip = clip;
    if (clip == nullptr) {
        d->m_singingClip = nullptr;
        d->moveToNullClipState();
        return;
    }
    connect(d->m_clip, &Clip::propertyChanged, d,
            &ClipEditorToolBarViewPrivate::onClipPropertyChanged);
    if (clip->clipType() == Clip::Singing) {
        d->m_singingClip = static_cast<SingingClip *>(clip);
        d->moveToSingingClipState();
    } else if (clip->clipType() == Clip::Audio) {
        d->m_singingClip = nullptr;
        d->moveToAudioClipState();
    }
}

PianoRollEditMode ClipEditorToolBarView::editMode() const {
    Q_D(const ClipEditorToolBarView);
    return d->m_editMode;
}

void ClipEditorToolBarViewPrivate::onPianoRollToolButtonToggled(const QAbstractButton *button,
                                                                const bool checked) {
    Q_Q(ClipEditorToolBarView);
    if (!checked)
        return;

    if (button == m_btnArrow) {
        m_editMode = Select;
    } else if (button == m_btnBeam) {
        m_editMode = IntervalSelect;
    } else if (button == m_btnNotePencil) {
        m_editMode = DrawNote;
    } else if (button == m_btnNoteEraser) {
        m_editMode = EraseNote;
    } else if (button == m_btnNoteSplit) {
        m_editMode = SplitNote;
    } else if (button == m_btnPitchPencil) {
        m_editMode = DrawPitch;
    } else if (button == m_btnPitchAnchor) {
        m_editMode = EditPitchAnchor;
    } else if (button == m_btnPitchEraser) {
        m_editMode = ErasePitch;
    } else {
        m_editMode = FreezePitch;
    }

    emit q->editModeChanged(m_editMode);
}

void ClipEditorToolBarViewPrivate::onClipNameEdited(const QString &text) {
    const auto clipId = m_editingClipId;
    m_editingClipId = -1;
    const auto clip = appModel->findClipById(clipId);
    if (!clip || clip->name() == text)
        return;

    Clip::ClipCommonProperties args(*clip);
    args.name = text;
    trackController->onClipPropertyChanged(args);
}

void ClipEditorToolBarViewPrivate::onClipPropertyChanged() const {
    m_leClipName->finishEditing();
    m_leClipName->setText(m_clip->name());
}

void ClipEditorToolBarViewPrivate::onClipLanguageChanged(const QString &language) const {
    m_cbClipLanguage->setCurrentText(language);
}

void ClipEditorToolBarViewPrivate::onLanguageEdited(const QString &language) const {
    if (m_singingClip) {
        m_singingClip->setDefaultLanguage(language);
    }
}

void ClipEditorToolBarViewPrivate::moveToNullClipState() const {
    m_leClipName->setEnabled(false);
    m_leClipName->setText(QString());

    setPianoRollToolsEnabled(false);
}

void ClipEditorToolBarViewPrivate::moveToSingingClipState() const {
    m_leClipName->setEnabled(true);
    m_leClipName->setText(m_clip->name());

    setPianoRollToolsEnabled(true);
}

void ClipEditorToolBarViewPrivate::moveToAudioClipState() const {
    m_leClipName->setEnabled(true);
    m_leClipName->setText(m_clip->name());

    setPianoRollToolsEnabled(false);
}

Button *ClipEditorToolBarViewPrivate::buildToolButton(const QString &objName,
                                                      const QString &svgPath,
                                                      const QString &tipTitle,
                                                      const QKeySequence &shortcut,
                                                      const QString &tipDesc) const {
    const auto btn = buildCommonButton(objName, svgPath, tipTitle, shortcut, tipDesc);
    btn->setCheckable(true);
    return btn;
}

Button *ClipEditorToolBarViewPrivate::buildCommonButton(const QString &objName,
                                                        const QString &svgPath,
                                                        const QString &tipTitle,
                                                        const QKeySequence &shortcut,
                                                        const QString &tipDesc) const {
    const auto btn = new Button;
    btn->setObjectName(objName);
    btn->setFixedSize(36, m_contentHeight);
    btn->setShortcut(shortcut);

    const QSize iconSize(16, 16);
    IconUtils::SvgIconToggleColorPalette palette;
    palette.off.normal = QColor(240, 240, 240);
    palette.off.disabled = QColor(240, 240, 240, 102);
    palette.on.normal = QColor(155, 186, 255);
    palette.on.disabled = QColor(155, 186, 255, 102);
    btn->setIconSize(iconSize);
    btn->setIcon(IconUtils::createTintedSvgIcon(svgPath, iconSize, palette));

    btn->setToolTip(tipTitle);
    const auto toolTip = new ToolTipFilter(btn, 500, false, true);
    if (!tipDesc.isEmpty())
        toolTip->appendMessage(tipDesc);
    btn->installEventFilter(toolTip);
    return btn;
}

void ClipEditorToolBarViewPrivate::setPianoRollToolsEnabled(const bool on) const {
    for (const auto btn : m_toolButtonGroup->buttons()) {
        btn->setVisible(on);
        btn->setEnabled(on);
    }

    m_cbSinger->setVisible(on);
    m_cbSinger->setEnabled(on && appStatus->packageModuleStatus == AppStatus::ModuleStatus::Ready);
    m_cbClipLanguage->setVisible(on);
    m_cbClipLanguage->setEnabled(on);
    m_cbPianoRollQuantize->setVisible(on);
    m_cbPianoRollQuantize->setEnabled(on);

    if (on) {
        refreshSingerComboPresentation();
        connect(m_cbSinger, &TwoLevelComboBox::currentDataChanged, this,
                &ClipEditorToolBarViewPrivate::onSingerEdited);
        connect(m_singingClip, &SingingClip::singerOrSpeakerChanged, this,
                &ClipEditorToolBarViewPrivate::onClipSingerChanged);
        connect(m_singingClip, &SingingClip::speakerMixChanged, this,
                [this](const SpeakerMixData &) { refreshSingerComboPresentation(); });

        m_cbClipLanguage->setCurrentText(m_singingClip->defaultLanguage());
        connect(m_cbClipLanguage, &ComboBox::currentTextChanged, this,
                &ClipEditorToolBarViewPrivate::onLanguageEdited);
        connect(m_singingClip, &SingingClip::defaultLanguageChanged, m_cbClipLanguage,
                &ComboBox::setCurrentText);
    } else {
        disconnect(m_cbSinger, &TwoLevelComboBox::currentDataChanged, this,
                   &ClipEditorToolBarViewPrivate::onSingerEdited);
        if (m_singingClip) {
            disconnect(m_singingClip, &SingingClip::singerOrSpeakerChanged, this,
                       &ClipEditorToolBarViewPrivate::onClipSingerChanged);
            disconnect(m_singingClip, &SingingClip::speakerMixChanged, this, nullptr);
            disconnect(m_singingClip, &SingingClip::defaultLanguageChanged, m_cbClipLanguage,
                       &ComboBox::setCurrentText);
        }

        disconnect(m_cbClipLanguage, &ComboBox::currentTextChanged, this,
                   &ClipEditorToolBarViewPrivate::onLanguageEdited);
        m_cbClipLanguage->setCurrentText("unknown");
    }
}

void ClipEditorToolBarViewPrivate::onClipSingerChanged() const {
    refreshSingerComboPresentation();
}

void ClipEditorToolBarViewPrivate::onSingerEdited() const {
    if (m_singingClip) {
        if (m_cbSinger->isInheritSelected()) {
            m_singingClip->useTrackSingerAndSpeaker();
            m_cbSinger->setCurrentData(m_singingClip->singerInfo(), m_singingClip->speakerInfo(),
                                       true);
        } else {
            const auto singerInfo = m_cbSinger->currentSinger();
            const auto speakerInfo = m_cbSinger->currentSpeaker();
            m_singingClip->setOwnSingerAndSpeaker(singerInfo, speakerInfo);
        }
        m_cbSinger->setToolTip(m_cbSinger->currentText());
    }
}

void ClipEditorToolBarViewPrivate::refreshSingerComboPresentation() const {
    if (!m_singingClip || !m_cbSinger)
        return;

    const bool inherit = m_singingClip->useTrackSingerInfo.get();
    m_cbSinger->setCurrentData(m_singingClip->singerInfo(), m_singingClip->speakerInfo(), inherit);
    if (const auto text = SpeakerMixDisplayUtils::comboDisplayText(m_singingClip->singerInfo(),
                                                                   m_singingClip->speakerMixData());
        !text.isEmpty()) {
        m_cbSinger->setDisplayTextOverride(text);
    } else {
        m_cbSinger->clearDisplayTextOverride();
    }
    m_cbSinger->setToolTip(m_cbSinger->currentText());
    populatePresetMenus();
}

void ClipEditorToolBarViewPrivate::populatePresetMenus() const {
    if (!m_cbSinger)
        return;

    m_cbSinger->clearInjectedActions();
    const auto sourcePreset =
        m_singingClip ? SpeakerMixPresetStore::sourcePresetForData(m_singingClip->singerInfo(),
                                                                   m_singingClip->speakerMixData())
                      : std::optional<SpeakerMixPreset>();
    QAction *checkedPresetAction = nullptr;
    const auto packages = packageManager->installedPackages().successfulPackages;
    for (const auto &package : packages) {
        for (const auto &singerInfo : package.singers()) {
            if (singerInfo.speakers().size() < 2 || !m_cbSinger->groupMenuForSinger(singerInfo))
                continue;

            m_cbSinger->addInjectedSeparatorToSinger(singerInfo);
            const auto presets = SpeakerMixPresetStore::presetsForSinger(singerInfo);
            for (const auto &preset : presets) {
                const auto action = m_cbSinger->addInjectedActionToSinger(singerInfo, preset.name);
                if (!action)
                    continue;
                if (sourcePreset && preset.id == sourcePreset->id)
                    checkedPresetAction = action;
                connect(action, &QAction::triggered, this,
                        [this, presetId = preset.id] { onPresetApplied(presetId); });
            }

            m_cbSinger->addInjectedSeparatorToSinger(singerInfo);
            if (const auto action = m_cbSinger->addInjectedActionToSinger(
                    singerInfo, tr("Manage mix presets..."))) {
                connect(action, &QAction::triggered, this,
                        [this, singerInfo] { onManagePresetsAction(singerInfo); });
            }
        }
    }
    m_cbSinger->setCheckedInjectedAction(checkedPresetAction);
}

void ClipEditorToolBarViewPrivate::onPresetApplied(const QString &presetId) const {
    if (!m_singingClip)
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

    const auto speakerInfo = data.sources.first().speaker;
    const auto actions = new SpeakerMixActions;
    actions->applyClipSpeakerMixPreset(singerInfo, speakerInfo, data, m_singingClip);
    actions->execute();
    historyManager->record(actions);

    refreshSingerComboPresentation();
}

void ClipEditorToolBarViewPrivate::onManagePresetsAction(const SingerInfo &singerInfo) const {
    if (singerInfo.speakers().size() < 2)
        return;

    Q_Q(const ClipEditorToolBarView);
    auto *parent = Dialog::globalParent();
    if (!parent)
        parent = const_cast<ClipEditorToolBarView *>(q);

    const auto initialData =
        m_singingClip && m_singingClip->singerInfo().identifier() == singerInfo.identifier()
            ? m_singingClip->speakerMixData()
            : SpeakerMixData();
    SpeakerMixDialog dialog(singerInfo, initialData, parent);
    if (dialog.exec() == QDialog::Accepted && m_singingClip) {
        const auto data = dialog.speakerMixData();
        if (!SpeakerMixModel::isSpeakerMixDataSingle(data)) {
            const auto actions = new SpeakerMixActions;
            actions->applyClipSpeakerMixPreset(singerInfo, data.sources.first().speaker, data,
                                               m_singingClip);
            actions->execute();
            historyManager->record(actions);
        }
    }
    refreshSingerComboPresentation();
}