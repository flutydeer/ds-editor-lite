//
// Created by fluty on 2024/2/10.
//

#include "ClipEditorToolBarView.h"
#include "ClipEditorToolBarView_p.h"
#include "Controller/AppController.h"

#include "Controller/TracksViewController.h"
#include "Model/AppModel/AppModel.h"
#include "Model/AppModel/Clip.h"
#include "Model/AppStatus/AppStatus.h"
#include "UI/Controls/Button.h"
#include "UI/Controls/DividerLine.h"
#include "UI/Controls/EditLabel.h"
#include "UI/Controls/LineEdit.h"
#include "UI/Controls/ToolTipFilter.h"
#include "UI/Views/Common/LanguageComboBox.h"
#include "Utils/SystemUtils.h"

#include <QButtonGroup>
#include <QHBoxLayout>
#include <SVSCraftWidgets/seekbar.h>
#include <QLabel>

ClipEditorToolBarView::ClipEditorToolBarView(QWidget *parent)
    : QWidget(parent), d_ptr(new ClipEditorToolBarViewPrivate(this)) {
    Q_D(ClipEditorToolBarView);
    setObjectName("ClipEditorToolBarView");
    setFocusPolicy(Qt::ClickFocus);

    d->m_leClipName = new LineEdit;
    d->m_leClipName->setObjectName("ClipNameEditLabel");
    d->m_leClipName->installEventFilter(new ToolTipFilter(d->m_leClipName, 500));
    d->m_leClipName->setToolTip(tr("Clip Name"));
    d->m_leClipName->setFixedWidth(128);
    d->m_leClipName->setFixedHeight(d->m_contentHeight);
    d->m_leClipName->setEnabled(false);
    d->m_leClipName->setText("");
    connect(d->m_leClipName, &QLineEdit::editingFinished, d,
            &ClipEditorToolBarViewPrivate::onClipNameEdited);

    d->m_cbClipLanguage = new LanguageComboBox(languageKeyFromType(AppGlobal::unknown), true);
    d->m_cbClipLanguage->installEventFilter(new ToolTipFilter(d->m_cbClipLanguage, 500));
    d->m_cbClipLanguage->setToolTip(tr("Clip Default Language"));

    d->m_btnArrow = d->buildToolButton("btnArrow", tr("Select"), Qt::Key_V);
    d->m_btnArrow->setChecked(true);
    d->m_btnBeam = d->buildToolButton("btnBeam", tr("Interval Select"), Qt::Key_B);
    auto notePencilDesc = tr("Drag in the blank: Draw a new note\nDrag on a note: Edit the note");
    d->m_btnNotePencil =
        d->buildToolButton("btnNotePencil", tr("Draw Note"), Qt::Key_N, notePencilDesc);
    d->m_btnNoteEraser = d->buildToolButton("btnNoteEraser", tr("Erase Note"), Qt::Key_M);
    d->m_btnPitchAnchor = d->buildToolButton("btnPitchAnchor", tr("Pitch Anchor"), Qt::Key_F);
    auto pitchPencilDesc = tr("Left drag: Draw\nRight drag: Erase");
    d->m_btnPitchPencil =
        d->buildToolButton("btnPitchPencil", tr("Draw Pitch"), Qt::Key_G, pitchPencilDesc);
    d->m_btnPitchEraser = d->buildToolButton("btnPitchEraser", tr("Erase Pitch"), Qt::Key_H);
    auto freezePitchDesc = tr("Copy automatic pitch inference results to edited pitch");
    d->m_btnFreezePitch =
        d->buildToolButton("btnFreezePitch", tr("Freeze Pitch"), Qt::Key_J, freezePitchDesc);

    auto buttonGroup = new QButtonGroup;
    buttonGroup->setExclusive(true);
    buttonGroup->addButton(d->m_btnArrow);
    buttonGroup->addButton(d->m_btnBeam);
    buttonGroup->addButton(d->m_btnNotePencil);
    buttonGroup->addButton(d->m_btnNoteEraser);
    buttonGroup->addButton(d->m_btnPitchAnchor);
    buttonGroup->addButton(d->m_btnPitchPencil);
    buttonGroup->addButton(d->m_btnPitchEraser);
    buttonGroup->addButton(d->m_btnFreezePitch);
    connect(buttonGroup, &QButtonGroup::buttonToggled, d,
            &ClipEditorToolBarViewPrivate::onPianoRollToolButtonToggled);


    auto sbGain = new SVS::SeekBar;
    // sbGain->setObjectName("m_sbarGain");
    sbGain->setMaximum(100); // +6dB
    sbGain->setMinimum(0);   // -inf
    sbGain->setDefaultValue(79.4328234724);
    sbGain->setValue(79.4328234724);
    sbGain->setTracking(false);
    sbGain->setFixedWidth(d->m_contentHeight * 4);

    auto leGain = new EditLabel();
    leGain->setText("0.0dB");
    leGain->setObjectName("leGain");
    leGain->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Minimum);
    leGain->label->setAlignment(Qt::AlignCenter);
    leGain->lineEdit->setAlignment(Qt::AlignCenter);
    leGain->setFixedWidth(d->m_contentHeight * 2);
    leGain->setFixedHeight(d->m_contentHeight);
    leGain->setEnabled(false);

    auto btnMute = d->buildToolButton("btnMute", tr("Mute or unmute clip"));
    btnMute->setText("M");

    auto btnPhonemeView = d->buildToolButton("btnPhonemeView", tr("Toggle phoneme editor"));
    btnPhonemeView->setChecked(true);

    auto btnParamView = d->buildToolButton("btnParamView", tr("Toggle param editor"));
    btnParamView->setChecked(true);

    auto btnMaximize = d->buildToolButton("btnPanelMaximize", tr("Maximize or restore"));
    connect(btnMaximize, &Button::clicked, this, [=] {
        if (appStatus->trackPanelCollapsed)
            appController->setTrackAndClipPanelCollapsed(false, false);
        else
            appController->setTrackAndClipPanelCollapsed(true, false);
    });
    connect(appStatus, &AppStatus::trackPanelCollapseStateChanged, btnMaximize,
            &Button::setChecked);

    auto btnHide = d->buildCommonButton("btnPanelHide", tr("Hide"));
    connect(btnHide, &Button::clicked, this,
            [=] { appController->setTrackAndClipPanelCollapsed(false, true); });

    auto mainLayout = new QHBoxLayout;
    mainLayout->addWidget(d->m_leClipName);
    mainLayout->addWidget(btnMute);
    mainLayout->addWidget(sbGain);
    mainLayout->addWidget(leGain);
    mainLayout->addWidget(new DividerLine(Qt::Vertical));
    mainLayout->addWidget(d->m_cbClipLanguage);
    mainLayout->addWidget(new DividerLine(Qt::Vertical));
    mainLayout->addWidget(d->m_btnArrow);
    mainLayout->addWidget(d->m_btnBeam);
    mainLayout->addWidget(d->m_btnNotePencil);
    mainLayout->addWidget(d->m_btnNoteEraser);
    mainLayout->addWidget(new DividerLine(Qt::Vertical));
    mainLayout->addWidget(d->m_btnPitchAnchor);
    mainLayout->addWidget(d->m_btnPitchPencil);
    mainLayout->addWidget(d->m_btnPitchEraser);
    mainLayout->addWidget(d->m_btnFreezePitch);
    mainLayout->addSpacerItem(new QSpacerItem(20, 20, QSizePolicy::Expanding));
    mainLayout->addWidget(btnPhonemeView);
    mainLayout->addWidget(btnParamView);
    // mainLayout->addSpacerItem(new QSpacerItem(20, 20, QSizePolicy::Expanding));
    mainLayout->addWidget(new DividerLine(Qt::Vertical));
    mainLayout->addWidget(btnMaximize);
    mainLayout->addWidget(btnHide);
    mainLayout->setContentsMargins(6, 6, 6, 6);
    mainLayout->setSpacing(6);
    setLayout(mainLayout);
    setFixedHeight(d->m_contentHeight + 12);

    d->moveToNullClipState();
}

