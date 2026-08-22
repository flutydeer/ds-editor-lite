#include "ClipEditorToolBarView.h"
#include "ClipEditorToolBarView_p.h"

#include "AppContext.h"
#include "Automation/CoreRuntime.h"
#include "Controller/TrackController.h"
#include <lite/ProjectModel/AppModel/AppModel.h>
#include <lite/ProjectModel/AppModel/SingingClip.h>
#include "Model/AppOptions/AppOptions.h"
#include "Model/SpeakerMixPreset/SpeakerMixPresetStore.h"
#include "Model/AppStatus/AppStatus.h"
#include <lite/ProjectModel/AppModel/SingerIdentifier.h>
#include <lite/PackageManager/PackageManager.h>
#include <lite/GUI/Controls/Button.h>
#include <lite/GUI/Controls/ComboBox.h>
#include <lite/GUI/Controls/ControlGroup.h>
#include <lite/GUI/Controls/InlineEditLabel.h>
#include <lite/GUI/Controls/SvsSeekbar.h>
#include <lite/GUI/Controls/ToolTipFilter.h>
#include <lite/GUI/Controls/Toast.h>
#include "UI/Controls/TwoLevelComboBox.h"
#include "UI/Dialogs/SpeakerMix/SpeakerMixDialog.h"
#include <lite/GUI/Utils/IconUtils.h>
#include "UI/Utils/SpeakerMixDisplayUtils.h"
#include "UI/Utils/QuantizeOptions.h"
#include "UI/Views/Common/LanguageComboBox.h"

#include <QButtonGroup>
#include <QDialog>
#include <QEvent>
#include <QHBoxLayout>
#include <QLabel>
#include <QSignalBlocker>
#include <QAbstractItemView>

#include <optional>

namespace {

    int quantizeIndex(int quantize) {
        return QuantizeOptions::indexOf(quantize);
    }

