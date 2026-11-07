#include "ClipEditorToolBarView.h"
#include "ClipEditorToolBarView_p.h"

#include "AppContext.h"
#include "Automation/CoreRuntime.h"
#include "Controller/EditorViewController.h"
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
#include <lite/GUI/Controls/AdaptiveToolBar.h>
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
#include "UI/Views/Common/EditorShortcutUtils.h"

#include <QButtonGroup>
#include <QDialog>
#include <QEvent>
#include <QHBoxLayout>
#include <QLabel>
#include <QSignalBlocker>
#include <QAbstractItemView>
#include <QApplication>
#include <QKeyEvent>
#include <QPointer>
#include <QShortcut>

#include <optional>

namespace {

    class ToolShortcutOverrideFilter final : public QObject {
    public:
        ToolShortcutOverrideFilter(QWidget *owner,
                                   const QList<QPair<QShortcut *, Button *>> &shortcuts)
            : QObject(owner), m_owner(owner) {
            for (const auto &entry : shortcuts)
                m_shortcuts.append(entry.first);
            qApp->installEventFilter(this);
        }

    protected:
        bool eventFilter(QObject *watched, QEvent *event) override {
            if (event->type() != QEvent::ShortcutOverride || !m_owner->isVisible() ||
                !m_owner->isEnabled()) {
                return false;
            }

            auto *window = m_owner->window();
            auto *popup = QApplication::activePopupWidget();
            const bool ownPopup = popup && window->isAncestorOf(popup);
            if (popup && !ownPopup)
                return false;
            if (auto *modal = QApplication::activeModalWidget(); modal && modal != window)
                return false;
            if (QApplication::activeWindow() != window && !ownPopup)
                return false;

            auto *receiver = qobject_cast<QWidget *>(watched);
            if (!receiver ||
                (receiver->window() != window &&
                 (!ownPopup || (receiver != popup && !popup->isAncestorOf(receiver))))) {
                return false;
            }
            if (!ownPopup && !EditorShortcutUtils::isTextInput(QApplication::focusWidget())) {
                return false;
            }

            auto *keyEvent = static_cast<QKeyEvent *>(event);
            const QKeySequence key(keyEvent->keyCombination());
            for (const auto &shortcut : m_shortcuts) {
                if (shortcut && shortcut->isEnabled() && shortcut->key() == key) {
                    // Override only tool keys, preserving text entry and popup type-search.
                    keyEvent->accept();
                    return true;
                }
            }
            return false;
        }