void ClipEditorToolBarView::setDataContext(Clip *clip) {
    Q_D(ClipEditorToolBarView);
    if (d->m_clip)
        disconnect(d->m_clip, nullptr, d, nullptr);

    d->m_clip = clip;
    if (clip == nullptr) {
        d->moveToNullClipState();
        return;
    }
    connect(d->m_clip, &Clip::propertyChanged, d,
            &ClipEditorToolBarViewPrivate::onClipPropertyChanged);
    if (clip->clipType() == Clip::Singing) {
        d->m_singingClip = reinterpret_cast<SingingClip *>(clip);
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

void ClipEditorToolBarViewPrivate::onPianoRollToolButtonToggled(QAbstractButton *button,
                                                                bool checked) {
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

void ClipEditorToolBarViewPrivate::onClipNameEdited() const {
    Clip::ClipCommonProperties args(*m_clip);
    args.name = m_leClipName->text();
    int trackIndex;
    appModel->findClipById(m_clip->id(), trackIndex);
    trackController->onClipPropertyChanged(args);
}

void ClipEditorToolBarViewPrivate::onClipPropertyChanged() const {
    m_leClipName->setText(m_clip->name());
}

void ClipEditorToolBarViewPrivate::onClipLanguageChanged(AppGlobal::LanguageType language) const {
    m_cbClipLanguage->setCurrentIndex(language);
}

void ClipEditorToolBarViewPrivate::onLanguageEdited(int index) const {
    if (m_singingClip)
        m_singingClip->defaultLanguage = static_cast<AppGlobal::LanguageType>(index);
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
                                                      const QString &tipTitle,
                                                      const QKeySequence &shortcut,
                                                      const QString &tipDesc) const {
    const auto btn = buildCommonButton(objName, tipTitle, shortcut, tipDesc);
    btn->setCheckable(true);
    return btn;
}

Button *ClipEditorToolBarViewPrivate::buildCommonButton(const QString &objName,
                                                        const QString &tipTitle,
                                                        const QKeySequence &shortcut,
                                                        const QString &tipDesc) const {
    const auto btn = new Button;
    btn->setObjectName(objName);
    btn->setFixedSize(m_contentHeight, m_contentHeight);
    btn->setShortcut(shortcut);
    if (SystemUtils::productType() == SystemUtils::SystemProductType::Windows) {
        btn->setToolTip(tipTitle);
        const auto toolTip = new ToolTipFilter(btn, 500, false, true);
        if (!tipDesc.isEmpty())
            toolTip->appendMessage(tipDesc);
        btn->installEventFilter(toolTip);
    } else {
        QString shortcutStr;
        QString descStr;
        if (!shortcut.isEmpty())
            shortcutStr = " (" + shortcut.toString() + ")";
        if (!tipDesc.isEmpty())
            descStr = "\n" + tipDesc;
        btn->setToolTip(tipTitle + shortcutStr + descStr);
    }
    return btn;
}

void ClipEditorToolBarViewPrivate::setPianoRollToolsEnabled(bool on) const {
    m_cbClipLanguage->setVisible(on);
    m_btnArrow->setVisible(on);
    m_btnNotePencil->setVisible(on);
    m_btnPitchPencil->setVisible(on);
    m_btnPitchAnchor->setVisible(on);

    m_cbClipLanguage->setEnabled(on);
    m_btnArrow->setEnabled(on);
    m_btnNotePencil->setEnabled(on);
    m_btnPitchPencil->setEnabled(on);
    m_btnPitchAnchor->setEnabled(on);

    if (on) {
        auto lang = m_singingClip->defaultLanguage;
        m_cbClipLanguage->setCurrentIndex(lang);
        connect(m_cbClipLanguage, &ComboBox::currentIndexChanged, this,
                &ClipEditorToolBarViewPrivate::onLanguageEdited);
        connect(m_singingClip, &SingingClip::defaultLanguageChanged, m_cbClipLanguage,
                &ComboBox::setCurrentIndex);
    } else {
        disconnect(m_cbClipLanguage, &ComboBox::currentIndexChanged, this,
                   &ClipEditorToolBarViewPrivate::onLanguageEdited);
        m_cbClipLanguage->setCurrentIndex(AppGlobal::unknown);
    }
}