    Automation::CommandContext commandContext(const Automation::CoreRuntime &runtime) {
        return {.expected = runtime.documentVersion(),
                .source = Automation::InvocationSource::TrustedGui};
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
    connect(appOptions, &AppOptions::optionsChanged, d, [d](AppOptionsGlobal::Option option) {
        if (option == AppOptionsGlobal::Option::General || option == AppOptionsGlobal::Option::All)
            d->refreshSingerComboPresentation();
    });

    d->m_cbClipLanguage = new LanguageComboBox("unknown", WheelEventPolicy::Handle);
    d->m_cbClipLanguage->setObjectName("cbClipLanguage");
    d->m_cbClipLanguage->installEventFilter(new ToolTipFilter(d->m_cbClipLanguage, 500));
    d->m_cbClipLanguage->setToolTip(tr("Clip Default Language"));

    d->m_cbPianoRollQuantize = new ComboBox(WheelEventPolicy::Handle);
    d->m_cbPianoRollQuantize->setObjectName("cbPianoRollQuantize");
    QStringList quantizeItems{tr("(No Quantize)")};
    quantizeItems.append(QuantizeOptions::strings());
    d->m_cbPianoRollQuantize->addItems(quantizeItems);
    // Scroll the popup list per pixel (pairs with global smooth scrolling;
    // per-line scrolling feels like jumping a lot)
    d->m_cbPianoRollQuantize->view()->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
    d->m_cbPianoRollQuantize->setFixedHeight(d->m_contentHeight);
    d->m_cbPianoRollQuantize->setToolTip(tr("Piano Roll Quantize"));
    connect(d->m_cbPianoRollQuantize, &QComboBox::currentIndexChanged, this, [](int index) {
        if (index <= 0) {
            appStatus->pianoRollQuantizeEnabled = false;
            return;
        }
        appStatus->pianoRollQuantizeEnabled = true;
        appStatus->pianoRollQuantize = QuantizeOptions::values().at(index - 1);
    });
    connect(appStatus, &AppStatus::pianoRollQuantizeChanged, d->m_cbPianoRollQuantize,
            [combo = d->m_cbPianoRollQuantize](int quantize) {
                const QSignalBlocker blocker(combo);
                combo->setCurrentIndex(1 + quantizeIndex(quantize));
            });
    connect(appStatus, &AppStatus::pianoRollQuantizeEnabledChanged, d->m_cbPianoRollQuantize,
            [combo = d->m_cbPianoRollQuantize](const bool enabled) {
                const QSignalBlocker blocker(combo);
                combo->setCurrentIndex(enabled ? 1 + quantizeIndex(appStatus->pianoRollQuantize)
                                               : 0);
            });
    d->m_cbPianoRollQuantize->setCurrentIndex(
        appStatus->pianoRollQuantizeEnabled ? 1 + quantizeIndex(appStatus->pianoRollQuantize) : 0);


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
    const auto bakePitchDesc =
        tr("Bake automatic pitch inference results into the edited pitch curve");
    d->m_btnPitchBake = d->buildToolButton("btnPitchBake", ":svg/icons/pitch_brush.svg",
                                           tr("Bake Pitch"), Qt::Key_J, bakePitchDesc);

    d->m_btnAutoPageTurn = d->buildToolButton(
        "btnAutoPageTurn", ":svg/icons/arrow_right_16_regular.svg", tr("Auto Page Turn"));
    connect(d->m_btnAutoPageTurn, &QPushButton::toggled, this,
            [](const bool checked) { appStatus->pianoRollAutoPageTurnEnabled = checked; });
    connect(appStatus, &AppStatus::pianoRollAutoPageTurnEnabledChanged, this,
            [btn = d->m_btnAutoPageTurn](const bool enabled) { btn->setChecked(enabled); });
    connect(appStatus, &AppStatus::pianoRollAutoPageTurnAvailabilityChanged, this,
            [btn = d->m_btnAutoPageTurn](const bool available) {
                if (btn->property("autoPageTurnAvailable").toBool() != available ||
                    !btn->property("autoPageTurnAvailable").isValid()) {
                    btn->setProperty("autoPageTurnAvailable", available);
                    btn->style()->unpolish(btn);
                    btn->style()->polish(btn);
                    btn->update();
                }
            });
    d->m_btnAutoPageTurn->setChecked(appStatus->pianoRollAutoPageTurnEnabled);

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
    d->m_toolButtonGroup->addButton(d->m_btnPitchBake);
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
    toolButtonLayout->addWidget(d->m_btnPitchBake);
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

    auto autoPageTurnLayout = new QHBoxLayout;
    autoPageTurnLayout->addWidget(d->m_btnAutoPageTurn);
    autoPageTurnLayout->setSpacing(1);
    autoPageTurnLayout->setContentsMargins({});

    auto autoPageTurnGroup = new ControlGroup;
    autoPageTurnGroup->setLayout(autoPageTurnLayout);


    const auto mainLayout = new QHBoxLayout;
    mainLayout->addWidget(clipInfoGroup);
    mainLayout->addSpacing(16);
    mainLayout->addWidget(toolButtonGroup);
    mainLayout->addSpacing(16);
    mainLayout->addWidget(quantizeGroup);
    mainLayout->addSpacing(16);
    mainLayout->addWidget(autoPageTurnGroup);
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

bool ClipEditorToolBarView::supportsEditMode(const PianoRollEditMode mode) const {
    Q_D(const ClipEditorToolBarView);
    if (mode < Select || mode > BakePitch)
        return false;
    return mode != BakePitch || (d->m_btnPitchBake && d->m_btnPitchBake->isEnabled());
}

bool ClipEditorToolBarView::setEditMode(const PianoRollEditMode mode) {
    Q_D(ClipEditorToolBarView);
    if (!supportsEditMode(mode))
        return false;

    Button *button = nullptr;
    switch (mode) {
        case Select:
            button = d->m_btnArrow;
            break;
        case IntervalSelect:
            button = d->m_btnBeam;
            break;
        case DrawNote:
            button = d->m_btnNotePencil;
            break;
        case EraseNote:
            button = d->m_btnNoteEraser;
            break;
        case SplitNote:
            button = d->m_btnNoteSplit;
            break;
        case DrawPitch:
            button = d->m_btnPitchPencil;
            break;
        case EditPitchAnchor:
            button = d->m_btnPitchAnchor;
            break;
        case ErasePitch:
            button = d->m_btnPitchEraser;
            break;
        case BakePitch:
            button = d->m_btnPitchBake;
            break;
    }

    if (!button || (mode == BakePitch && !button->isEnabled()))
        return false;
    button->setChecked(true);
    return true;
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
    } else if (button == m_btnPitchBake) {
        m_editMode = BakePitch;
    } else {
        return;
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
    m_cbClipLanguage->setCurrentLanguage(language);
}

void ClipEditorToolBarViewPrivate::onLanguageEdited(const QString &language) const {
    if (!m_singingClip || m_singingClip->defaultLanguage() == language)
        return;
    if (auto *runtime = AppContext::instance<Automation::CoreRuntime>())
        runtime->project().setSingingClipDefaultLanguage(
            commandContext(*runtime), Automation::ClipId(m_singingClip->id()), language);
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
    palette.off.normal = m_iconColor;
    palette.off.disabled = m_iconDisabledColor;
    palette.on.normal = m_iconOnColor;
    palette.on.disabled = m_iconOnDisabledColor;
    btn->setIconSize(iconSize);
    btn->setIcon(IconUtils::createTintedSvgIcon(svgPath, iconSize, palette));
    m_tintedButtons.append({btn, svgPath});

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
        connect(m_singingClip, &SingingClip::voiceContextChanged, this,
                [this](const VoiceContextChange &) {
                    refreshSingerComboPresentation();
                    refreshPitchBakeAvailability();
                });
        connect(m_singingClip, &SingingClip::paramChanged, this,
                [this](const ParamInfo::Name name, const Param::Type type) {
                    if (name == ParamInfo::Pitch && type == Param::Original)
                        refreshPitchBakeAvailability();
                });

        connect(m_cbClipLanguage, &LanguageComboBox::currentLanguageChanged, this,
                &ClipEditorToolBarViewPrivate::onLanguageEdited);
        connect(m_singingClip, &SingingClip::defaultLanguageChanged, this,
                &ClipEditorToolBarViewPrivate::onClipLanguageChanged);
    } else {
        disconnect(m_cbSinger, &TwoLevelComboBox::currentDataChanged, this,
                   &ClipEditorToolBarViewPrivate::onSingerEdited);
        if (m_singingClip) {
            disconnect(m_singingClip, &SingingClip::voiceContextChanged, this, nullptr);
            disconnect(m_singingClip, &SingingClip::paramChanged, this, nullptr);
            disconnect(m_singingClip, &SingingClip::defaultLanguageChanged, this,
                       &ClipEditorToolBarViewPrivate::onClipLanguageChanged);
        }

        disconnect(m_cbClipLanguage, &LanguageComboBox::currentLanguageChanged, this,
                   &ClipEditorToolBarViewPrivate::onLanguageEdited);
        m_cbClipLanguage->setLanguages({}, QStringLiteral("unknown"));
    }
    refreshPitchBakeAvailability();
}

void ClipEditorToolBarViewPrivate::refreshPitchBakeAvailability() const {
    const auto *pitch = m_singingClip && !m_singingClip->singerInfo().isEmpty()
                            ? m_singingClip->params.getParamByName(ParamInfo::Pitch)
                            : nullptr;
    const bool available = pitch && !pitch->curves(Param::Original).isEmpty();
    m_btnPitchBake->setEnabled(available);
    if (!available && m_btnPitchBake->isChecked())
        m_btnPitchPencil->setChecked(true);
}

void ClipEditorToolBarViewPrivate::onSingerEdited() const {
    if (!m_singingClip)
        return;

    if (m_cbSinger->isInheritSelected()) {
        if (auto *runtime = AppContext::instance<Automation::CoreRuntime>())
            runtime->parameters().useTrackVoiceContext(
                commandContext(*runtime), Automation::ClipId(m_singingClip->id()));
    } else {
        const auto singerInfo = m_cbSinger->currentSinger();
        const auto speakerInfo = m_cbSinger->currentSpeaker();
        if (auto *runtime = AppContext::instance<Automation::CoreRuntime>())
            runtime->parameters().selectClipSingleSpeaker(
                commandContext(*runtime), Automation::ClipId(m_singingClip->id()), singerInfo,
                speakerInfo);
    }
    refreshSingerComboPresentation();
}

void ClipEditorToolBarViewPrivate::refreshSingerComboPresentation() const {
    if (!m_singingClip || !m_cbSinger)
        return;

    const bool inherit = m_singingClip->usesTrackVoiceContext();
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
    refreshLanguageComboPresentation();
}

void ClipEditorToolBarViewPrivate::refreshLanguageComboPresentation() const {
    if (!m_singingClip || !m_cbClipLanguage)
        return;

    const auto singerInfo = m_singingClip->singerInfo();
    const auto language = m_cbClipLanguage->setLanguages(
        singerInfo.languages(), m_singingClip->defaultLanguage(), singerInfo.defaultLanguage());
    // latch singer default language only when singer is resolved; do not write back without a
    // singer
    if (singerInfo.resolutionState() == ResolutionState::Resolved &&
        language != m_singingClip->defaultLanguage()) {
        if (auto *runtime = AppContext::instance<Automation::CoreRuntime>())
            runtime->project().setSingingClipDefaultLanguage(
                commandContext(*runtime), Automation::ClipId(m_singingClip->id()), language);
    }
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
    if (auto *runtime = AppContext::instance<Automation::CoreRuntime>())
        runtime->parameters().applyClipSpeakerMix(
            commandContext(*runtime), Automation::ClipId(m_singingClip->id()), singerInfo,
            speakerInfo, data);

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
            if (auto *runtime = AppContext::instance<Automation::CoreRuntime>())
                runtime->parameters().applyClipSpeakerMix(
                    commandContext(*runtime), Automation::ClipId(m_singingClip->id()), singerInfo,
                    data.sources.first().speaker, data);
        }
    }
    refreshSingerComboPresentation();
}