    private:
        QWidget *m_owner;
        QList<QPointer<QShortcut>> m_shortcuts;
    };

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
    d->setParent(this);
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
    d->m_cbSinger->setFixedHeight(d->m_contentHeight);
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
                    d->m_cbSinger->setEnabled(d->m_singingClip != nullptr);
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
    d->m_cbClipLanguage->setFixedHeight(d->m_contentHeight);
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
            editorViewController->setPianoRollQuantize(appStatus->pianoRollQuantize, false);
            return;
        }
        editorViewController->setPianoRollQuantize(QuantizeOptions::values().at(index - 1), true);
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
    const auto tracePitchDesc =
        tr("Trace automatic pitch inference results onto the edited pitch curve");
    d->m_btnPitchTrace = d->buildToolButton("btnPitchTrace", ":svg/icons/pitch_brush.svg",
                                            tr("Trace Pitch"), Qt::Key_J, tracePitchDesc);
    const auto pitchModulationDesc =
        tr("Modulate pitch deviations from the smoothed note pitch curve");
    d->m_btnPitchModulation =
        d->buildToolButton("btnPitchModulation", ":svg/icons/pitch_modulation_24_filled.svg",
                           tr("Modulate Pitch"), Qt::Key_K, pitchModulationDesc);

    d->m_btnAutoPageTurn = d->buildToolButton(
        "btnAutoPageTurn", ":svg/icons/arrow_right_16_regular.svg", tr("Auto Page Turn"));
    connect(d->m_btnAutoPageTurn, &QPushButton::toggled, this,
            [](const bool checked) { editorViewController->setPianoRollAutoPageTurn(checked); });
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
    d->m_toolButtonGroup->addButton(d->m_btnPitchTrace);
    d->m_toolButtonGroup->addButton(d->m_btnPitchModulation);
    connect(d->m_toolButtonGroup, &QButtonGroup::buttonToggled, d,
            &ClipEditorToolBarViewPrivate::onPianoRollToolButtonToggled);

    d->m_adaptiveToolBar = new AdaptiveToolBar(this);
    d->m_adaptiveToolBar->setItemSpacing(1);
    d->m_adaptiveToolBar->setGroupSpacing(18);
    connect(d->m_adaptiveToolBar, &AdaptiveToolBar::widgetAboutToRelocate, d,
            [d](QWidget *widget, QWidget *destinationHost) {
                if (widget == d->m_leClipName) {
                    d->m_leClipName->finishEditing();
                    d->m_leClipName->setOverlayParent(destinationHost);
                }
            });

    d->m_adaptiveToolBar->addWidget(d->m_leClipName, {0, 0, 2});
    d->m_adaptiveToolBar->addWidget(d->m_cbSinger, {0, 1, 1});
    d->m_adaptiveToolBar->addWidget(d->m_cbClipLanguage, {0, 2, 0});
    d->m_adaptiveToolBar->addWidget(d->m_btnArrow, {1, 0, 12});
    d->m_adaptiveToolBar->addWidget(d->m_btnBeam, {1, 1, 11});
    d->m_adaptiveToolBar->addWidget(d->m_btnNotePencil, {1, 2, 10});
    d->m_adaptiveToolBar->addWidget(d->m_btnNoteEraser, {1, 3, 9});
    d->m_adaptiveToolBar->addWidget(d->m_btnNoteSplit, {1, 4, 8});
    d->m_adaptiveToolBar->addWidget(d->m_btnPitchAnchor, {1, 5, 7});
    d->m_adaptiveToolBar->addWidget(d->m_btnPitchPencil, {1, 6, 6});
    d->m_adaptiveToolBar->addWidget(d->m_btnPitchEraser, {1, 7, 5});
    d->m_adaptiveToolBar->addWidget(d->m_btnPitchTrace, {1, 8, 4});
    d->m_adaptiveToolBar->addWidget(d->m_btnPitchModulation, {1, 9, 3});
    d->m_adaptiveToolBar->addWidget(d->m_cbPianoRollQuantize, {2, 0, 14});
    d->m_adaptiveToolBar->addWidget(d->m_btnAutoPageTurn, {3, 0, 13});

    for (int group = 0; group < 4; ++group) {
        auto *moreButton = d->m_adaptiveToolBar->overflowButton(group);
        moreButton->setText({});
        d->configureCommonButton(moreButton, ":svg/icons/more_horizontal_24_regular.svg",
                                 tr("More Controls"));
        moreButton->setFixedWidth(28);
        moreButton->setAccessibleName(tr("More Controls"));
    }
    new ToolShortcutOverrideFilter(this, d->m_toolShortcuts);

    const auto mainLayout = new QHBoxLayout;
    mainLayout->addWidget(d->m_adaptiveToolBar);
    mainLayout->setContentsMargins({});
    mainLayout->setSpacing(0);
    setLayout(mainLayout);
    setFixedHeight(d->m_contentHeight);

    d->moveToNullClipState();
}

void ClipEditorToolBarView::setDataContext(Clip *clip) {
    Q_D(ClipEditorToolBarView);
    // Commit any in-progress edit before switching clips
    d->m_leClipName->finishEditing();
    d->m_adaptiveToolBar->closeOverflow();
    d->m_editingClipId = -1;

    if (d->m_clip)
        disconnect(d->m_clip, nullptr, d, nullptr);

    d->m_clip = clip;
    if (clip == nullptr) {
        d->m_singingClip = nullptr;
        d->moveToNullClipState();
    } else {
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
    // Keep context applicability synchronous while resize work remains coalesced.
    d->m_adaptiveToolBar->layout()->invalidate();
    d->m_adaptiveToolBar->layout()->activate();
}

PianoRollEditMode ClipEditorToolBarView::editMode() const {
    Q_D(const ClipEditorToolBarView);
    return d->m_editMode;
}

bool ClipEditorToolBarView::supportsEditMode(const PianoRollEditMode mode) const {
    return mode >= Select && mode <= ModulatePitch;
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
        case TracePitch:
            button = d->m_btnPitchTrace;
            break;
        case ModulatePitch:
            button = d->m_btnPitchModulation;
            break;
    }

    if (!button)
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
    } else if (button == m_btnPitchTrace) {
        m_editMode = TracePitch;
    } else if (button == m_btnPitchModulation) {
        m_editMode = ModulatePitch;
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
    setItemsApplicable(false, false);
}

