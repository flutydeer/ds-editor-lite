//
// Created by fluty on 2024/2/10.
//

#include "ClipEditorToolBarView.h"
#include "ClipEditorToolBarView_p.h"

#include "Controller/TracksViewController.h"
#include "Model/AppModel/AppModel.h"
#include "Model/AppModel/Clip.h"
#include "UI/Controls/Button.h"
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
    d->m_leClipName->setFixedWidth(128);
    d->m_leClipName->setFixedHeight(d->m_contentHeight);
    d->m_leClipName->setEnabled(false);
    d->m_leClipName->setText("");
    connect(d->m_leClipName, &QLineEdit::editingFinished, d,
            &ClipEditorToolBarViewPrivate::onClipNameEdited);

    d->m_btnArrow = new Button;
    d->m_btnArrow->setObjectName("btnArrow");
    d->m_btnArrow->setCheckable(true);
    d->m_btnArrow->setFixedSize(d->m_contentHeight, d->m_contentHeight);
    d->m_btnArrow->setChecked(true);
    d->m_btnArrow->setToolTip(tr("Select"));
    d->m_btnArrow->installEventFilter(new ToolTipFilter(d->m_btnArrow, 500, false, true));

    d->m_btnNotePencil = new Button;
    d->m_btnNotePencil->setObjectName("btnNotePencil");
    d->m_btnNotePencil->setCheckable(true);
    d->m_btnNotePencil->setFixedSize(d->m_contentHeight, d->m_contentHeight);
    d->m_btnNotePencil->setToolTip(tr("Draw Note"));
    d->m_btnNotePencil->installEventFilter(new ToolTipFilter(d->m_btnNotePencil, 500, false, true));

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
    d->m_btnPitchPencil->installEventFilter(
        new ToolTipFilter(d->m_btnPitchPencil, 500, false, true));

    auto buttonGroup = new QButtonGroup;
    buttonGroup->setExclusive(true);
    buttonGroup->addButton(d->m_btnArrow);
    buttonGroup->addButton(d->m_btnNotePencil);
    buttonGroup->addButton(d->m_btnPitchAnchor);
    buttonGroup->addButton(d->m_btnPitchPencil);
    connect(buttonGroup, &QButtonGroup::buttonToggled, d,
            &ClipEditorToolBarViewPrivate::onPianoRollToolButtonToggled);

    d->m_cbLanguage = new LanguageComboBox(languageKeyFromType(AppGlobal::Unknown));

    auto mainLayout = new QHBoxLayout;
    mainLayout->addWidget(d->m_leClipName);
    mainLayout->addSpacing(6);
    mainLayout->addWidget(d->m_btnArrow);
    mainLayout->addWidget(d->m_btnNotePencil);
    mainLayout->addWidget(d->m_btnPitchAnchor);
    mainLayout->addWidget(d->m_btnPitchPencil);
    mainLayout->setSpacing(6);
    mainLayout->setContentsMargins(6, 6, 6, 6);
    mainLayout->addSpacerItem(new QSpacerItem(20, 20, QSizePolicy::Expanding));
    mainLayout->addWidget(d->m_cbLanguage);
    setLayout(mainLayout);
    setFixedHeight(d->m_contentHeight + 12);

    d->moveToNullClipState();
}
void ClipEditorToolBarView::setDataContext(Clip *clip) {
    Q_D(ClipEditorToolBarView);
    if (d->m_clip)
        disconnect(d->m_clip, nullptr, d, nullptr);
    if (d->m_singingClip)
        disconnect(d->m_singingClip, nullptr, d, nullptr);

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
    } else if (clip->clipType() == Clip::Audio)
        d->moveToAudioClipState();
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
        emit q->editModeChanged(Select);
    } else if (button == m_btnNotePencil) {
        m_editMode = DrawNote;
        emit q->editModeChanged(DrawNote);
    } else if (button == m_btnPitchPencil) {
        m_editMode = DrawPitch;
        emit q->editModeChanged(DrawPitch);
    } else if (button == m_btnPitchAnchor) {
        m_editMode = EditPitchAnchor;
        emit q->editModeChanged(EditPitchAnchor);
    }
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
    m_cbLanguage->setCurrentIndex(language);
}
void ClipEditorToolBarViewPrivate::onLanguageEdited(int index) {
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
    m_btnArrow->setVisible(on);
    m_btnNotePencil->setVisible(on);
    m_btnPitchPencil->setVisible(on);
    m_btnPitchAnchor->setVisible(on);
    m_btnArrow->setEnabled(on);
    m_btnNotePencil->setEnabled(on);
    m_btnPitchPencil->setEnabled(on);
    m_btnPitchAnchor->setEnabled(on);

    m_cbLanguage->setEnabled(on);
    if (on) {
        auto lang = m_singingClip->defaultLanguage();
        m_cbLanguage->setCurrentIndex(lang);
        connect(m_cbLanguage, &ComboBox::currentIndexChanged, this,
                &ClipEditorToolBarViewPrivate::onLanguageEdited);
    } else {
        disconnect(m_cbLanguage, &ComboBox::currentIndexChanged, this,
                   &ClipEditorToolBarViewPrivate::onLanguageEdited);
        m_cbLanguage->setCurrentIndex(AppGlobal::Unknown);
    }
}