void ClipEditorToolBarView::changeEvent(QEvent *event) {
    QWidget::changeEvent(event);
    if (event->type() == QEvent::LanguageChange)
        d_ptr->retranslateUi();
}

void ClipEditorToolBarViewPrivate::retranslateUi() const {
    m_leClipName->setToolTip(ClipEditorToolBarView::tr("Clip Name"));
    m_cbSinger->setToolTip(ClipEditorToolBarView::tr("Clip Singer"));
    if (appStatus->packageModuleStatus != AppStatus::ModuleStatus::Ready)
        m_cbSinger->setLoadingText(ClipEditorToolBarView::tr("(Scanning packages...)"));
    else
        m_cbSinger->setItems(packageManager->installedPackages().successfulPackages);
    m_cbClipLanguage->setToolTip(ClipEditorToolBarView::tr("Clip Default Language"));
    m_cbPianoRollQuantize->setToolTip(ClipEditorToolBarView::tr("Piano Roll Quantize"));

    const auto setToolTip = [](Button *button, const QString &title,
                               const QString &description = {}) {
        button->setToolTip(title);
        for (auto *object : button->children()) {
            if (auto *filter = dynamic_cast<ToolTipFilter *>(object)) {
                filter->clearMessage();
                if (!description.isEmpty())
                    filter->appendMessage(description);
            }
        }
    };
    setToolTip(m_btnArrow, ClipEditorToolBarView::tr("Select"));
    setToolTip(m_btnBeam, ClipEditorToolBarView::tr("Interval Select"));
    setToolTip(m_btnNotePencil, ClipEditorToolBarView::tr("Draw Note"),
               ClipEditorToolBarView::tr(
                   "Drag in the blank: Draw a new note\nDrag on a note: Edit the note"));
    setToolTip(m_btnNoteEraser, ClipEditorToolBarView::tr("Erase Note"));
    setToolTip(m_btnNoteSplit, ClipEditorToolBarView::tr("Split Note"),
               ClipEditorToolBarView::tr("Split note at quantize line"));
    setToolTip(m_btnPitchAnchor, ClipEditorToolBarView::tr("Pitch Anchor"));
    setToolTip(m_btnPitchPencil, ClipEditorToolBarView::tr("Draw Pitch"),
               ClipEditorToolBarView::tr("Left drag: Draw\nRight drag: Erase"));
    setToolTip(m_btnPitchEraser, ClipEditorToolBarView::tr("Erase Pitch"));
    setToolTip(m_btnPitchBake, ClipEditorToolBarView::tr("Bake Pitch"),
               ClipEditorToolBarView::tr(
                   "Bake automatic pitch inference results into the edited pitch curve"));
    refreshSingerComboPresentation();
}