void ClipEditorToolBarViewPrivate::moveToSingingClipState() const {
    m_leClipName->setEnabled(true);
    m_leClipName->setText(m_clip->name());

    setItemsApplicable(true, true);
    setPianoRollToolsEnabled(true);
}

void ClipEditorToolBarViewPrivate::moveToAudioClipState() const {
    m_leClipName->setEnabled(true);
    m_leClipName->setText(m_clip->name());

    // An audio clip has no notes, no pitch anchors and no quantize, so the only
    // control that applies to it is its name
    setItemsApplicable(true, false);
    setPianoRollToolsEnabled(false);
}

void ClipEditorToolBarViewPrivate::setItemsApplicable(const bool name, const bool singing) const {
    m_adaptiveToolBar->setItemApplicable(m_leClipName, name);
    m_adaptiveToolBar->setItemApplicable(m_cbSinger, singing);
    m_adaptiveToolBar->setItemApplicable(m_cbClipLanguage, singing);
    for (auto *button : m_toolButtonGroup->buttons())
        m_adaptiveToolBar->setItemApplicable(button, singing);
    m_adaptiveToolBar->setItemApplicable(m_cbPianoRollQuantize, singing);
    m_adaptiveToolBar->setItemApplicable(m_btnAutoPageTurn, singing);
    updateToolShortcuts();
}

void ClipEditorToolBarViewPrivate::updateToolShortcuts() const {
    Q_Q(const ClipEditorToolBarView);
    const bool available = q->isVisible() && q->isEnabled() && m_singingClip;
    for (const auto &[shortcut, button] : m_toolShortcuts)
        shortcut->setEnabled(available && button->isEnabled() &&
                             m_adaptiveToolBar->isItemApplicable(button));
}

Button *ClipEditorToolBarViewPrivate::buildToolButton(const QString &objName,
                                                      const QString &svgPath,
                                                      const QString &tipTitle,
                                                      const QKeySequence &shortcut,
                                                      const QString &tipDesc) const {
    const auto btn = new Button;
    btn->setObjectName(objName);
    configureCommonButton(btn, svgPath, tipTitle, shortcut, tipDesc);
    btn->setCheckable(true);
    return btn;
}

void ClipEditorToolBarViewPrivate::configureCommonButton(Button *btn, const QString &svgPath,
                                                         const QString &tipTitle,
                                                         const QKeySequence &shortcut,
                                                         const QString &tipDesc) const {
    btn->setFixedSize(36, m_contentHeight);
    if (!shortcut.isEmpty()) {
        auto *toolShortcut = new QShortcut(shortcut, q_ptr);
        toolShortcut->setContext(Qt::WindowShortcut);
        m_toolShortcuts.append({toolShortcut, btn});
        connect(toolShortcut, &QShortcut::activated, q_ptr, [this, btn] {
            if (q_ptr->isVisible() && q_ptr->isEnabled() && m_singingClip && btn->isEnabled() &&
                m_adaptiveToolBar->isItemApplicable(btn) && !QApplication::activePopupWidget() &&
                !EditorShortcutUtils::isTextInput(QApplication::focusWidget())) {
                btn->click();
            }
        });
    }

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
    toolTip->setShortcutKey(shortcut.toString());
    if (!tipDesc.isEmpty())
        toolTip->appendMessage(tipDesc);
    btn->installEventFilter(toolTip);
}

void ClipEditorToolBarViewPrivate::setPianoRollToolsEnabled(const bool on) const {
    for (const auto btn : m_toolButtonGroup->buttons())
        btn->setEnabled(on);

    m_cbSinger->setEnabled(on && appStatus->packageModuleStatus == AppStatus::ModuleStatus::Ready);
    m_cbClipLanguage->setEnabled(on);
    m_cbPianoRollQuantize->setEnabled(on);
    updateToolShortcuts();

    if (on) {
        refreshSingerComboPresentation();
        connect(m_cbSinger, &TwoLevelComboBox::currentDataChanged, this,
                &ClipEditorToolBarViewPrivate::onSingerEdited, Qt::UniqueConnection);
        connect(m_singingClip, &SingingClip::voiceContextChanged, this,
                [this](const VoiceContextChange &) { refreshSingerComboPresentation(); });

        connect(m_cbClipLanguage, &LanguageComboBox::currentLanguageChanged, this,
                &ClipEditorToolBarViewPrivate::onLanguageEdited, Qt::UniqueConnection);
        connect(m_singingClip, &SingingClip::defaultLanguageChanged, this,
                &ClipEditorToolBarViewPrivate::onClipLanguageChanged);
    } else {
        disconnect(m_cbSinger, &TwoLevelComboBox::currentDataChanged, this,
                   &ClipEditorToolBarViewPrivate::onSingerEdited);
        if (m_singingClip) {
            disconnect(m_singingClip, &SingingClip::voiceContextChanged, this, nullptr);
            disconnect(m_singingClip, &SingingClip::defaultLanguageChanged, this,
                       &ClipEditorToolBarViewPrivate::onClipLanguageChanged);
        }

        disconnect(m_cbClipLanguage, &LanguageComboBox::currentLanguageChanged, this,
                   &ClipEditorToolBarViewPrivate::onLanguageEdited);
        m_cbClipLanguage->setLanguages({}, QStringLiteral("unknown"));
        m_adaptiveToolBar->invalidateItemSize(m_cbClipLanguage);
    }
}

