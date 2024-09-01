//
// Created by fluty on 2024/2/10.
//

#include "ClipEditorToolBarView.h"
#include "ClipEditorToolBarView_p.h"

#include "Controller/TracksViewController.h"
#include "Model/AppModel/AppModel.h"
#include "Model/AppModel/Clip.h"
#include "UI/Controls/Button.h"
#include "UI/Controls/DividerLine.h"
#include "UI/Controls/EditLabel.h"
#include "UI/Controls/LineEdit.h"
#include "UI/Controls/ToolTipFilter.h"
#include "UI/Views/Common/LanguageComboBox.h"

#include <QButtonGroup>
#include <QHBoxLayout>

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

    d->m_btnArrow = new Button;
    d->m_btnArrow->setObjectName("btnArrow");
    d->m_btnArrow->setCheckable(true);
    d->m_btnArrow->setFixedSize(d->m_contentHeight, d->m_contentHeight);
    d->m_btnArrow->setChecked(true);
    d->m_btnArrow->setShortcut(Qt::Key_V);
    d->m_btnArrow->setToolTip(tr("Select"));
    auto btnArrowToolTip = new ToolTipFilter(d->m_btnArrow, 500, false, true);
    // btnArrowToolTip->appendMessage(tr("Drag to select notes"));
    d->m_btnArrow->installEventFilter(btnArrowToolTip);

    d->m_btnBeam = new Button;
    d->m_btnBeam->setObjectName("btnBeam");
    d->m_btnBeam->setCheckable(true);
    d->m_btnBeam->setFixedSize(d->m_contentHeight, d->m_contentHeight);
    d->m_btnBeam->setShortcut(Qt::Key_B);
    d->m_btnBeam->setToolTip(tr("Interval Select"));
    auto btnBeamToolTip = new ToolTipFilter(d->m_btnBeam, 500, false, true);
    d->m_btnBeam->installEventFilter(btnBeamToolTip);

    d->m_btnNotePencil = new Button;
    d->m_btnNotePencil->setObjectName("btnNotePencil");
    d->m_btnNotePencil->setCheckable(true);
    d->m_btnNotePencil->setFixedSize(d->m_contentHeight, d->m_contentHeight);
    d->m_btnNotePencil->setShortcut(Qt::Key_N);
    d->m_btnNotePencil->setToolTip(tr("Draw Note"));
    auto btnNotePencilToolTip = new ToolTipFilter(d->m_btnNotePencil, 500, false, true);
    btnNotePencilToolTip->appendMessage(
        tr("Drag in the blank to draw a note,\nor drag on a note to edit it"));
    d->m_btnNotePencil->installEventFilter(btnNotePencilToolTip);

    d->m_btnNoteEraser = new Button;
    d->m_btnNoteEraser->setObjectName("btnNoteEraser");
    d->m_btnNoteEraser->setCheckable(true);
    d->m_btnNoteEraser->setFixedSize(d->m_contentHeight, d->m_contentHeight);
    d->m_btnNoteEraser->setShortcut(Qt::Key_M);
    d->m_btnNoteEraser->setToolTip(tr("Erase Note"));
    auto btnNoteEraserToolTip = new ToolTipFilter(d->m_btnNoteEraser, 500, false, true);
    d->m_btnNoteEraser->installEventFilter(btnNoteEraserToolTip);

    d->m_btnPitchAnchor = new Button;
    d->m_btnPitchAnchor->setObjectName("btnPitchAnchor");
    d->m_btnPitchAnchor->setCheckable(true);
    d->m_btnPitchAnchor->setFixedSize(d->m_contentHeight, d->m_contentHeight);
    d->m_btnPitchAnchor->setToolTip(tr("Pitch Anchor"));
    d->m_btnPitchAnchor->installEventFilter(
        new ToolTipFilter(d->m_btnPitchAnchor, 500, false, true));

    d->m_btnPitchPencil = new Button;
    d->m_btnPitchPencil->setObjectName("btnPitchPencil");
    d->m_btnPitchPencil->setCheckable(true);
    d->m_btnPitchPencil->setFixedSize(d->m_contentHeight, d->m_contentHeight);
    d->m_btnPitchPencil->setToolTip(tr("Draw Pitch"));
    auto btnPitchPencilToolTip = new ToolTipFilter(d->m_btnPitchPencil, 500, false, true);
    btnPitchPencilToolTip->appendMessage(
        tr("Left drag to edit pitch curves,\nor right drag to erase"));
    d->m_btnPitchPencil->installEventFilter(btnPitchPencilToolTip);

    d->m_btnPitchEraser = new Button;
    d->m_btnPitchEraser->setObjectName("btnPitchEraser");
    d->m_btnPitchEraser->setCheckable(true);
    d->m_btnPitchEraser->setFixedSize(d->m_contentHeight, d->m_contentHeight);
    d->m_btnPitchEraser->setToolTip(tr("Erase Pitch"));
    auto btnPitchEraserToolTip = new ToolTipFilter(d->m_btnPitchEraser, 500, false, true);
    // btnPitchEraserToolTip->appendMessage(tr("Drag to erase pitch curves"));
    d->m_btnPitchEraser->installEventFilter(btnPitchEraserToolTip);

    auto buttonGroup = new QButtonGroup;
    buttonGroup->setExclusive(true);
    buttonGroup->addButton(d->m_btnArrow);
    buttonGroup->addButton(d->m_btnBeam);
    buttonGroup->addButton(d->m_btnNotePencil);
    buttonGroup->addButton(d->m_btnNoteEraser);
    buttonGroup->addButton(d->m_btnPitchAnchor);
    buttonGroup->addButton(d->m_btnPitchPencil);
    buttonGroup->addButton(d->m_btnPitchEraser);
    connect(buttonGroup, &QButtonGroup::buttonToggled, d,
            &ClipEditorToolBarViewPrivate::onPianoRollToolButtonToggled);

    auto mainLayout = new QHBoxLayout;
    mainLayout->addWidget(d->m_leClipName);
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
    mainLayout->addSpacerItem(new QSpacerItem(20, 20, QSizePolicy::Expanding));
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
    } else
        qFatal() << "Unhandled button clicked";

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