void ClipEditorToolBarViewPrivate::rebuildIcons() const {
    const QSize iconSize(16, 16);
    IconUtils::SvgIconToggleColorPalette palette;
    palette.off.normal = m_iconColor;
    palette.off.disabled = m_iconDisabledColor;
    palette.on.normal = m_iconOnColor;
    palette.on.disabled = m_iconOnDisabledColor;
    for (const auto &[btn, svgPath] : m_tintedButtons)
        btn->setIcon(IconUtils::createTintedSvgIcon(svgPath, iconSize, palette));
}

QColor ClipEditorToolBarView::iconColor() const {
    Q_D(const ClipEditorToolBarView);
    return d->m_iconColor;
}

void ClipEditorToolBarView::setIconColor(const QColor &color) {
    Q_D(ClipEditorToolBarView);
    if (d->m_iconColor == color)
        return;
    d->m_iconColor = color;
    d->rebuildIcons();
}

QColor ClipEditorToolBarView::iconDisabledColor() const {
    Q_D(const ClipEditorToolBarView);
    return d->m_iconDisabledColor;
}

void ClipEditorToolBarView::setIconDisabledColor(const QColor &color) {
    Q_D(ClipEditorToolBarView);
    if (d->m_iconDisabledColor == color)
        return;
    d->m_iconDisabledColor = color;
    d->rebuildIcons();
}

QColor ClipEditorToolBarView::iconOnColor() const {
    Q_D(const ClipEditorToolBarView);
    return d->m_iconOnColor;
}

void ClipEditorToolBarView::setIconOnColor(const QColor &color) {
    Q_D(ClipEditorToolBarView);
    if (d->m_iconOnColor == color)
        return;
    d->m_iconOnColor = color;
    d->rebuildIcons();
}

QColor ClipEditorToolBarView::iconOnDisabledColor() const {
    Q_D(const ClipEditorToolBarView);
    return d->m_iconOnDisabledColor;
}

void ClipEditorToolBarView::setIconOnDisabledColor(const QColor &color) {
    Q_D(ClipEditorToolBarView);
    if (d->m_iconOnDisabledColor == color)
        return;
    d->m_iconOnDisabledColor = color;
    d->rebuildIcons();
}