void ClipEditorToolBarViewPrivate::onSingerEdited() const {
    if (!m_singingClip)
        return;

    if (m_cbSinger->isInheritSelected()) {
        if (auto *runtime = AppContext::instance<Automation::CoreRuntime>())
            runtime->parameters().useTrackVoiceContext(commandContext(*runtime),
                                                       Automation::ClipId(m_singingClip->id()));
    } else {
        const auto singerInfo = m_cbSinger->currentSinger();
        const auto speakerInfo = m_cbSinger->currentSpeaker();
        if (auto *runtime = AppContext::instance<Automation::CoreRuntime>())
            runtime->parameters().selectClipSingleSpeaker(commandContext(*runtime),
                                                          Automation::ClipId(m_singingClip->id()),
                                                          singerInfo, speakerInfo);
    }
    refreshSingerComboPresentation();
}

void ClipEditorToolBarViewPrivate::refreshSingerComboPresentation() const {
    if (!m_singingClip || !m_cbSinger) {
        if (m_adaptiveToolBar && m_cbSinger)
            m_adaptiveToolBar->invalidateItemSize(m_cbSinger);
        return;
    }

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
    m_adaptiveToolBar->invalidateItemSize(m_cbSinger);
    refreshLanguageComboPresentation();
}

void ClipEditorToolBarViewPrivate::refreshLanguageComboPresentation() const {
    if (!m_singingClip || !m_cbClipLanguage)
        return;

    const auto singerInfo = m_singingClip->singerInfo();
    const auto language = m_cbClipLanguage->setLanguages(
        singerInfo.languages(), m_singingClip->defaultLanguage(), singerInfo.defaultLanguage());
    m_adaptiveToolBar->invalidateItemSize(m_cbClipLanguage);
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
        runtime->parameters().applyClipSpeakerMix(commandContext(*runtime),
                                                  Automation::ClipId(m_singingClip->id()),
                                                  singerInfo, speakerInfo, data);

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

bool ClipEditorToolBarView::event(QEvent *event) {
    const auto handled = QWidget::event(event);
    if (event->type() == QEvent::Show || event->type() == QEvent::Hide ||
        event->type() == QEvent::EnabledChange) {
        d_ptr->updateToolShortcuts();
    }
    return handled;
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
    setToolTip(m_btnPitchTrace, ClipEditorToolBarView::tr("Trace Pitch"),
               ClipEditorToolBarView::tr(
                   "Trace automatic pitch inference results onto the edited pitch curve"));
    setToolTip(
        m_btnPitchModulation, ClipEditorToolBarView::tr("Modulate Pitch"),
        ClipEditorToolBarView::tr("Modulate pitch deviations from the smoothed note pitch curve"));
    setToolTip(m_btnAutoPageTurn, ClipEditorToolBarView::tr("Auto Page Turn"));
    for (int group = 0; group < 4; ++group) {
        auto *moreButton = m_adaptiveToolBar->overflowButton(group);
        setToolTip(moreButton, ClipEditorToolBarView::tr("More Controls"));
        moreButton->setAccessibleName(ClipEditorToolBarView::tr("More Controls"));
    }
    refreshSingerComboPresentation();
    m_adaptiveToolBar->invalidateItemSize(m_cbSinger);
    m_adaptiveToolBar->invalidateItemSize(m_cbClipLanguage);
    m_adaptiveToolBar->invalidateItemSize(m_cbPianoRollQuantize);
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
